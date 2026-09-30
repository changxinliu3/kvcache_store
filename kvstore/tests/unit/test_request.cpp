#include "kvstore/kvstore.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace kvstore;

int main() {
  const auto path =
      (std::filesystem::temp_directory_path() / "kvstore_test_request.bin").string();
  std::filesystem::remove(path);

  KvStore store;
  KvStoreConfig cfg;
  cfg.device_path = path;
  cfg.file_backed = true;
  cfg.capacity_bytes = 64ull * 1024 * 1024;
  cfg.data_start_lba = 8;
  assert(store.open(cfg) == Status::OK);

  KvKey key{42, 7};
  std::vector<uint8_t> payload(16 * 1024, 0xAB);
  KvValue val{payload.data(), static_cast<uint32_t>(payload.size())};

  KvRequest* req = nullptr;
  assert(store.put_async(key, val, req) == Status::OK);
  assert(req != nullptr);
  assert(store.wait(req) == Status::OK);
  assert(req->state() == KvRequestState::COMPLETED);
  assert(store.release(req) == Status::OK);

  std::vector<uint8_t> out(payload.size(), 0);
  KvValue got{out.data(), static_cast<uint32_t>(out.size())};
  assert(store.get_async(key, got, req) == Status::OK);
  assert(store.wait(req) == Status::OK);
  assert(store.release(req) == Status::OK);
  assert(out == payload);

  (void)store.close();
  std::filesystem::remove(path);
  std::printf("[PASS] Request\n");
  return 0;
}
