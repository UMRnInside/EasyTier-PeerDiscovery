# easytier-peerdisc — Command-Line Arguments

The standalone discovery service is driven entirely by CLI arguments. Options
use `getopt_long` style (`--key value`); short aliases are provided where
natural. Frozen protocol rules (infohash derivation, HTTP contract, announce
interval) are **not** configurable from the CLI.

## Core (required)

| Option | Type | Default | Description |
|---|---|---|---|
| `-n, --network-name <name>` | string | required | Network name, same as `easytier-core -n`; part of infohash derivation. |
| `-k, --secret <secret>` | string | required | Network secret, same as `easytier-core -k`; input to the KDF. |

## HTTP `/endpoint` service

| Option | Type | Default | Description |
|---|---|---|---|
| `--http <host:port>` | string | `127.0.0.1:8080` | Local `GET /endpoint` listen address, consumed by EasyTier `-p http://...`. |

## DHT runtime

| Option | Type | Default | Description |
|---|---|---|---|
| `--dht-port <port>` | int | `0` (random) | DHT UDP listen port. |
| `--bootstrap <host:port>` | string | four built-in nodes | Repeatable; add a custom bootstrap node. The built-ins are Transmission, BitTorrent, µTorrent, and libtorrent. |
| `--announce-interval <sec>` | int | `900` | Announce interval in seconds; minimum `300`, rejected below that. |
| `--no-announce` | flag | off | Force query-only, override automatic announce decisions. |
| `--ipv6` / `--no-ipv6` | flag | on | Dual-stack toggle (BEP32). |
| `--quic` | flag | off | Also discover and announce the optional QUIC namespace. |

## External endpoint source (easytier-cli RPC)

| Option | Type | Default | Description |
|---|---|---|---|
| `--rpc <host:port>` | string | `127.0.0.1:15888` | `easytier-cli` rpc portal; queried for public IP / NAT type / listen port. |

## Generic

| Option | Type | Default | Description |
|---|---|---|---|
| `-v, --verbose` | flag | off | Verbose logging. |
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
```

## Design notes

- Defaults align with upstream: `--rpc 127.0.0.1:15888` matches
  `easytier-cli`'s default rpc portal; `--http 127.0.0.1:8080` follows the
  `AGENTS.md` example.
- Frozen rules are baked in, not exposed: infohash KDF parameters are fixed;
  `--announce-interval` enforces the `900` default / `300` minimum.
- Restraint is controllable: `--no-announce` keeps poor-NAT nodes stable
  clients; `--bootstrap` may be given multiple times.
