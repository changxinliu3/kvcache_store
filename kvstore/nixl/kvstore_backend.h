#pragma once

// Skeleton only. Replace includes/signatures with the active NIXL checkout ABI.
//
// Expected dependency direction:
//   NIXL -> KvstoreBackend -> IKvStore / KvStore
//
// KvstoreBackend must NOT own HashIndex / Allocator / IoEngine.

#include "kvstore/kvstore.h"

#include <cstdint>
#include <memory>
#include <string>

namespace kvstore {
namespace nixl_adapter {

// Opaque stand-ins so this tree compiles without NIXL headers.
// When wiring a real NIXL tree, delete these and inherit nixlBackendEngine.
struct NixlBackendInitParamsStub {
  std::string device_path;
  uint64_t capacity_bytes{0};
};

class KvstoreBackend {
 public:
  explicit KvstoreBackend(const NixlBackendInitParamsStub& params);
  ~KvstoreBackend();

  bool supportsLocal() const { return true; }
  bool supportsRemote() const { return false; }
  bool supportsNotif() const { return false; }

  Status open();
  Status close();

  IKvStore* store() { return store_.get(); }

 private:
  NixlBackendInitParamsStub params_;
  std::unique_ptr<KvStore> store_;
};

}  // namespace nixl_adapter
}  // namespace kvstore
