# EasyTier-PeerDiscovery — Engineering Design

Detailed design for the standalone discovery service. High-level intent and the
tech stack summary live in `../AGENTS.md`; this file carries the frozen
protocol rules and implementation guidance.

## Core idea

- **Architecture B**: a standalone `discovery` program runs the real DHT;
  EasyTier itself is untouched.
- Injection: EasyTier natively accepts `-p http://<discovery>/endpoint`
  (Manual Discovery). On startup a node asks "who should I connect to", this
  program returns one currently-online member (`tcp://ip:port` /
  `quic://ip:port`); the node joins and the rest of the mesh is spread by
  EasyTier's internal OSPF / `SyncRouteInfo`.
- DHT is only **entry/member address discovery**, not a trusted identity
  directory; identity and data-plane connectivity are fully left to EasyTier's
  existing stack (`network_secret` auth as the anti-poisoning backstop).

## infohash derivation (FROZEN, do not change)

Bucketed per protocol; the same `network_name` + `network_secret` yields the
same topic network-wide:

```
K        = KDF(secret, salt="easytier:dht:v1")              # 32-byte output
digest   = SHA-256(K || network_name || scheme)             # scheme is a frozen string
infohash = digest[0..20]                                    # first 20 bytes → DHT topic
```

- `KDF`: **mandatory slow hash** (resist offline brute force on a low-entropy
  secret after DHT exposure). Fixed as **Argon2id** (RFC 9106 SECOND
  RECOMMENDED: `m=64 MiB, t=3, p=1`, 32-byte output), provided by `libargon2`.
- `--secret-hash` accepts `K` as exactly 64 hexadecimal characters, allowing
  Argon2id to be run ahead of time by `et-secret-hash`. `network_name` remains
  a separate mandatory input, so precomputation does not change the frozen
  derivation or DHT compatibility.
- Two default namespaces:
  - `scheme="tcp"` → TCP endpoints
  - `scheme="udp"` → UDP endpoints
- Optional `scheme="quic"` namespace, enabled with `--quic`.
- A multi-protocol node announces to each corresponding infohash.
- Any change (salt / KDF and params / concat order / scheme / truncation)
  invalidates the whole network; keep it fixed.

## HTTP discovery interface (toward EasyTier)

Contract for EasyTier `-p http://<host>:<port>/endpoint`:

- **Request**: `GET /endpoint`, header `X-Network-Name: <network_name>`
  (attached automatically by EasyTier's `CoreManualEndpointResolver`, see
  `easytier-core/src/connectivity/manual/discovery/implementation.rs:236`).
- **Response**: HTTP `200`, body **one connector URL per line**
  (`tcp://ip:port`, `udp://ip:port`, or optional `quic://ip:port`). EasyTier
  picks one valid URL at random (`resolve_http_body`); blank lines and multiple
  lines are allowed.
- EasyTier requests this endpoint only on **initial connect and reconnect**
  (not periodic polling); returning a currently-online member is enough,
  no long-lived connections needed.

## Restraint policy (respect the public DHT)

- **No announce by default**: `dht_search(infohash, port=0, af, cb)` only queries.
- **On-demand announce** (`dht_search(..., port!=0, ...)`) only when:
  1. every enabled protocol and address-family search in this round has
     completed without returning a peer;
  2. uptime is at least 30 seconds and `easytier-cli --output json node`
      reports the information required for that protocol and address family.
      IPv4 TCP uses the TCP NAT type and TCP listener; IPv4 UDP uses the UDP
      NAT type and UDP listener. Eligible NAT types are `Public`,
      `FriendlyNAT`, `NoPAT`, `OpenInternet`, and `FullCone`. Optional IPv4
      QUIC uses UDP NAT eligibility and its QUIC listener. IPv6 TCP/UDP require
      a reported public IPv6 address and their respective listeners, but do not
      use NAT status. `--assume-good-ipv4-nat` overrides both IPv4 NAT checks.
- Nodes without an eligible IPv4 NAT path or public IPv6 address do **not**
  announce that protocol/address-family combination; they only query and join
  other public peers as clients, relying on internal OSPF to spread.
- The announced port is the **EasyTier listener port**, not this program's own
  DHT UDP port. Mainline DHT derives the address from the IPv4 or IPv6 request
  source. Public IP / NAT type / listen port eligibility comes from
  `easytier-cli --output json node` (RPC portal). JSON parsing reads
  `stun_info` and `listeners`, ignores unrelated fields (including the
  secret-bearing `config` string), and rejects malformed or oversized output.
  Raw NAT enum names are preserved. Missing or unknown IPv4 NAT types remain
  ineligible unless explicitly overridden.
- Announce interval defaults to 15 minutes (keeps the entry refreshed at least
  twice within Mainline's ~30-min–2-hour reaping window).

## DHT state persistence

The DHT node ID and up to 256 good IPv4 plus 256 good IPv6 routing nodes are
stored in `dht-state.bin`. The state is loaded at startup, saved every five
minutes, and saved again during orderly shutdown. Discovered EasyTier peers are
not persisted because BEP5 peer records may become stale. At most 32 saved
nodes per address family are pinged during startup to avoid a bootstrap burst.

Default state directories:

- Linux and other POSIX systems: `$XDG_STATE_HOME/easytier-peerdiscovery` when
  `XDG_STATE_HOME` is an absolute path; otherwise
  `~/.local/state/easytier-peerdiscovery`.
- Win32: `FOLDERID_LocalAppDataLow\easytier-peerdiscovery`, resolved with the
  Windows Known Folder API.

`--dht-state-path <directory>` overrides this default; `dht-state.bin` is
stored directly in the supplied directory. Relative paths use the process
working directory. Paths are literal (no environment-variable or tilde
expansion by the program).

Writes use a temporary file followed by an atomic replacement. On POSIX, the
state file mode is restricted to `0600` and newly created directories use
`0700`.

## Directory layout

```
3rdparty/            # git submodules (read-only, do not modify)
  dht/               # transmission/dht lib (dht.c + dht.h)
  uthash/            # uthash header-only lib
src/
  main.c             # entry: init, event loop (select + dht_periodic), HTTP server
  dht_runtime.c/h    # dht_init / dht_periodic driver, dual-stack sockets, bootstrap
  dht_state.c/h      # persistent node ID and routing-node state
  infohash.c/h       # infohash derivation (KDF + SHA-256)
  discovery.c/h      # periodic dht_search + on-demand announce decision + callback
  httpd.c/h          # local HTTP `/endpoint` service
  endpoint.c/h       # "active members" table + URL generation for EasyTier
  easytier_rpc.c/h   # easytier-cli node query (public IP / NAT / listen port)
include/
build/               # gitignore
CMakeLists.txt
```

## Functions the dht lib requires from us

`dht.c` needs these implemented by the caller (declared in
`3rdparty/dht/dht.h`):

- `dht_sendto()`: low-level UDP send (`sendto`).
- `dht_blacklisted()`: address blacklist check; return 0 for now (no restriction).
- `dht_hash()`: **strong hash** (use OpenSSL SHA-1, not the `crypt` in the
  example), used for tokens.
- `dht_random_bytes()`: secure randomness (`/dev/urandom` or `RAND_bytes`).
