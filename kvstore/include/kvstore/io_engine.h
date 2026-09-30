#pragma once

#include "kvstore/status.h"

#include <cstdint>
#include <memory>
#include <string>

#ifndef KVSTORE_HAS_IO_URING
#define KVSTORE_HAS_IO_URING 0
#endif

namespace kvstore {

struct IoRequest {
  uint64_t lba{0};
  uint32_t blocks{0};
  void* buffer{nullptr};
  bool write{false};
  uint64_t request_id{0};
};

enum class IoState {
  CREATED,
  SUBMITTED,
  IN_PROGRESS,
  COMPLETED,
  FAILED,
};

class IoEngine {
 public:
  virtual ~IoEngine() = default;

  virtual Status open() = 0;
  virtual Status close() = 0;

  virtual Status submit(IoRequest* request) = 0;
  virtual IoState query(IoRequest* request) = 0;
  virtual Status wait(IoRequest* request) = 0;
  virtual Status release(IoRequest* request) = 0;

  virtual uint32_t block_size() const { return 4096; }
};

struct IoEngineConfig {
  std::string path;           // block device or file-backed test path
  uint64_t size_bytes{0};     // required for file-backed create
  bool file_backed{true};     // test mode: regular file
  bool o_direct{false};       // Linux O_DIRECT when available
  uint32_t queue_depth{128};
};

std::unique_ptr<IoEngine> create_file_io_engine(const IoEngineConfig& config);

#if KVSTORE_HAS_IO_URING
std::unique_ptr<IoEngine> create_io_uring_engine(const IoEngineConfig& config);
#endif

}  // namespace kvstore
