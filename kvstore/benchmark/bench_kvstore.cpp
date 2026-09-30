#include "kvstore/kvstore.h"
#include "kvstore/log.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <numeric>
#include <string>
#include <vector>

using namespace kvstore;
using clock_type = std::chrono::steady_clock;

static double percentile(std::vector<double>& v, double p) {
  if (v.empty()) {
    return 0;
  }
  std::sort(v.begin(), v.end());
  const size_t idx =
      static_cast<size_t>(std::min(v.size() - 1, static_cast<size_t>(p * (v.size() - 1))));
  return v[idx];
}

int main(int argc, char** argv) {
  set_log_level(LogLevel::WARN);

  int n = 1000;
  uint32_t kv_size = 128 * 1024;
  if (argc > 1) {
    n = std::atoi(argv[1]);
  }
  if (argc > 2) {
    kv_size = static_cast<uint32_t>(std::atoi(argv[2]));
  }

  const std::string path =
      (std::filesystem::temp_directory_path() / "kvstore_bench.bin").string();
  std::filesystem::remove(path);

  const uint64_t capacity =
      std::max<uint64_t>(256ull * 1024 * 1024,
                         static_cast<uint64_t>(n) * kv_size * 2 + 16ull * 1024 * 1024);

  KvStore store;
  KvStoreConfig cfg;
  cfg.device_path = path;
  cfg.file_backed = true;
  cfg.capacity_bytes = capacity;
  if (!ok(store.open(cfg))) {
    std::fprintf(stderr, "open failed\n");
    return 1;
  }

  std::vector<uint8_t> payload(kv_size, 0x77);
  std::vector<double> put_us;
  std::vector<double> get_us;
  put_us.reserve(n);
  get_us.reserve(n);

  const auto t0 = clock_type::now();
  for (int i = 0; i < n; ++i) {
    KvKey key{static_cast<uint64_t>(i), 1};
    KvValue val{payload.data(), kv_size};
    KvRequest* req = nullptr;
    const auto a = clock_type::now();
    if (!ok(store.put_async(key, val, req)) || !ok(store.wait(req))) {
      std::fprintf(stderr, "put failed\n");
      return 1;
    }
    store.release(req);
    const auto b = clock_type::now();
    put_us.push_back(std::chrono::duration<double, std::micro>(b - a).count());
  }
  const auto t1 = clock_type::now();

  for (int i = 0; i < n; ++i) {
    KvKey key{static_cast<uint64_t>(i), 1};
    std::vector<uint8_t> out(kv_size, 0);
    KvValue val{out.data(), kv_size};
    KvRequest* req = nullptr;
    const auto a = clock_type::now();
    if (!ok(store.get_async(key, val, req)) || !ok(store.wait(req))) {
      std::fprintf(stderr, "get failed\n");
      return 1;
    }
    store.release(req);
    const auto b = clock_type::now();
    get_us.push_back(std::chrono::duration<double, std::micro>(b - a).count());
  }
  const auto t2 = clock_type::now();

  const double put_sec = std::chrono::duration<double>(t1 - t0).count();
  const double get_sec = std::chrono::duration<double>(t2 - t1).count();
  const double bytes = static_cast<double>(n) * kv_size;

  std::printf("bench_kvstore n=%d kv_size=%u\n", n, kv_size);
  std::printf("PUT  IOPS=%.1f  GB/s=%.3f  P50=%.1fus P95=%.1fus P99=%.1fus\n",
              n / put_sec, (bytes / put_sec) / 1e9, percentile(put_us, 0.50),
              percentile(put_us, 0.95), percentile(put_us, 0.99));
  std::printf("GET  IOPS=%.1f  GB/s=%.3f  P50=%.1fus P95=%.1fus P99=%.1fus\n",
              n / get_sec, (bytes / get_sec) / 1e9, percentile(get_us, 0.50),
              percentile(get_us, 0.95), percentile(get_us, 0.99));

  store.close();
  std::filesystem::remove(path);
  return 0;
}
