#pragma once

#include "kvstore/key.h"
#include "kvstore/record.h"
#include "kvstore/status.h"

#include <cstddef>
#include <shared_mutex>
#include <unordered_map>

namespace kvstore {

class KvIndex {
 public:
  virtual ~KvIndex() = default;

  virtual Status lookup(const KvKey& key, KvRecord& record) = 0;
  virtual Status insert(const KvKey& key, const KvRecord& record) = 0;
  virtual Status update(const KvKey& key, const KvRecord& record) = 0;
  virtual Status erase(const KvKey& key) = 0;
  virtual size_t size() const = 0;
};

class HashIndex : public KvIndex {
 public:
  Status lookup(const KvKey& key, KvRecord& record) override;
  Status insert(const KvKey& key, const KvRecord& record) override;
  Status update(const KvKey& key, const KvRecord& record) override;
  Status erase(const KvKey& key) override;
  size_t size() const override;

 private:
  mutable std::shared_mutex mu_;
  std::unordered_map<KvKey, KvRecord, KvKeyHash> map_;
};

}  // namespace kvstore
