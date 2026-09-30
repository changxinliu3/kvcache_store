#pragma once

#include "kvstore/key.h"
#include "kvstore/record.h"
#include "kvstore/status.h"
#include "kvstore/value.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace kvstore {

class KvStore;
struct IoRequest;
struct KvRecord;
struct KvExtent;

enum class KvRequestState {
  CREATED,
  PREPARED,
  SUBMITTED,
  IN_PROGRESS,
  COMPLETED,
  FAILED,
  CANCELLED,
};

enum class KvOp {
  PUT,
  GET,
  ERASE,
};

class KvRequest {
 public:
  KvRequest(KvStore* store, uint64_t id, KvOp op);
  ~KvRequest();

  KvRequest(const KvRequest&) = delete;
  KvRequest& operator=(const KvRequest&) = delete;

  uint64_t id() const { return id_; }
  KvOp op() const { return op_; }
  KvRequestState state() const { return state_.load(); }
  Status status() const { return status_.load(); }

  Status submit();
  KvRequestState query();
  Status wait();

 private:
  friend class KvStore;

  Status on_io_complete();
  void mark_failed(Status st);

  KvStore* store_;
  uint64_t id_;
  KvOp op_;
  std::atomic<KvRequestState> state_{KvRequestState::CREATED};
  std::atomic<Status> status_{Status::OK};

  KvKey key_{};
  KvValue value_{};
  KvRecord pending_record_{};
  std::vector<KvExtent> allocated_extents_;
  std::vector<std::unique_ptr<IoRequest>> io_requests_;
  std::vector<KvExtent> old_extents_to_free_;
  // Staging buffer: block-aligned copy for partial last block I/O.
  std::vector<uint8_t> staging_;
  bool replace_existing_{false};
  uint64_t submit_ns_{0};
  uint64_t complete_ns_{0};
};

}  // namespace kvstore
