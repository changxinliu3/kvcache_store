#include "kvstore/request.h"
#include "kvstore/kvstore.h"
#include "kvstore/log.h"

#include <algorithm>
#include <chrono>
#include <cstring>

namespace kvstore {
namespace {

uint64_t now_ns() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

}  // namespace

KvRequest::KvRequest(KvStore* store, uint64_t id, KvOp op)
    : store_(store), id_(id), op_(op) {}

KvRequest::~KvRequest() {
  if (store_ != nullptr && store_->io_) {
    for (auto& io : io_requests_) {
      if (io) {
        (void)store_->io_->release(io.get());
      }
    }
  }
}

void KvRequest::mark_failed(Status st) {
  status_.store(st);
  state_.store(KvRequestState::FAILED);
  complete_ns_ = now_ns();
  if (store_ != nullptr) {
    store_->metrics_.kv_io_errors.fetch_add(1);
  }
}

Status KvRequest::submit() {
  if (state_.load() != KvRequestState::PREPARED &&
      state_.load() != KvRequestState::CREATED) {
    return Status::INVALID_ARGUMENT;
  }

  submit_ns_ = now_ns();
  state_.store(KvRequestState::SUBMITTED);

  for (auto& io : io_requests_) {
    Status st = store_->io_->submit(io.get());
    if (!ok(st)) {
      mark_failed(st);
      if (op_ == KvOp::PUT) {
        (void)store_->rollback_put(this);
      }
      return st;
    }
  }

  state_.store(KvRequestState::IN_PROGRESS);
  return Status::OK;
}

KvRequestState KvRequest::query() {
  const auto cur = state_.load();
  if (cur != KvRequestState::IN_PROGRESS && cur != KvRequestState::SUBMITTED) {
    return cur;
  }

  bool any_failed = false;
  bool any_progress = false;
  for (auto& io : io_requests_) {
    const IoState st = store_->io_->query(io.get());
    if (st == IoState::FAILED) {
      any_failed = true;
    } else if (st != IoState::COMPLETED) {
      any_progress = true;
    }
  }

  if (any_failed) {
    mark_failed(Status::IO_FAILED);
    if (op_ == KvOp::PUT) {
      (void)store_->rollback_put(this);
    }
    return KvRequestState::FAILED;
  }
  if (any_progress) {
    state_.store(KvRequestState::IN_PROGRESS);
    return KvRequestState::IN_PROGRESS;
  }

  Status st = on_io_complete();
  if (!ok(st)) {
    mark_failed(st);
    return KvRequestState::FAILED;
  }
  return state_.load();
}

Status KvRequest::wait() {
  while (true) {
    const auto st = query();
    if (st == KvRequestState::COMPLETED) {
      return Status::OK;
    }
    if (st == KvRequestState::FAILED || st == KvRequestState::CANCELLED) {
      return status_.load();
    }
    // Block on first incomplete IO.
    for (auto& io : io_requests_) {
      const IoState iost = store_->io_->query(io.get());
      if (iost != IoState::COMPLETED && iost != IoState::FAILED) {
        Status wst = store_->io_->wait(io.get());
        if (!ok(wst)) {
          mark_failed(wst);
          if (op_ == KvOp::PUT) {
            (void)store_->rollback_put(this);
          }
          return wst;
        }
      }
    }
  }
}

Status KvRequest::on_io_complete() {
  if (op_ == KvOp::PUT) {
    Status st = store_->commit_put(this);
    if (!ok(st)) {
      (void)store_->rollback_put(this);
      return st;
    }
    store_->metrics_.kv_bytes_written.fetch_add(value_.size);
  } else if (op_ == KvOp::GET) {
    if (value_.data != nullptr && !staging_.empty()) {
      const size_t n = std::min(static_cast<size_t>(value_.size), staging_.size());
      std::memcpy(value_.data, staging_.data(), n);
    }
    store_->metrics_.kv_bytes_read.fetch_add(value_.size);
  }

  complete_ns_ = now_ns();
  status_.store(Status::OK);
  state_.store(KvRequestState::COMPLETED);
  KV_LOG_DEBUG("req", "id=%llu op=%d completed latency_ns=%llu",
               static_cast<unsigned long long>(id_), static_cast<int>(op_),
               static_cast<unsigned long long>(complete_ns_ - submit_ns_));
  return Status::OK;
}

}  // namespace kvstore
