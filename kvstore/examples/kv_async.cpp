#include "kvstore/kvstore.h"

#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace kvstore;

int main() {
  const std::string path =
      (std::filesystem::temp_directory_path() / "kvstore_example_async.bin").string();

  KvStore store;
  KvStoreConfig cfg;
  cfg.device_path = path;
  cfg.file_backed = true;
  cfg.capacity_bytes = 64ull * 1024 * 1024;
  if (!ok(store.open(cfg))) {
    return 1;
  }

  constexpr int N = 64;
  std::vector<std::vector<uint8_t>> payloads(N, std::vector<uint8_t>(4096, 0x3C));
  std::vector<KvRequest*> reqs(N, nullptr);

  for (int i = 0; i < N; ++i) {
    KvKey key{static_cast<uint64_t>(i), 99};
    KvValue val{payloads[i].data(), static_cast<uint32_t>(payloads[i].size())};
    if (!ok(store.put_async(key, val, reqs[i]))) {
      std::fprintf(stderr, "submit failed\n");
      return 1;
    }
  }

  for (int i = 0; i < N; ++i) {
    while (true) {
      auto st = store.query(reqs[i]);
      if (st == KvRequestState::COMPLETED) {
        break;
      }
      if (st == KvRequestState::FAILED) {
        std::fprintf(stderr, "request failed\n");
        return 1;
      }
    }
    store.release(reqs[i]);
  }

  store.close();
  std::filesystem::remove(path);
  std::printf("example kv_async: OK (%d inflight puts)\n", N);
  return 0;
}
