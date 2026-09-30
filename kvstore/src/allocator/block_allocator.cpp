#include "kvstore/allocator.h"

#include <algorithm>

namespace kvstore {

FixedBlockAllocator::FixedBlockAllocator(uint64_t data_start_lba, uint64_t total_blocks)
    : data_start_lba_(data_start_lba),
      total_blocks_(total_blocks),
      free_blocks_(total_blocks) {
  if (total_blocks_ > 0) {
    free_list_.push_back(FreeRange{data_start_lba_, total_blocks_});
  }
}

Status FixedBlockAllocator::allocate(uint32_t blocks, std::vector<KvExtent>& extents) {
  extents.clear();
  if (blocks == 0) {
    return Status::INVALID_ARGUMENT;
  }

  std::lock_guard lock(mu_);
  if (blocks > free_blocks_) {
    return Status::NO_SPACE;
  }

  // V1: first-fit contiguous allocation. Spill into multiple extents if needed.
  uint32_t remaining = blocks;
  for (auto it = free_list_.begin(); it != free_list_.end() && remaining > 0;) {
    const uint32_t take = static_cast<uint32_t>(std::min<uint64_t>(it->blocks, remaining));
    if (extents.size() >= kMaxExtentsPerRecord) {
      // Roll back partial allocation within this call.
      for (const auto& e : extents) {
        free_list_.push_back(FreeRange{e.lba, e.blocks});
        free_blocks_ += e.blocks;
      }
      extents.clear();
      coalesce_unlocked();
      return Status::NO_SPACE;
    }

    extents.push_back(KvExtent{it->lba, take});
    free_blocks_ -= take;
    remaining -= take;

    if (take == it->blocks) {
      it = free_list_.erase(it);
    } else {
      it->lba += take;
      it->blocks -= take;
      ++it;
    }
  }

  if (remaining != 0) {
    for (const auto& e : extents) {
      free_list_.push_back(FreeRange{e.lba, e.blocks});
      free_blocks_ += e.blocks;
    }
    extents.clear();
    coalesce_unlocked();
    return Status::NO_SPACE;
  }

  return Status::OK;
}

Status FixedBlockAllocator::free(const std::vector<KvExtent>& extents) {
  std::lock_guard lock(mu_);
  for (const auto& e : extents) {
    if (e.blocks == 0) {
      continue;
    }
    if (e.lba < data_start_lba_ ||
        e.lba + e.blocks > data_start_lba_ + total_blocks_) {
      return Status::INVALID_ARGUMENT;
    }
    free_list_.push_back(FreeRange{e.lba, e.blocks});
    free_blocks_ += e.blocks;
  }
  coalesce_unlocked();
  return Status::OK;
}

uint64_t FixedBlockAllocator::free_blocks() const {
  std::lock_guard lock(mu_);
  return free_blocks_;
}

uint64_t FixedBlockAllocator::total_blocks() const {
  return total_blocks_;
}

void FixedBlockAllocator::coalesce_unlocked() {
  if (free_list_.empty()) {
    return;
  }
  std::sort(free_list_.begin(), free_list_.end(),
            [](const FreeRange& a, const FreeRange& b) { return a.lba < b.lba; });

  std::vector<FreeRange> merged;
  merged.reserve(free_list_.size());
  merged.push_back(free_list_.front());
  for (size_t i = 1; i < free_list_.size(); ++i) {
    auto& last = merged.back();
    const auto& cur = free_list_[i];
    if (last.lba + last.blocks == cur.lba) {
      last.blocks += cur.blocks;
    } else {
      merged.push_back(cur);
    }
  }
  free_list_.swap(merged);
}

}  // namespace kvstore
