#include "kvstore_backend.h"
#include "kvstore/log.h"

namespace kvstore {
namespace nixl_adapter {

KvstoreBackend::KvstoreBackend(const NixlBackendInitParamsStub& params)
    : params_(params) {}

KvstoreBackend::~KvstoreBackend() { (void)close(); }

Status KvstoreBackend::open() {
  store_ = std::make_unique<KvStore>();
  KvStoreConfig cfg;
  cfg.device_path = params_.device_path;
  cfg.capacity_bytes = params_.capacity_bytes;
  cfg.file_backed = true;
  Status st = store_->open(cfg);
  if (!ok(st)) {
    store_.reset();
    return st;
  }
  KV_LOG_INFO("nixl", "KvstoreBackend opened (skeleton, local-only)");
  return Status::OK;
}

Status KvstoreBackend::close() {
  if (store_) {
    (void)store_->close();
    store_.reset();
  }
  return Status::OK;
}

}  // namespace nixl_adapter
}  // namespace kvstore
