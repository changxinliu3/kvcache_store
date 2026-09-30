#pragma once

#include <cstddef>
#include <cstdint>

namespace kvstore {

// Caller-owned buffer describing a KV payload.
struct KvValue {
  void* data{nullptr};
  uint32_t size{0};

  bool valid() const { return data != nullptr && size > 0; }
};

}  // namespace kvstore
