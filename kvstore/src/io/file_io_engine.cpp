#include "kvstore/io_engine.h"
#include "kvstore/log.h"

#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <cstdio>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace kvstore {
namespace {

struct PendingIo {
  IoState state{IoState::CREATED};
  Status status{Status::OK};
  std::mutex mu;
  std::condition_variable cv;
};

// File-backed / portable test engine.
// Production path should use IoUringEngine against a raw block device.
class FileIoEngine final : public IoEngine {
 public:
  explicit FileIoEngine(IoEngineConfig config) : config_(std::move(config)) {}

  ~FileIoEngine() override { (void)close(); }

  Status open() override {
    if (fd_open_) {
      return Status::OK;
    }
    if (config_.path.empty()) {
      return Status::INVALID_ARGUMENT;
    }

#if defined(_WIN32)
    // Portable test mode on Windows: std::fstream over a file.
    if (config_.file_backed) {
      {
        std::fstream probe(config_.path, std::ios::in | std::ios::out | std::ios::binary);
        if (!probe.good()) {
          std::ofstream create(config_.path, std::ios::binary | std::ios::trunc);
          if (!create) {
            return Status::IO_ERROR;
          }
          if (config_.size_bytes > 0) {
            create.seekp(static_cast<std::streamoff>(config_.size_bytes - 1));
            create.put('\0');
          }
        }
      }
      file_ = std::make_unique<std::fstream>(config_.path,
                                             std::ios::in | std::ios::out | std::ios::binary);
      if (!file_->good()) {
        return Status::IO_ERROR;
      }
      fd_open_ = true;
      return Status::OK;
    }
    return Status::NOT_SUPPORTED;
#else
    int flags = O_RDWR;
    if (config_.file_backed) {
      flags |= O_CREAT;
      if (config_.o_direct) {
        flags |= O_DIRECT;
      }
      fd_ = ::open(config_.path.c_str(), flags, 0644);
      if (fd_ < 0) {
        KV_LOG_ERROR("io", "open(%s) failed errno=%d", config_.path.c_str(), errno);
        return Status::IO_ERROR;
      }
      if (config_.size_bytes > 0) {
        if (::ftruncate(fd_, static_cast<off_t>(config_.size_bytes)) != 0) {
          ::close(fd_);
          fd_ = -1;
          return Status::IO_ERROR;
        }
      }
    } else {
      if (config_.o_direct) {
        flags |= O_DIRECT;
      }
      fd_ = ::open(config_.path.c_str(), flags);
      if (fd_ < 0) {
        KV_LOG_ERROR("io", "open device(%s) failed errno=%d", config_.path.c_str(), errno);
        return Status::IO_ERROR;
      }
    }
    fd_open_ = true;
    return Status::OK;
#endif
  }

  Status close() override {
    if (!fd_open_) {
      return Status::OK;
    }
#if defined(_WIN32)
    file_.reset();
#else
    if (fd_ >= 0) {
      ::close(fd_);
      fd_ = -1;
    }
#endif
    fd_open_ = false;
    return Status::OK;
  }

  Status submit(IoRequest* request) override {
    if (!fd_open_ || request == nullptr || request->buffer == nullptr || request->blocks == 0) {
      return Status::INVALID_ARGUMENT;
    }

    auto pending = std::make_shared<PendingIo>();
    pending->state = IoState::SUBMITTED;

    {
      std::lock_guard lock(map_mu_);
      pending_[request->request_id] = pending;
    }

    // Async completion via worker thread (portable stand-in for io_uring CQ).
    std::thread([this, request, pending]() {
      {
        std::lock_guard lock(pending->mu);
        pending->state = IoState::IN_PROGRESS;
      }

      const uint64_t offset =
          request->lba * static_cast<uint64_t>(block_size());
      const size_t nbytes =
          static_cast<size_t>(request->blocks) * static_cast<size_t>(block_size());

      Status st = do_io(offset, request->buffer, nbytes, request->write);

      {
        std::lock_guard lock(pending->mu);
        pending->status = st;
        pending->state = ok(st) ? IoState::COMPLETED : IoState::FAILED;
      }
      pending->cv.notify_all();
    }).detach();

    return Status::OK;
  }

  IoState query(IoRequest* request) override {
    if (request == nullptr) {
      return IoState::FAILED;
    }
    std::shared_ptr<PendingIo> pending;
    {
      std::lock_guard lock(map_mu_);
      auto it = pending_.find(request->request_id);
      if (it == pending_.end()) {
        return IoState::FAILED;
      }
      pending = it->second;
    }
    std::lock_guard lock(pending->mu);
    return pending->state;
  }

  Status wait(IoRequest* request) override {
    if (request == nullptr) {
      return Status::INVALID_ARGUMENT;
    }
    std::shared_ptr<PendingIo> pending;
    {
      std::lock_guard lock(map_mu_);
      auto it = pending_.find(request->request_id);
      if (it == pending_.end()) {
        return Status::INVALID_ARGUMENT;
      }
      pending = it->second;
    }
    std::unique_lock lock(pending->mu);
    pending->cv.wait(lock, [&] {
      return pending->state == IoState::COMPLETED || pending->state == IoState::FAILED;
    });
    return pending->status;
  }

  Status release(IoRequest* request) override {
    if (request == nullptr) {
      return Status::INVALID_ARGUMENT;
    }
    std::lock_guard lock(map_mu_);
    pending_.erase(request->request_id);
    return Status::OK;
  }

 private:
  Status do_io(uint64_t offset, void* buffer, size_t nbytes, bool write) {
#if defined(_WIN32)
    std::lock_guard lock(file_mu_);
    if (!file_ || !file_->good()) {
      return Status::IO_ERROR;
    }
    file_->seekp(static_cast<std::streamoff>(offset));
    file_->seekg(static_cast<std::streamoff>(offset));
    if (write) {
      file_->write(static_cast<const char*>(buffer), static_cast<std::streamsize>(nbytes));
    } else {
      file_->read(static_cast<char*>(buffer), static_cast<std::streamsize>(nbytes));
    }
    if (!file_->good()) {
      file_->clear();
      return Status::IO_ERROR;
    }
    file_->flush();
    return Status::OK;
#else
    size_t done = 0;
    while (done < nbytes) {
      ssize_t n = 0;
      if (write) {
        n = ::pwrite(fd_, static_cast<const char*>(buffer) + done, nbytes - done,
                     static_cast<off_t>(offset + done));
      } else {
        n = ::pread(fd_, static_cast<char*>(buffer) + done, nbytes - done,
                    static_cast<off_t>(offset + done));
      }
      if (n < 0) {
        if (errno == EINTR) {
          continue;
        }
        return Status::IO_ERROR;
      }
      if (n == 0) {
        return Status::IO_ERROR;
      }
      done += static_cast<size_t>(n);
    }
    return Status::OK;
#endif
  }

  IoEngineConfig config_;
  bool fd_open_{false};
#if defined(_WIN32)
  std::unique_ptr<std::fstream> file_;
  std::mutex file_mu_;
#else
  int fd_{-1};
#endif
  std::mutex map_mu_;
  std::unordered_map<uint64_t, std::shared_ptr<PendingIo>> pending_;
};

}  // namespace

std::unique_ptr<IoEngine> create_file_io_engine(const IoEngineConfig& config) {
  return std::make_unique<FileIoEngine>(config);
}

}  // namespace kvstore
