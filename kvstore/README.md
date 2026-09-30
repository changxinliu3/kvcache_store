# KVStore V1

KV-aware local NVMe SSD storage engine for LLM inference KV cache offload/restore.

## Design

```text
KV key -> HashIndex -> KvRecord -> KvExtent(LBA) -> IoEngine -> NVMe
```

- Not a POSIX filesystem; no pathname/inode path on the production data path
- Metadata is memory-only in V1 (process restart invalidates the namespace)
- NIXL is an adapter only; Core owns Index / Allocator / IO

## Build

```bash
cmake -S kvstore -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Linux io_uring (optional):

```bash
cmake -S kvstore -B build -DKVSTORE_ENABLE_IO_URING=ON
# requires liburing
```

NIXL backend (optional, requires a real NIXL checkout):

```bash
cmake -S kvstore -B build -DKVSTORE_BUILD_NIXL=ON -DNIXL_ROOT=/path/to/nixl
```

## Quick example

```bash
./build/examples/kv_put_get
./build/benchmark/bench_kvstore 1000 131072
```

## Layout

See `include/kvstore/` for public headers and `src/` for Core implementation.
`nixl/` contains the backend adapter skeleton; wire ABI against your NIXL tree before production use.
