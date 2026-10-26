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
  (`tcp://ip:port` or `quic://ip:port`). EasyTier picks one valid URL at random
  (`resolve_http_body`); blank lines and multiple lines are allowed.
- EasyTier requests this endpoint only on **initial connect and reconnect**
  (not periodic polling); returning a currently-online member is enough,
  no long-lived connections needed.

## Restraint policy (respect the public DHT)

- **No announce by default**: `dht_search(infohash, port=0, af, cb)` only queries.
- **On-demand announce** (`dht_search(..., port!=0, ...)`) only when:
  1. every enabled protocol and address-family search in this round has
     completed without returning a peer;
  2. NAT is good (public / full-cone, via STUN) and uptime ≥ 5 minutes.
- LAN / poor-NAT nodes do **not** announce; they only query and join other
  public peers as clients, relying on internal OSPF to spread (avoid zombie
  entries).
- The announced content is the **EasyTier connectable endpoint**
  (`tcp://<publicIP>:<listenPort>`), **not** this program's own DHT UDP port.
  Public IP / NAT type / listen port come from `easytier-cli node` (RPC portal).
- Announce interval defaults to 15 minutes (keeps the entry refreshed at least
  twice within Mainline's ~30-min–2-hour reaping window).

## Directory layout

```
3rdparty/            # git submodules (read-only, do not modify)
  dht/               # transmission/dht lib (dht.c + dht.h)
  uthash/            # uthash header-only lib
src/
  main.c             # entry: init, event loop (select + dht_periodic), HTTP server
  dht_runtime.c/h    # dht_init / dht_periodic driver, dual-stack sockets, bootstrap
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
