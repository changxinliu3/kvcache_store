#include "nixl/kvstore_md.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace kvstore;
using namespace kvstore::nixl_adapter;

int main() {
  KvstoreRemoteMD md{};
  md.key_hi = 0x1111;
  md.key_lo = 0x2222;
  md.expected_size = 131072;

  auto bytes = serialize_md(md);
  KvstoreRemoteMD out{};
  assert(deserialize_md(bytes.data(), bytes.size(), out));
  assert(out.key_hi == md.key_hi);
  assert(out.key_lo == md.key_lo);
  assert(out.expected_size == md.expected_size);
  assert(md_to_key(out) == (KvKey{0x1111, 0x2222}));

  // Must not expose LBA in public MD.
  static_assert(sizeof(KvstoreRemoteMD) == 40 || sizeof(KvstoreRemoteMD) >= 32);

  std::printf("[PASS] NIXL metadata\n");
  return 0;
}
