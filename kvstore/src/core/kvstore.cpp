#include "kvstore/kvstore.h"
#include "kvstore/log.h"
#include "kvstore/record.h"

#include <algorithm>
#include <cstring>

namespace kvstore {

KvStore::KvStore() = default;

KvStore::~KvStore() {
  (void)close();
}

uint32_t KvStore::blocks_for_size(uint32_t size) const {
  const uint32_t bsz = config_.physical_block_size;
  return (size + bsz - 1) / bsz;
}

Status KvStore::open(const KvStoreConfig& config) {
  if (open_) {
    return Status::OK;
  }
  if (config.device_path.empty() || config.capacity_bytes == 0 ||
      config.physical_block_size == 0) {
    return Status::INVALID_ARGUMENT;
  }

  config_ = config;

  const uint64_t total_blocks =
      config_.capacity_bytes / config_.physical_block_size;
  if (total_blocks <= config_.data_start_lba) {
    return Status::INVALID_ARGUMENT;
  }
  const uint64_t data_blocks = total_blocks - config_.data_start_lba;

  index_ = std::make_unique<HashIndex>();
  allocator_ = std::make_unique<FixedBlockAllocator>(config_.data_start_lba, data_blocks);

  IoEngineConfig ioc;
  ioc.path = config_.device_path;
  ioc.size_bytes = config_.capacity_bytes;
  ioc.file_backed = config_.file_backed;
  ioc.o_direct = !config_.file_backed;

#if KVSTORE_HAS_IO_URING
  if (config_.use_io_uring) {
    io_ = create_io_uring_engine(ioc);
  } else {
    io_ = create_file_io_engine(ioc);
  }
#else
  if (config_.use_io_uring) {
    KV_LOG_WARN("kvstore", "io_uring requested but not available; using FileIoEngine");
  }
  io_ = create_file_io_engine(ioc);
#endif

  Status st = io_->open();
  if (!ok(st)) {
    io_.reset();
    allocator_.reset();
    index_.reset();
    return st;
  }

  open_ = true;
  KV_LOG_INFO("kvstore",
              "opened path=%s capacity=%llu data_start_lba=%llu data_blocks=%llu file_backed=%d",
              config_.device_path.c_str(),
              static_cast<unsigned long long>(config_.capacity_bytes),
              static_cast<unsigned long long>(config_.data_start_lba),
              static_cast<unsigned long long>(data_blocks),
              config_.file_backed ? 1 : 0);
  return Status::OK;
}

Status KvStore::close() {
  if (!open_) {
    return Status::OK;
  }

  {
    std::lock_guard lock(request_mu_);
    live_requests_.clear();
  }

  if (io_) {
    (void)io_->close();
    io_.reset();
  }
  allocator_.reset();
  index_.reset();
  open_ = false;
  // V1: process restart invalidates existing KV namespace (no durable metadata).
  return Status::OK;
}

Status KvStore::prepare_put(KvRequest* req) {
  const uint32_t blocks = blocks_for_size(req->value_.size);
  if (blocks == 0) {
    return Status::INVALID_ARGUMENT;
  }

  KvRecord existing;
  Status lst = index_->lookup(req->key_, existing);
  if (ok(lst)) {
    req->replace_existing_ = true;
    req->old_extents_to_free_.assign(existing.extents, existing.extents + existing.extent_count);
  } else if (lst != Status::NOT_FOUND) {
    return lst;
  }

  Status ast = allocator_->allocate(blocks, req->allocated_extents_);
  if (!ok(ast)) {
    return ast;
  }

  KvRecord rec{};
  rec.key = req->key_;
  rec.generation = next_generation_.fetch_add(1);
  rec.size = req->value_.size;
  rec.extent_count = static_cast<uint32_t>(req->allocated_extents_.size());
  if (rec.extent_count > kMaxExtentsPerRecord) {
    (void)allocator_->free(req->allocated_extents_);
    req->allocated_extents_.clear();
    return Status::INTERNAL_ERROR;
  }
  for (uint32_t i = 0; i < rec.extent_count; ++i) {
    rec.extents[i] = req->allocated_extents_[i];
  }
  rec.checksum = crc32(req->value_.data, req->value_.size);
  req->pending_record_ = rec;

  // Copy into block-aligned staging so last partial block is zero-padded.
  const size_t io_bytes =
      static_cast<size_t>(blocks) * static_cast<size_t>(config_.physical_block_size);
  req->staging_.assign(io_bytes, 0);
  std::memcpy(req->staging_.data(), req->value_.data, req->value_.size);

  uint8_t* cursor = req->staging_.data();
  for (const auto& ext : req->allocated_extents_) {
    auto io = std::make_unique<IoRequest>();
    io->lba = ext.lba;
    io->blocks = ext.blocks;
    io->buffer = cursor;
    io->write = true;
    io->request_id = next_request_id_.fetch_add(1);
    cursor += static_cast<size_t>(ext.blocks) * config_.physical_block_size;
    req->io_requests_.push_back(std::move(io));
  }

  req->state_.store(KvRequestState::PREPARED);
  return Status::OK;
}

Status KvStore::prepare_get(KvRequest* req) {
  KvRecord rec;
  Status st = index_->lookup(req->key_, rec);
  if (st == Status::NOT_FOUND) {
    metrics_.kv_get_miss.fetch_add(1);
    return Status::NOT_FOUND;
  }
  if (!ok(st)) {
    return st;
  }
  metrics_.kv_get_hit.fetch_add(1);

  if (req->value_.data == nullptr || req->value_.size < rec.size) {
    return Status::INVALID_ARGUMENT;
  }
  req->value_.size = rec.size;
  req->pending_record_ = rec;

  const size_t io_bytes =
      static_cast<size_t>(rec.total_blocks()) *
      static_cast<size_t>(config_.physical_block_size);
  req->staging_.assign(io_bytes, 0);

  uint8_t* cursor = req->staging_.data();
  for (uint32_t i = 0; i < rec.extent_count; ++i) {
    const auto& ext = rec.extents[i];
    auto io = std::make_unique<IoRequest>();
    io->lba = ext.lba;
    io->blocks = ext.blocks;
    io->buffer = cursor;
    io->write = false;
    io->request_id = next_request_id_.fetch_add(1);
    cursor += static_cast<size_t>(ext.blocks) * config_.physical_block_size;
    req->io_requests_.push_back(std::move(io));
  }

  req->state_.store(KvRequestState::PREPARED);
  return Status::OK;
}

Status KvStore::commit_put(KvRequest* req) {
  Status st;
  if (req->replace_existing_) {
    st = index_->update(req->key_, req->pending_record_);
    if (st == Status::NOT_FOUND) {
      st = index_->insert(req->key_, req->pending_record_);
    }
  } else {
    st = index_->insert(req->key_, req->pending_record_);
    if (st == Status::ALREADY_EXISTS) {
      st = index_->update(req->key_, req->pending_record_);
    }
  }
  if (!ok(st)) {
    return st;
  }

  if (!req->old_extents_to_free_.empty()) {
    (void)allocator_->free(req->old_extents_to_free_);
    req->old_extents_to_free_.clear();
  }
  return Status::OK;
}

Status KvStore::rollback_put(KvRequest* req) {
  if (!req->allocated_extents_.empty()) {
    (void)allocator_->free(req->allocated_extents_);
    req->allocated_extents_.clear();
  }
  return Status::OK;
}

Status KvStore::put_async(const KvKey& key, const KvValue& value, KvRequest*& request) {
  request = nullptr;
  if (!open_ || !value.valid()) {
    return Status::INVALID_ARGUMENT;
  }

  metrics_.kv_put_total.fetch_add(1);

  auto req = std::make_unique<KvRequest>(this, next_request_id_.fetch_add(1), KvOp::PUT);
  req->key_ = key;
  req->value_ = value;

  Status st = prepare_put(req.get());
  if (!ok(st)) {
    return st;
  }

  st = req->submit();
  if (!ok(st)) {
    return st;
  }

  request = req.get();
  std::lock_guard lock(request_mu_);
  live_requests_[request->id()] = std::move(req);
  return Status::OK;
}

Status KvStore::get_async(const KvKey& key, KvValue& value, KvRequest*& request) {
  request = nullptr;
  if (!open_ || value.data == nullptr || value.size == 0) {
    return Status::INVALID_ARGUMENT;
  }

  metrics_.kv_get_total.fetch_add(1);

  auto req = std::make_unique<KvRequest>(this, next_request_id_.fetch_add(1), KvOp::GET);
  req->key_ = key;
  req->value_ = value;

  Status st = prepare_get(req.get());
  if (!ok(st)) {
    return st;
  }

  // Reflect actual logical size back to caller.
  value.size = req->value_.size;

  st = req->submit();
  if (!ok(st)) {
    return st;
  }

  request = req.get();
  std::lock_guard lock(request_mu_);
  live_requests_[request->id()] = std::move(req);
  return Status::OK;
}

Status KvStore::erase(const KvKey& key) {
  if (!open_) {
    return Status::INVALID_ARGUMENT;
  }

  metrics_.kv_delete_total.fetch_add(1);

  KvRecord rec;
  Status st = index_->lookup(key, rec);
  if (!ok(st)) {
    return st;
  }

  st = index_->erase(key);
  if (!ok(st)) {
    return st;
  }

  std::vector<KvExtent> extents(rec.extents, rec.extents + rec.extent_count);
  return allocator_->free(extents);
}

KvRequestState KvStore::query(KvRequest* request) {
  if (request == nullptr) {
    return KvRequestState::FAILED;
  }
  return request->query();
}

Status KvStore::wait(KvRequest* request) {
  if (request == nullptr) {
    return Status::INVALID_ARGUMENT;
  }
  return request->wait();
}

Status KvStore::release(KvRequest* request) {
  if (request == nullptr) {
    return Status::INVALID_ARGUMENT;
  }
  std::lock_guard lock(request_mu_);
  live_requests_.erase(request->id());
  return Status::OK;
}

}  // namespace kvstore
