#!/bin/bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
mkdir -p build-wsl/obj build-wsl/bin

compile() {
  local f="$1"
  local base
  base="$(basename "$f" .cpp)"
  local obj="build-wsl/obj/${base}.o"
  echo "CXX $f"
  g++ -std=c++20 -O2 -Wall -Wextra -Wno-unused-parameter \
    -Ikvstore/include -DKVSTORE_HAS_IO_URING=0 -pthread \
    -c "$f" -o "$obj"
  echo "$obj"
}

OBJS=()
while IFS= read -r -d '' f; do
  case "$f" in
    *io_uring_engine.cpp) continue ;;
  esac
  ObjsPath="$(compile "$f")"
  OBJS+=("$ObjsPath")
done < <(find kvstore/src -name '*.cpp' -print0)

ar rcs build-wsl/libkvstore.a "${OBJS[@]}"

link_one() {
  local name="$1"
  local src="$2"
  local extra_inc="${3:-}"
  echo "LINK $name"
  # shellcheck disable=SC2086
  g++ -std=c++20 -O2 -Ikvstore/include $extra_inc -DKVSTORE_HAS_IO_URING=0 -pthread \
    "$src" build-wsl/libkvstore.a -o "build-wsl/bin/$name"
}

link_one test_hash_index kvstore/tests/unit/test_hash_index.cpp
link_one test_allocator kvstore/tests/unit/test_allocator.cpp
link_one test_record kvstore/tests/unit/test_record.cpp
link_one test_request kvstore/tests/unit/test_request.cpp
link_one test_kvstore kvstore/tests/integration/test_kvstore.cpp
link_one test_io kvstore/tests/integration/test_io.cpp
link_one test_nixl_md kvstore/tests/nixl/test_nixl_backend.cpp "-Ikvstore"
link_one kv_put_get kvstore/examples/kv_put_get.cpp
link_one kv_async kvstore/examples/kv_async.cpp
link_one bench_kvstore kvstore/benchmark/bench_kvstore.cpp
link_one bench_io kvstore/benchmark/bench_io.cpp

echo BUILD_OK
