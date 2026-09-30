#pragma once

#include "kvstore/io_engine.h"
#include "kvstore/request.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace kvstore {
namespace nixl_adapter {

// Maps one NIXL transfer to one KvRequest (+ underlying IoRequests).
// When integrating real NIXL, inherit nixlBackendReqH and adapt fields.
class KvstoreReq {
 public:
  uint64_t request_id{0};
  KvRequest* kv_request{nullptr};
  std::vector<IoRequest*> io_requests;
  std::atomic<uint32_t> pending{0};
  std::atomic<bool> failed{false};
  uint64_t submit_timestamp{0};
  uint64_t complete_timestamp{0};

  bool completed() const {
    return kv_request != nullptr &&
           kv_request->state() == KvRequestState::COMPLETED;
  }

  bool failed_state() const {
    return failed.load() ||
           (kv_request != nullptr && kv_request->state() == KvRequestState::FAILED);
  }
};

}  // namespace nixl_adapter
}  // namespace kvstore
