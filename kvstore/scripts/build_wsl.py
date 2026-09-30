#!/usr/bin/env python3
"""Portable build helper when cmake is unavailable (WSL / Linux)."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
os.chdir(ROOT)

OBJ_DIR = ROOT / "build-wsl" / "obj"
BIN_DIR = ROOT / "build-wsl" / "bin"
LIB = ROOT / "build-wsl" / "libkvstore.a"
OBJ_DIR.mkdir(parents=True, exist_ok=True)
BIN_DIR.mkdir(parents=True, exist_ok=True)

CXX = ["g++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Wno-unused-parameter",
       "-Ikvstore/include", "-DKVSTORE_HAS_IO_URING=0", "-pthread"]


def run(cmd: list[str]) -> None:
    print("+", " ".join(cmd), flush=True)
    subprocess.check_call(cmd)


def main() -> int:
    srcs = sorted((ROOT / "kvstore" / "src").rglob("*.cpp"))
    objs: list[Path] = []
    for src in srcs:
        if src.name == "io_uring_engine.cpp":
            continue
        obj = OBJ_DIR / (src.stem + ".o")
        run(CXX + ["-c", str(src), "-o", str(obj)])
        objs.append(obj)

    run(["ar", "rcs", str(LIB), *map(str, objs)])

    targets = [
        ("test_hash_index", "kvstore/tests/unit/test_hash_index.cpp", []),
        ("test_allocator", "kvstore/tests/unit/test_allocator.cpp", []),
        ("test_record", "kvstore/tests/unit/test_record.cpp", []),
        ("test_request", "kvstore/tests/unit/test_request.cpp", []),
        ("test_kvstore", "kvstore/tests/integration/test_kvstore.cpp", []),
        ("test_io", "kvstore/tests/integration/test_io.cpp", []),
        ("test_nixl_md", "kvstore/tests/nixl/test_nixl_backend.cpp", ["-Ikvstore"]),
        ("kv_put_get", "kvstore/examples/kv_put_get.cpp", []),
        ("kv_async", "kvstore/examples/kv_async.cpp", []),
        ("bench_kvstore", "kvstore/benchmark/bench_kvstore.cpp", []),
        ("bench_io", "kvstore/benchmark/bench_io.cpp", []),
    ]

    for name, src, extra in targets:
        out = BIN_DIR / name
        run(CXX + extra + [src, str(LIB), "-o", str(out)])

    print("BUILD_OK", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
