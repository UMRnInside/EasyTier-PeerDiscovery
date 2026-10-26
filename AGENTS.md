# EasyTier-PeerDiscovery

An EasyTier P2P mesh helper: lets peers join a network **without pre-specifying
any member node**. It peers into the BitTorrent Mainline DHT (BEP5 + BEP32
dual-stack) for decentralized member/entry discovery, then injects the found
nodes into EasyTier via its native `-p http://` Manual Discovery — **zero source
modification** to EasyTier.

> Design rationale lives in `EasyTier-DHT-Plan.md` at the repo root (or parent
> dir). Detailed engineering design is in [`doc/DESIGN.md`](doc/DESIGN.md).

## Tech stack

| Item | Choice | Notes |
|---|---|---|
| Language | C (C11) | zero external deps, pure C, easy to embed |
| DHT lib | `transmission/dht` (maintained fork of `jech/dht`) | `3rdparty/dht` submodule, dual-stack BEP32 |
| Hash table | `uthash` | `3rdparty/uthash` submodule |
| Slow hash | `libargon2` (Argon2id: m=64 MiB, t=3, p=1) | infohash derivation |
| Digest | OpenSSL EVP (SHA-256 / SHA-1) | infohash derivation and dht_hash |
| Test framework | `Unity` | `3rdparty/unity` submodule, unit tests in `tests/` |
| Build | CMake | see DESIGN.md |

### Dependency submodules

```sh
git submodule update --init --recursive
```

- `3rdparty/dht` → `https://github.com/transmission/dht` (`post-0.27-transmission` line)
- `3rdparty/uthash` → `https://github.com/troydhanson/uthash`

## Development conventions

If you are an LLM/agent:

1. Do not commit unless the user explicitly asks you to.
2. Never create Issues or Pull Requests on your own. If the user asks you to,
   prefix the title with three robot emoji.

- Pure C11, no third-party runtime deps (system OpenSSL only).
- **Write all comments, code identifiers, and commit messages in English.**
- **Write documentation in English too**, unless the user explicitly requests
  another language, or the file path marks it as non-English (e.g.
  `README.zh-CN.md` is Simplified Chinese).
- **No comments unless necessary**; prefer self-explanatory naming.
- Put new source in `src/` and register it in the build script; update the
  stack table in `AGENTS.md` when adding a dependency.
- Keep build steps in `AGENTS.md` under "Build & run"; do not move them into
  `doc/DESIGN.md`.
- Before changing any "frozen" rule (infohash derivation / HTTP contract /
  announce interval), confirm with the requester first. The full design is in
  `doc/DESIGN.md`.
- Run `cmake --build build` before committing to confirm it builds; never
  modify submodule content under `3rdparty/dht`.

## Build & run

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build
./build/easytier-peerdisc --network-name <net> --secret <secret> \
    --http 127.0.0.1:8080 --dht-port 0
easytier-core -n <net> -k <secret> -p http://127.0.0.1:8080/endpoint
```