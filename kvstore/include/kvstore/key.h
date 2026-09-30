#pragma once

#include <cstdint>
#include <functional>

namespace kvstore {

// V1 internal key: fixed 128-bit hash. No string keys.
struct KvKey {
  uint64_t hi{0};
  uint64_t lo{0};

  constexpr bool operator==(const KvKey& other) const {
    return hi == other.hi && lo == other.lo;
  }

  constexpr bool operator!=(const KvKey& other) const { return !(*this == other); }
};

struct KvKeyHash {
  size_t operator()(const KvKey& key) const noexcept {
    // SplitMix64 style mix of hi/lo.
    uint64_t x = key.hi ^ (key.lo + 0x9e3779b97f4a7c15ULL + (key.hi << 6) + (key.hi >> 2));
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return static_cast<size_t>(x);
  }
};

}  // namespace kvstore
