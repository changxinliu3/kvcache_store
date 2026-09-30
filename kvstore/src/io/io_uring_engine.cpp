#include "kvstore/io_engine.h"
#include "kvstore/log.h"

#if KVSTORE_HAS_IO_URING

#include <liburing.h>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <unistd.h>
#include <unordered_map>

namespace kvstore {
namespace {

struct UringPending {
  IoState state{IoState::CREATED};
  Status status{Status::OK};
  int res{0};
};

class IoUringEngine final : public IoEngine {
 public:
  explicit IoUringEngine(IoEngineConfig config) : config_(std::move(config)) {}

  ~IoUringEngine() override { (void)close(); }

  Status open() override {
    if (opened_) {
      return Status::OK;
    }
    if (config_.path.empty()) {
      return Status::INVALID_ARGUMENT;
    }

    int flags = O_RDWR;
    if (config_.file_backed) {
      flags |= O_CREAT;
    }
    if (config_.o_direct) {
      flags |= O_DIRECT;
    }

    fd_ = ::open(config_.path.c_str(), flags, 0644);
    if (fd_ < 0) {
      KV_LOG_ERROR("uring", "open(%s) failed errno=%d", config_.path.c_str(), errno);
      return Status::IO_ERROR;
    }

    if (config_.file_backed && config_.size_bytes > 0) {
      if (::ftruncate(fd_, static_cast<off_t>(config_.size_bytes)) != 0) {
        ::close(fd_);
        fd_ = -1;
        return Status::IO_ERROR;
      }
    }

    const unsigned qd = config_.queue_depth == 0 ? 128u : config_.queue_depth;
    if (io_uring_queue_init(qd, &ring_, 0) != 0) {
      ::close(fd_);
      fd_ = -1;
      return Status::IO_ERROR;
    }

    opened_ = true;
    return Status::OK;
  }

  Status close() override {
    if (!opened_) {
      return Status::OK;
    }
    io_uring_queue_exit(&ring_);
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
    opened_ = false;
    return Status::OK;
  }

  Status submit(IoRequest* request) override {
    if (!opened_ || request == nullptr || request->buffer == nullptr || request->blocks == 0) {
      return Status::INVALID_ARGUMENT;
    }

    auto pending = std::make_shared<UringPending>();
    pending->state = IoState::SUBMITTED;

    {
      std::lock_guard lock(mu_);
      pending_[request->request_id] = pending;

      io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
      if (sqe == nullptr) {
        pending_.erase(request->request_id);
        return Status::IO_ERROR;
      }

      const uint64_t offset = request->lba * static_cast<uint64_t>(block_size());
      const unsigned nbytes = request->blocks * block_size();

      if (request->write) {
        io_uring_prep_write(sqe, fd_, request->buffer, nbytes, offset);
      } else {
        io_uring_prep_read(sqe, fd_, request->buffer, nbytes, offset);
      }
      io_uring_sqe_set_data64(sqe, request->request_id);

      const int ret = io_uring_submit(&ring_);
      if (ret < 0) {
        pending_.erase(request->request_id);
        return Status::IO_ERROR;
      }
      pending->state = IoState::IN_PROGRESS;
    }
    return Status::OK;
  }

  IoState query(IoRequest* request) override {
    if (request == nullptr) {
      return IoState::FAILED;
    }
    harvest();
    std::lock_guard lock(mu_);
    auto it = pending_.find(request->request_id);
    if (it == pending_.end()) {
      return IoState::FAILED;
    }
    return it->second->state;
  }

  Status wait(IoRequest* request) override {
    if (request == nullptr) {
      return Status::INVALID_ARGUMENT;
    }

    while (true) {
      {
        std::lock_guard lock(mu_);
        auto it = pending_.find(request->request_id);
        if (it == pending_.end()) {
          return Status::INVALID_ARGUMENT;
        }
        if (it->second->state == IoState::COMPLETED) {
          return Status::OK;
        }
        if (it->second->state == IoState::FAILED) {
          return it->second->status;
        }
      }

      io_uring_cqe* cqe = nullptr;
      const int ret = io_uring_wait_cqe(&ring_, &cqe);
      if (ret < 0) {
        return Status::IO_ERROR;
      }
      complete_one(cqe);
      io_uring_cqe_seen(&ring_, cqe);
    }
  }

  Status release(IoRequest* request) override {
    if (request == nullptr) {
      return Status::INVALID_ARGUMENT;
    }
    std::lock_guard lock(mu_);
    pending_.erase(request->request_id);
    return Status::OK;
  }

 private:
  void harvest() {
    io_uring_cqe* cqe = nullptr;
    while (io_uring_peek_cqe(&ring_, &cqe) == 0 && cqe != nullptr) {
      complete_one(cqe);
      io_uring_cqe_seen(&ring_, cqe);
      cqe = nullptr;
    }
  }

  void complete_one(io_uring_cqe* cqe) {
    const uint64_t id = io_uring_cqe_get_data64(cqe);
    std::lock_guard lock(mu_);
    auto it = pending_.find(id);
    if (it == pending_.end()) {
      return;
    }
    it->second->res = cqe->res;
    if (cqe->res < 0) {
      it->second->status = Status::IO_FAILED;
      it->second->state = IoState::FAILED;
    } else {
      it->second->status = Status::OK;
      it->second->state = IoState::COMPLETED;
    }
  }

  IoEngineConfig config_;
  bool opened_{false};
  int fd_{-1};
  io_uring ring_{};
  std::mutex mu_;
  std::unordered_map<uint64_t, std::shared_ptr<UringPending>> pending_;
};

}  // namespace

std::unique_ptr<IoEngine> create_io_uring_engine(const IoEngineConfig& config) {
  return std::make_unique<IoUringEngine>(config);
}

}  // namespace kvstore

#endif  // KVSTORE_HAS_IO_URING
