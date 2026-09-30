#include "kvstore/kvstore.h"

// Transaction helpers reserved for future stricter semantics.
// V1 uses simple allocate / commit / abort rollback on IO failure.

namespace kvstore {

Status KvStore::reserve(const KvKey&, uint32_t size, std::vector<KvExtent>& extents) {
  if (!open_) {
    return Status::INVALID_ARGUMENT;
  }
  return allocator_->allocate(blocks_for_size(size), extents);
}

Status KvStore::commit(const KvKey& key, const KvRecord& record) {
  if (!open_) {
    return Status::INVALID_ARGUMENT;
  }
  KvRecord existing;
  Status st = index_->lookup(key, existing);
  if (st == Status::NOT_FOUND) {
    return index_->insert(key, record);
  }
  if (!ok(st)) {
    return st;
  }
  return index_->update(key, record);
}

Status KvStore::abort(const std::vector<KvExtent>& extents) {
  if (!open_) {
    return Status::INVALID_ARGUMENT;
  }
  return allocator_->free(extents);
}

}  // namespace kvstore
