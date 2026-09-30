#!/usr/bin/env python3
"""Run KVStore V1 benchmarks and print a summary table."""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path("/mnt/d/work/kvcachestore")
BIN = ROOT / "build-wsl" / "bin"


def run(cmd: list[str]) -> str:
    print("+", " ".join(cmd), flush=True)
    p = subprocess.run(cmd, cwd=str(ROOT), capture_output=True, text=True)
    out = (p.stdout or "") + (p.stderr or "")
    print(out, end="" if out.endswith("\n") else "\n", flush=True)
    if p.returncode != 0:
        raise SystemExit(f"command failed: {cmd} rc={p.returncode}\n{out}")
    return out


def main() -> int:
    if not (BIN / "bench_kvstore").exists():
        subprocess.check_call([sys.executable, str(ROOT / "kvstore/scripts/build_wsl.py")])

    print("\n==== bench_io ====", flush=True)
    run([str(BIN / "bench_io")])

    # Spec §38: 1K/10K/100K and block sizes 16..256 KiB (scale down 100K*256KiB for file-backed time)
    cases = [
        (1000, 16 * 1024),
        (1000, 32 * 1024),
        (1000, 64 * 1024),
        (1000, 128 * 1024),
        (1000, 256 * 1024),
        (10000, 16 * 1024),
        (10000, 64 * 1024),
        (10000, 128 * 1024),
        (100000, 16 * 1024),
    ]

    print("\n==== bench_kvstore matrix ====", flush=True)
    for n, sz in cases:
        run([str(BIN / "bench_kvstore"), str(n), str(sz)])

    print("\nBENCHMARK_DONE", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
