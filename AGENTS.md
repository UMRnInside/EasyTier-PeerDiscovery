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
    --http 127.0.0.1:11008 --dht-port 0
easytier-core -n <net> -k <secret> -p http://127.0.0.1:11008/endpoint
```

### Native Windows (MinGW-w64)

Use the MSYS2 **UCRT64** terminal, not the MSYS compiler. The resulting
executables use Winsock and native Windows process APIs and do not require
`msys-2.0.dll`. OpenSSL and Argon2 must come from the same native toolchain.

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,pkgconf,openssl,argon2}
git submodule update --init --recursive
cmake -S . -B build-win32 -G Ninja
cmake --build build-win32
ctest --test-dir build-win32 --output-on-failure
./build-win32/easytier-peerdisc.exe --network-name <net> --secret <secret> \
    --easytier-cli-path 'C:\Program Files\EasyTier\easytier-cli.exe'
```

For Linux cross-compilation, use `cmake/mingw-w64.cmake`. Set
`CMAKE_FIND_ROOT_PATH` to include the Windows dependency installation prefix,
`OPENSSL_ROOT_DIR` to that prefix, and `PKG_CONFIG_LIBDIR` to its pkg-config
directory. Never let pkg-config select host Linux libraries. Set
`CMAKE_CROSSCOMPILING_EMULATOR=wine` to run the portable CTest suites with Wine;
initialize the Wine prefix before running tests.

Windows subprocesses are launched directly without a shell, with explicit
standard-handle inheritance and stderr redirected to `NUL`. Socket handles
retain their native width; the unmodified DHT library receives integer tokens
mapped back to IPv4/IPv6 sockets in `dht_sendto`. DHT random bytes use OpenSSL
`RAND_bytes` on every platform.

This does not change the existing Windows state-storage limitations: custom
state directories need existing parents, and narrow path inputs use the system
code page. Full Unicode paths and Windows console-close handling remain separate
work. The POSIX-only path suite is supplemented by portable platform tests for
subprocess arguments/status, IPv4/IPv6 DHT sends, HTTP/event-loop sockets, and RNG.
