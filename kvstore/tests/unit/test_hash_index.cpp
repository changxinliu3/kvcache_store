#include "kvstore/index.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace kvstore;

static KvRecord make_record(KvKey key, uint32_t size, uint64_t lba) {
  KvRecord r;
  r.key = key;
  r.size = size;
  r.generation = 1;
  r.extent_count = 1;
  r.extents[0] = KvExtent{lba, (size + 4095) / 4096};
  return r;
}

int main() {
  HashIndex index;

  KvKey k1{1, 2};
  KvRecord out;

  assert(index.lookup(k1, out) == Status::NOT_FOUND);
  assert(index.insert(k1, make_record(k1, 4096, 100)) == Status::OK);
  assert(index.insert(k1, make_record(k1, 4096, 100)) == Status::ALREADY_EXISTS);
  assert(index.lookup(k1, out) == Status::OK);
  assert(out.extents[0].lba == 100);

  KvRecord updated = make_record(k1, 8192, 200);
  updated.generation = 2;
  assert(index.update(k1, updated) == Status::OK);
  assert(index.lookup(k1, out) == Status::OK);
  assert(out.extents[0].lba == 200);
  assert(out.generation == 2);

  assert(index.erase(k1) == Status::OK);
  assert(index.erase(k1) == Status::NOT_FOUND);
  assert(index.size() == 0);

  // Scale: 100K keys
  constexpr size_t N = 100000;
  for (size_t i = 0; i < N; ++i) {
    KvKey k{i, i * 7};
    assert(index.insert(k, make_record(k, 4096, i + 1)) == Status::OK);
  }
  assert(index.size() == N);
  for (size_t i = 0; i < N; i += 97) {
    KvKey k{i, i * 7};
    assert(index.lookup(k, out) == Status::OK);
    assert(out.extents[0].lba == i + 1);
  }

  std::printf("[PASS] HashIndex\n");
  return 0;
}
