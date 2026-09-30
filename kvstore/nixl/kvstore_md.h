#pragma once

#include "kvstore/key.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace kvstore {
namespace nixl_adapter {

inline constexpr uint64_t kKvstoreMdMagic = 0x4B5653544D443031ULL;  // KVSTMD01
inline constexpr uint32_t kKvstoreMdVersion = 1;

// NIXL descriptor payload: exposes KV key, NEVER raw LBA.
struct KvstoreRemoteMD {
  uint64_t magic{kKvstoreMdMagic};
  uint32_t version{kKvstoreMdVersion};
  uint32_t flags{0};
  uint64_t key_hi{0};
  uint64_t key_lo{0};
  uint32_t expected_size{0};
  uint32_t reserved{0};
};

inline KvKey md_to_key(const KvstoreRemoteMD& md) {
  return KvKey{md.key_hi, md.key_lo};
}

inline std::vector<uint8_t> serialize_md(const KvstoreRemoteMD& md) {
  std::vector<uint8_t> out(sizeof(KvstoreRemoteMD));
  std::memcpy(out.data(), &md, sizeof(md));
  return out;
}

inline bool deserialize_md(const void* data, size_t len, KvstoreRemoteMD& md) {
  if (data == nullptr || len < sizeof(KvstoreRemoteMD)) {
    return false;
  }
  std::memcpy(&md, data, sizeof(md));
  return md.magic == kKvstoreMdMagic && md.version == kKvstoreMdVersion;
}

}  // namespace nixl_adapter
}  // namespace kvstore
