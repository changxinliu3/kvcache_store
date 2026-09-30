#include "kvstore/kvstore.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace kvstore;

int main() {
  const std::string path =
      (std::filesystem::temp_directory_path() / "kvstore_example_put_get.bin").string();

  KvStore store;
  KvStoreConfig cfg;
  cfg.device_path = path;
  cfg.file_backed = true;
  cfg.capacity_bytes = 128ull * 1024 * 1024;
  if (!ok(store.open(cfg))) {
    std::fprintf(stderr, "open failed\n");
    return 1;
  }

  constexpr int N = 1000;
  std::vector<uint8_t> payload(128 * 1024, 0xA5);

  for (int i = 0; i < N; ++i) {
    KvKey key{static_cast<uint64_t>(i), 0};
    KvValue val{payload.data(), static_cast<uint32_t>(payload.size())};
    KvRequest* req = nullptr;
    if (!ok(store.put_async(key, val, req)) || !ok(store.wait(req))) {
      std::fprintf(stderr, "put failed at %d\n", i);
      return 1;
    }
    store.release(req);
  }

  for (int i = 0; i < N; ++i) {
    KvKey key{static_cast<uint64_t>(i), 0};
    std::vector<uint8_t> out(payload.size(), 0);
    KvValue val{out.data(), static_cast<uint32_t>(out.size())};
    KvRequest* req = nullptr;
    if (!ok(store.get_async(key, val, req)) || !ok(store.wait(req))) {
      std::fprintf(stderr, "get failed at %d\n", i);
      return 1;
    }
    store.release(req);
  }

  for (int i = 0; i < N; ++i) {
    KvKey key{static_cast<uint64_t>(i), 0};
    if (!ok(store.erase(key))) {
      std::fprintf(stderr, "erase failed at %d\n", i);
      return 1;
    }
  }

  store.close();
  std::filesystem::remove(path);
  std::printf("example kv_put_get: OK (%d keys)\n", N);
  return 0;
}
