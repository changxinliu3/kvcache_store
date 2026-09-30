#pragma once

#include "kvstore/record.h"
#include "kvstore/status.h"

#include <cstdint>
#include <mutex>
#include <vector>

namespace kvstore {

class BlockAllocator {
 public:
  virtual ~BlockAllocator() = default;

  virtual Status allocate(uint32_t blocks, std::vector<KvExtent>& extents) = 0;
  virtual Status free(const std::vector<KvExtent>& extents) = 0;
  virtual uint64_t free_blocks() const = 0;
  virtual uint64_t total_blocks() const = 0;
};

// Simple fixed-block free-list allocator over [data_start_lba, data_start_lba + total).
// Metadata is entirely in memory (V1: no persistence / crash recovery).
class FixedBlockAllocator : public BlockAllocator {
 public:
  FixedBlockAllocator(uint64_t data_start_lba, uint64_t total_blocks);

  Status allocate(uint32_t blocks, std::vector<KvExtent>& extents) override;
  Status free(const std::vector<KvExtent>& extents) override;
  uint64_t free_blocks() const override;
  uint64_t total_blocks() const override;

 private:
  struct FreeRange {
    uint64_t lba;
    uint64_t blocks;
  };

  void coalesce_unlocked();

  mutable std::mutex mu_;
  uint64_t data_start_lba_;
  uint64_t total_blocks_;
  uint64_t free_blocks_;
  std::vector<FreeRange> free_list_;
};

}  // namespace kvstore
