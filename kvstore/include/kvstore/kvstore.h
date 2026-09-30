#pragma once

#include "kvstore/allocator.h"
#include "kvstore/index.h"
#include "kvstore/io_engine.h"
#include "kvstore/key.h"
#include "kvstore/request.h"
#include "kvstore/status.h"
#include "kvstore/value.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace kvstore {

struct KvStoreConfig {
  std::string device_path;          // NVMe device or file path (test)
  bool file_backed{true};           // file-backed test mode
  bool use_io_uring{false};         // Linux production path
  uint64_t capacity_bytes{0};       // total usable SSD/file capacity
  uint64_t data_start_lba{1024};    // reserve low LBAs for future metadata/journal
  uint32_t physical_block_size{kPhysicalBlockSize};
  uint32_t kv_block_size{kDefaultKvBlockSize};
};

class IKvStore {
 public:
  virtual ~IKvStore() = default;

  virtual Status open(const KvStoreConfig& config) = 0;
  virtual Status close() = 0;

  virtual Status put_async(const KvKey& key, const KvValue& value, KvRequest*& request) = 0;
  virtual Status get_async(const KvKey& key, KvValue& value, KvRequest*& request) = 0;
  virtual Status erase(const KvKey& key) = 0;

  virtual KvRequestState query(KvRequest* request) = 0;
  virtual Status wait(KvRequest* request) = 0;
  virtual Status release(KvRequest* request) = 0;
};

// V1 metrics counters (in-memory only).
struct KvStoreMetrics {
  std::atomic<uint64_t> kv_put_total{0};
  std::atomic<uint64_t> kv_get_total{0};
  std::atomic<uint64_t> kv_delete_total{0};
  std::atomic<uint64_t> kv_get_hit{0};
  std::atomic<uint64_t> kv_get_miss{0};
  std::atomic<uint64_t> kv_bytes_read{0};
  std::atomic<uint64_t> kv_bytes_written{0};
  std::atomic<uint64_t> kv_io_errors{0};
};

class KvStore : public IKvStore {
 public:
  KvStore();
  ~KvStore() override;

  Status open(const KvStoreConfig& config) override;
  Status close() override;

  Status put_async(const KvKey& key, const KvValue& value, KvRequest*& request) override;
  Status get_async(const KvKey& key, KvValue& value, KvRequest*& request) override;
  Status erase(const KvKey& key) override;

  KvRequestState query(KvRequest* request) override;
  Status wait(KvRequest* request) override;
  Status release(KvRequest* request) override;

  // Reserved for future journal/recovery path.
  Status reserve(const KvKey& key, uint32_t size, std::vector<KvExtent>& extents);
  Status commit(const KvKey& key, const KvRecord& record);
  Status abort(const std::vector<KvExtent>& extents);

  const KvStoreMetrics& metrics() const { return metrics_; }
  uint32_t physical_block_size() const { return config_.physical_block_size; }
  bool is_open() const { return open_; }

 private:
  friend class KvRequest;

  Status prepare_put(KvRequest* req);
  Status prepare_get(KvRequest* req);
  Status commit_put(KvRequest* req);
  Status rollback_put(KvRequest* req);

  uint32_t blocks_for_size(uint32_t size) const;

  KvStoreConfig config_{};
  bool open_{false};

  std::unique_ptr<HashIndex> index_;
  std::unique_ptr<FixedBlockAllocator> allocator_;
  std::unique_ptr<IoEngine> io_;

  std::atomic<uint64_t> next_request_id_{1};
  std::atomic<uint64_t> next_generation_{1};

  mutable std::mutex request_mu_;
  std::unordered_map<uint64_t, std::unique_ptr<KvRequest>> live_requests_;

  KvStoreMetrics metrics_{};
};

}  // namespace kvstore
