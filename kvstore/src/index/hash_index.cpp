#include "kvstore/index.h"

#include <mutex>

namespace kvstore {

Status HashIndex::lookup(const KvKey& key, KvRecord& record) {
  std::shared_lock lock(mu_);
  auto it = map_.find(key);
  if (it == map_.end()) {
    return Status::NOT_FOUND;
  }
  record = it->second;
  return Status::OK;
}

Status HashIndex::insert(const KvKey& key, const KvRecord& record) {
  std::unique_lock lock(mu_);
  auto [it, inserted] = map_.emplace(key, record);
  if (!inserted) {
    return Status::ALREADY_EXISTS;
  }
  (void)it;
  return Status::OK;
}

Status HashIndex::update(const KvKey& key, const KvRecord& record) {
  std::unique_lock lock(mu_);
  auto it = map_.find(key);
  if (it == map_.end()) {
    return Status::NOT_FOUND;
  }
  it->second = record;
  return Status::OK;
}

Status HashIndex::erase(const KvKey& key) {
  std::unique_lock lock(mu_);
  auto n = map_.erase(key);
  return n > 0 ? Status::OK : Status::NOT_FOUND;
}

size_t HashIndex::size() const {
  std::shared_lock lock(mu_);
  return map_.size();
}

}  // namespace kvstore
