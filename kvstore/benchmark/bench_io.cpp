#include "kvstore/io_engine.h"
#include "kvstore/log.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace kvstore;

int main() {
  set_log_level(LogLevel::WARN);
  const auto path =
      (std::filesystem::temp_directory_path() / "kvstore_bench_io.bin").string();
  std::filesystem::remove(path);

  IoEngineConfig cfg;
  cfg.path = path;
  cfg.file_backed = true;
  cfg.size_bytes = 64ull * 1024 * 1024;
  auto engine = create_file_io_engine(cfg);
  if (!ok(engine->open())) {
    return 1;
  }

  constexpr int N = 1000;
  constexpr uint32_t blocks = 32;  // 128 KiB
  std::vector<uint8_t> buf(blocks * 4096, 0x42);

  const auto t0 = std::chrono::steady_clock::now();
  for (int i = 0; i < N; ++i) {
    IoRequest req{};
    req.lba = static_cast<uint64_t>(8 + i * blocks);
    req.blocks = blocks;
    req.buffer = buf.data();
    req.write = true;
    req.request_id = static_cast<uint64_t>(i + 1);
    if (!ok(engine->submit(&req)) || !ok(engine->wait(&req))) {
      return 1;
    }
    engine->release(&req);
  }
  const auto t1 = std::chrono::steady_clock::now();
  const double sec = std::chrono::duration<double>(t1 - t0).count();
  const double bytes = static_cast<double>(N) * blocks * 4096;
  std::printf("bench_io sequential write: IOPS=%.1f GB/s=%.3f\n", N / sec,
              (bytes / sec) / 1e9);

  engine->close();
  std::filesystem::remove(path);
  return 0;
}
