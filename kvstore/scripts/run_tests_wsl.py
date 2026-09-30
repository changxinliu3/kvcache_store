#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

BIN = Path("/mnt/d/work/kvcachestore/build-wsl/bin")
tests = [
    "test_hash_index",
    "test_allocator",
    "test_record",
    "test_request",
    "test_io",
    "test_kvstore",
    "test_nixl_md",
]

for name in tests:
    print(f"=== {name} ===", flush=True)
    subprocess.check_call([str(BIN / name)])

print("ALL_PASS", flush=True)
