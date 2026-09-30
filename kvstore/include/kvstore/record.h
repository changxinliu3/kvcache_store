#pragma once

#include "kvstore/key.h"

#include <cstdint>

namespace kvstore {

inline constexpr uint32_t kPhysicalBlockSize = 4096;  // 4 KiB
inline constexpr uint32_t kDefaultKvBlockSize = 131072;  // 128 KiB
inline constexpr uint32_t kMaxExtentsPerRecord = 4;

struct KvExtent {
  uint64_t lba{0};     // SSD logical block address (in physical blocks)
  uint32_t blocks{0};  // number of 4 KiB physical blocks
};

struct KvRecord {
  KvKey key{};
  uint64_t generation{0};
  uint32_t size{0};
  uint32_t extent_count{0};
  KvExtent extents[kMaxExtentsPerRecord]{};
  uint32_t checksum{0};
  uint32_t flags{0};

  uint64_t total_blocks() const {
    uint64_t n = 0;
    for (uint32_t i = 0; i < extent_count; ++i) {
      n += extents[i].blocks;
    }
    return n;
  }
};

uint32_t crc32(const void* data, size_t len);

}  // namespace kvstore
