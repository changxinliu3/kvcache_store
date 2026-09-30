#include "kvstore/kvstore.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

using namespace kvstore;

static std::string temp_path(const char* name) {
  return (std::filesystem::temp_directory_path() / name).string();
}

int main() {
  const std::string path = temp_path("kvstore_integration.bin");
  std::filesystem::remove(path);

  KvStore store;
  KvStoreConfig cfg;
  cfg.device_path = path;
  cfg.file_backed = true;
  cfg.capacity_bytes = 256ull * 1024 * 1024;
  cfg.data_start_lba = 16;
  assert(store.open(cfg) == Status::OK);

  // PUT -> GET
  KvKey k1{1, 1};
  std::vector<uint8_t> p1(32 * 1024);
  for (size_t i = 0; i < p1.size(); ++i) {
    p1[i] = static_cast<uint8_t>(i & 0xFF);
  }
  KvValue v1{p1.data(), static_cast<uint32_t>(p1.size())};
  KvRequest* req = nullptr;
  assert(store.put_async(k1, v1, req) == Status::OK);
  assert(store.wait(req) == Status::OK);
  assert(store.release(req) == Status::OK);

  std::vector<uint8_t> out(p1.size(), 0);
  KvValue g1{out.data(), static_cast<uint32_t>(out.size())};
  assert(store.get_async(k1, g1, req) == Status::OK);
  assert(store.wait(req) == Status::OK);
  assert(store.release(req) == Status::OK);
  assert(out == p1);

  // PUT -> UPDATE -> GET
  std::vector<uint8_t> p2(48 * 1024, 0x5A);
  KvValue v2{p2.data(), static_cast<uint32_t>(p2.size())};
  assert(store.put_async(k1, v2, req) == Status::OK);
  assert(store.wait(req) == Status::OK);
  assert(store.release(req) == Status::OK);

  out.assign(p2.size(), 0);
  g1 = KvValue{out.data(), static_cast<uint32_t>(out.size())};
  assert(store.get_async(k1, g1, req) == Status::OK);
  assert(store.wait(req) == Status::OK);
  assert(store.release(req) == Status::OK);
  assert(out == p2);

  // PUT -> DELETE -> GET miss
  assert(store.erase(k1) == Status::OK);
  out.assign(p2.size(), 0);
  g1 = KvValue{out.data(), static_cast<uint32_t>(out.size())};
  assert(store.get_async(k1, g1, req) == Status::NOT_FOUND);

  // Missing key
  KvKey missing{9, 9};
  assert(store.erase(missing) == Status::NOT_FOUND);

  // Concurrent PUT/GET
  constexpr int N = 200;
  std::vector<std::thread> writers;
  for (int t = 0; t < 4; ++t) {
    writers.emplace_back([&, t] {
      for (int i = 0; i < N; ++i) {
        KvKey key{static_cast<uint64_t>(t), static_cast<uint64_t>(i)};
        std::vector<uint8_t> buf(8192, static_cast<uint8_t>(t + 1));
        KvValue val{buf.data(), static_cast<uint32_t>(buf.size())};
        KvRequest* r = nullptr;
        Status st = store.put_async(key, val, r);
        assert(st == Status::OK);
        assert(store.wait(r) == Status::OK);
        assert(store.release(r) == Status::OK);
      }
    });
  }
  for (auto& th : writers) {
    th.join();
  }

  for (int t = 0; t < 4; ++t) {
    for (int i = 0; i < N; i += 17) {
      KvKey key{static_cast<uint64_t>(t), static_cast<uint64_t>(i)};
      std::vector<uint8_t> buf(8192, 0);
      KvValue val{buf.data(), static_cast<uint32_t>(buf.size())};
      KvRequest* r = nullptr;
      assert(store.get_async(key, val, r) == Status::OK);
      assert(store.wait(r) == Status::OK);
      assert(store.release(r) == Status::OK);
      assert(buf[0] == static_cast<uint8_t>(t + 1));
    }
  }

  assert(store.close() == Status::OK);
  std::filesystem::remove(path);

  std::printf("[PASS] KV PUT\n");
  std::printf("[PASS] KV GET\n");
  std::printf("[PASS] KV DELETE\n");
  std::printf("[PASS] Async request\n");
  std::printf("[PASS] Concurrent GET\n");
  std::printf("[PASS] Concurrent PUT\n");
  return 0;
}
