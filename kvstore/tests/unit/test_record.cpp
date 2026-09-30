#include "kvstore/record.h"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace kvstore;

int main() {
  const char* msg = "kvstore-v1";
  uint32_t c1 = crc32(msg, std::strlen(msg));
  uint32_t c2 = crc32(msg, std::strlen(msg));
  assert(c1 == c2);
  assert(c1 != 0);

  KvRecord r{};
  r.extent_count = 2;
  r.extents[0] = {10, 4};
  r.extents[1] = {20, 8};
  assert(r.total_blocks() == 12);

  std::printf("[PASS] Record\n");
  return 0;
}
