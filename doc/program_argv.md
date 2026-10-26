# easytier-peerdisc — Command-Line Arguments

The standalone discovery service is driven entirely by CLI arguments. Options
use `getopt_long` style (`--key value`); short aliases are provided where
natural. Frozen protocol rules such as infohash derivation and the HTTP
contract are **not** configurable from the CLI.

## Core (required)

| Option | Type | Default | Description |
|---|---|---|---|
| `-n, --network-name <name>` | string | required | Network name, same as `easytier-core -n`; part of infohash derivation. |
| `-k, --secret <secret>` | string | conditional | Network secret, same as `easytier-core -k`; input to the KDF. Mutually exclusive with `--secret-hash`. |
| `--secret-hash <hex>` | 64 hex chars | conditional | Precomputed 32-byte Argon2id result from `et-secret-hash`. Mutually exclusive with `--secret`. |

When either option is omitted, `ET_NETWORK_NAME` and `ET_NETWORK_SECRET` are
used respectively. Explicit command-line options take precedence over the
environment.

Exactly one of `--secret`, `ET_NETWORK_SECRET`, or `--secret-hash` supplies the
secret credential. Generate a reusable hash without changing the DHT protocol:

```sh
HASH=$(et-secret-hash --network-name mynet --secret mysecret)
./easytier-peerdisc --network-name mynet --secret-hash "$HASH"
```

`--network-name` is optional for `et-secret-hash` and is accepted as context;
the frozen protocol applies Argon2id to the secret alone. The network name is
still required when starting `easytier-peerdisc` because it participates in the
subsequent SHA-256 topic derivation.

## HTTP `/endpoint` service

| Option | Type | Default | Description |
|---|---|---|---|
| `--http <host:port>` | string | `127.0.0.1:11008` | Local `GET /endpoint` listen address, consumed by EasyTier `-p http://...`. |

## DHT runtime

| Option | Type | Default | Description |
|---|---|---|---|
| `--dht-port <port>` | int | `0` (random) | DHT UDP listen port. |
| `--dht-state-path <directory>` | path | platform state directory | Store `dht-state.bin` directly in this directory; create it if needed. Overrides XDG/HOME or the Windows Known Folder default. |
| `--bootstrap <host:port>` | string | four built-in nodes | Repeatable; add a custom bootstrap node. The built-ins are Transmission, BitTorrent, µTorrent, and libtorrent. |
| `--announce-interval <sec>` | int | `900` | Announce interval in seconds; minimum `300`, rejected below that. |
| `--no-announce` | flag | off | Force query-only, override automatic announce decisions. |
| `--ipv6` / `--no-ipv6` | flag | on | Dual-stack toggle (BEP32). |
| `--quic` | flag | off | Also discover and announce the optional QUIC namespace. |

## External endpoint source (easytier-cli RPC)

| Option | Type | Default | Description |
|---|---|---|---|
| `--rpc <host:port>` | string | `127.0.0.1:15888` | `easytier-cli` rpc portal; queried for public IP / NAT type / listen port. |
| `--easytier-cli-path <path-to-easytier-cli>` | path | `easytier-cli` from `PATH` | Execute this exact file instead of searching `PATH`. Absolute and working-directory-relative paths are supported. No shell expansion or command arguments are accepted. |

## Generic

| Option | Type | Default | Description |
|---|---|---|---|
| `-v, --verbose` | flag | off | Log every Mainline DHT peer result, protocol/address-family search completion, and DHT state load/save outcome (path and saved node counts on success). |
| `-h, --help` | flag | — | Usage. |
| `--version` | flag | — | Version number. |

## Examples

```sh
# Minimal: required options only
./easytier-peerdisc -n mynet -k mysecret

# Custom HTTP port + custom bootstrap + query-only
./easytier-peerdisc -n mynet -k mysecret --http 0.0.0.0:8080 \
    --bootstrap dht.transmissionbt.com:6881 --no-announce

# Fully custom
./easytier-peerdisc -n mynet -k mysecret --dht-port 33445 \
    --announce-interval 600 --rpc 127.0.0.1:15888

# Paths containing spaces must be quoted in the invoking shell
./easytier-peerdisc -n mynet -k mysecret \
    --easytier-cli-path '/opt/Easy Tier/easytier-cli' \
    --dht-state-path "$HOME/custom state/peerdisc"
```

## Design notes

- Defaults align with upstream: `--rpc 127.0.0.1:15888` matches
  `easytier-cli`'s default rpc portal; `--http 127.0.0.1:11008` follows the
  `AGENTS.md` example.
- Frozen rules are baked in, not exposed: infohash KDF parameters are fixed;
  `--announce-interval` enforces the `900` default / `300` minimum.
- Restraint is controllable: `--no-announce` keeps poor-NAT nodes stable
  clients; `--bootstrap` may be given multiple times.
