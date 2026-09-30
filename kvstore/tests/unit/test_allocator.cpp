#include "kvstore/allocator.h"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>

using namespace kvstore;

int main() {
  FixedBlockAllocator alloc(/*data_start_lba=*/1024, /*total_blocks=*/1024);

  assert(alloc.total_blocks() == 1024);
  assert(alloc.free_blocks() == 1024);

  std::vector<KvExtent> e1;
  assert(alloc.allocate(32, e1) == Status::OK);
  assert(!e1.empty());
  assert(alloc.free_blocks() == 1024 - 32);

  std::vector<KvExtent> e2;
  assert(alloc.allocate(64, e2) == Status::OK);
  assert(alloc.free(e1) == Status::OK);
  assert(alloc.free_blocks() == 1024 - 64);

  // Reuse freed range.
  std::vector<KvExtent> e3;
  assert(alloc.allocate(32, e3) == Status::OK);
  assert(e3[0].lba == e1[0].lba);

  // Out of space
  std::vector<KvExtent> big;
  assert(alloc.allocate(100000, big) == Status::NO_SPACE);

  // Concurrent allocation
  FixedBlockAllocator concurrent(0, 10000);
  std::vector<std::thread> threads;
  std::atomic<int> ok_count{0};
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&] {
      for (int i = 0; i < 100; ++i) {
        std::vector<KvExtent> ex;
        if (concurrent.allocate(4, ex) == Status::OK) {
          ok_count.fetch_add(1);
        }
      }
    });
  }
  for (auto& th : threads) {
    th.join();
  }
  assert(ok_count.load() > 0);
  assert(concurrent.free_blocks() == 10000 - static_cast<uint64_t>(ok_count.load()) * 4);

  std::printf("[PASS] Allocator\n");
  return 0;
}
