# TexitCoin Core - Codebase Overview

**TexitCoin Core v0.25.0** — a Bitcoin Core fork with its own blockchain, launched Jan 26, 2024.

Binaries: `texitcoind`, `texitcoin-qt`, `texitcoin-cli`, `texitcoin-tx`, `texitcoin-wallet`.
Default data directory: `~/.texitcoin`.

---

## Coin Parameters

| Parameter | Value |
|---|---|
| **PoW Algorithm** | SHA256d (double SHA-256) |
| **Block Time** | 3 minutes |
| **Max Supply** | ~353.4M TXC |
| **Genesis Reward** | 254 TXC |
| **Halving Interval** | 695,662 blocks |
| **Mainnet Port** | 15740 |
| **Testnet Port** | 25740 |
| **Bech32 Prefix** | `txc` (mainnet), `ttxc` (testnet) |
| **MWEB HRP** | `txcmweb` |
| **Base58 Prefixes** | PUBKEY: 66, SCRIPT: 5, SCRIPT2: 65, SECRET: 193 |
| **Genesis Message** | *"You may all go to hell and I will go to Texas"* |
| **Genesis Timestamp** | 1706236287 (Jan 26, 2024) |
| **Genesis Hash** | `0xb628195b74011675c216718bba39e04b631c1c82c060e8ee3e975ea87377b8ca` |
| **Required Coinbase Address** | `TdaxfTr1sBjoPNbZLSWpaXxSc6bxJsNyc2` |
| **DNS Seeds** | node1.texitcoin.org, node2.texitcoin.org, node10.texitcoin.org |

---

## Consensus & Proof-of-Work

### Difficulty Adjustment

- **Before height 275,520 (V1)**: Retargets every 4 blocks, 12-minute target timespan
- **After height 275,520 (V2)**: Retargets every 40 blocks, 2-hour target timespan
- Adjustment clamped to 4x per period to limit difficulty swings

### Merge-Mining (Auxpow)

- **Enabled at block 73,000** — blocks before this are legacy (non-merge-mined)
- **Chain ID V1**: `0x62` (98 decimal)
- **Chain ID V2**: `0x1b39` (6969 decimal) — activated at height 275,520
- Validates merge-mining via parent block proof-of-work and merkle path verification

### Consensus Activations

| Feature | Status |
|---|---|
| P2SH (BIP16) | Always active (height 0) |
| BIP34/65/66 | Always active (height 0) |
| CSV (BIP68/112/113) | Always active (height 0) |
| Segwit (BIP141/143/147) | Always active (height 0) |
| Taproot (BIP340-342) | Always active |
| MWEB (LIP-0002/0003/0004) | Disabled on mainnet |

### Hardcoded Checkpoints

| Height | Block Hash |
|---|---|
| 0 | `0xb628195b74011675c216718bba39e04b631c1c82c060e8ee3e975ea87377b8ca` |
| 73,000 | `0x1d5edc7fb63949849033f51b474479b671d73e2c2a13b8e0b2560006b9dcc716` |
| 267,500 | `0x4ec4346c8403a4a36dcab87fcf90711ceacedd33b32c5321eb98985a6ba2b10d` |
| 275,520 | `0x329ddcf8de1cc797338508069d49b4aa6729ec34b46a5c17c1944c02ec341b72` |

### Key Protocol Heights

| Height | Event |
|---|---|
| 73,000 | Auxpow (merge-mining) enabled |
| 267,500 | Protocol upgrade |
| 275,520 | Difficulty adjustment V2 + Chain ID V2 |
| 301,770 | Required coinbase address enforced |

---

## Key Customizations vs Bitcoin Core

1. **Merge-mining (Auxpow)** — SHA256d-based auxiliary proof-of-work with evolving chain IDs
2. **Difficulty adjustment fork** — V1 (4-block) to V2 (40-block) retargeting at height 275,520
3. **Required coinbase address** — enforced from height 301,770
4. **Omni Protocol** (`src/omnicore/`) — token/property layer for creating assets on-chain
5. **MWEB** (`src/mweb/`, `src/libmw/`) — Mimblewimble privacy extension (disabled on mainnet)
6. **secp256k1-zkp** — zero-knowledge proof variant of the EC library
7. **Custom `-texitkey` CLI option** — for specifying a custom key file

---

## Source Structure (`src/`)

| Area | Location | Purpose |
|---|---|---|
| **Consensus** | `consensus/`, `pow.cpp`, `auxpow.cpp` | Rules, difficulty, merge-mining |
| **Validation** | `validation.cpp` (~5800 lines) | Core blockchain validation engine |
| **Chain Params** | `chainparams.cpp` (~472 lines) | Network parameters, genesis block, checkpoints |
| **Networking** | `net.cpp`, `net_*.cpp`, `protocol.cpp` | P2P layer |
| **Wallet** | `wallet/` (30+ files) | Key management, coin selection, fees |
| **RPC** | `rpc/` (20+ files) | JSON-RPC interface |
| **Mining** | `miner.cpp` | Block template construction |
| **GUI** | `qt/` (140+ files) | Qt-based wallet UI |
| **Omni** | `omnicore/` (40+ files) | Token/property protocol |
| **MWEB** | `mweb/`, `libmw/` | Mimblewimble privacy layer |
| **Crypto** | `crypto/`, `secp256k1-zkp/` | SHA256, BLAKE3, EC signing, ZK proofs |
| **Storage** | `leveldb/`, `txdb.cpp` | Blockchain database (LevelDB-backed) |
| **Primitives** | `primitives/` | Block and transaction data structures |
| **Script** | `script/` (9+ files) | Script interpreter and signing |
| **HTTP/ZMQ** | `httprpc.cpp`, `httpserver.cpp`, `zmq/` | JSON-RPC over HTTP, ZeroMQ notifications |
| **Initialization** | `init.cpp` (~5800 lines) | Daemon startup, argument handling |
| **Utilities** | `util/` (15+ files) | System helpers, logging, threading |

---

## Key Files

| File | Purpose |
|---|---|
| `src/chainparams.cpp` | All network parameters, genesis block, checkpoints |
| `src/pow.cpp` | Difficulty adjustment algorithm |
| `src/auxpow.cpp` | Merge-mining validation |
| `src/validation.cpp` | Core blockchain validation |
| `src/init.cpp` | Daemon startup and CLI arguments |
| `src/consensus/params.h` | Consensus rule structures |
| `src/miner.cpp` | Block template construction |
| `src/amount.h` | Monetary unit definitions (COIN = 100,000,000) |
| `src/clientversion.cpp` | Client name: `TexitCoinCore` |
| `src/util/system.cpp` | Default data directory (`~/.texitcoin`) |
| `configure.ac` | Build configuration and version (0.25.0) |

---

## Build System

- **Autotools**: `configure.ac` + `Makefile.am` + `autogen.sh`
- **MSVC**: `build_msvc/` for Windows Visual Studio builds
- **Dependencies**: managed via `depends/` (30+ packages)

### Key Dependencies

| Package | Purpose |
|---|---|
| OpenSSL | Cryptography |
| Boost | Threading, system |
| Qt5 | GUI framework |
| BerkeleyDB | Legacy wallet support |
| LevelDB | Chainstate database |
| ZeroMQ | Message queue notifications |
| miniupnpc | UPnP NAT traversal |
| qrencode | QR code generation |

### Platforms

- **Linux** — primary (build-unix.md)
- **macOS** — supported (build-osx.md)
- **Windows** — supported via MSVC and cross-compilation (build-windows.md)
- **FreeBSD/NetBSD/OpenBSD** — community builds

---

## CI/CD

| Platform | Config | Notes |
|---|---|---|
| Travis CI | `.travis.yml` | Linux builds, lint + test stages |
| Cirrus CI | `.cirrus.yml` | Linux containers, 2 CPU / 8GB RAM, KVM |
| AppVeyor | `.appveyor.yml` | Windows builds, artifact upload |

CI pipeline: setup env -> install deps -> configure/make -> run tests -> deploy artifacts

---

## Tests

| Type | Location | Count | Framework |
|---|---|---|---|
| Unit tests | `src/test/` | 89+ files | C++ / Boost |
| Functional tests | `test/functional/` | 178 scripts | Python |
| Fuzz tests | `test/fuzz/` | Various | libFuzzer |
| Linting | `test/lint/` | Various | Shell / Python |

Run unit tests: `make check`
Run functional tests: `test/functional/test_runner.py`

---

## Networks

| Network | Port | Bech32 | Purpose |
|---|---|---|---|
| Mainnet | 15740 | `txc` | Production |
| Testnet | 25740 | `ttxc` | Testing |
| Regtest | — | `rltc` | Development / unit tests |