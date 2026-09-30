#include "kvstore/io_engine.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

using namespace kvstore;

int main() {
  const auto path =
      (std::filesystem::temp_directory_path() / "kvstore_io_test.bin").string();
  std::filesystem::remove(path);

  IoEngineConfig cfg;
  cfg.path = path;
  cfg.file_backed = true;
  cfg.size_bytes = 8ull * 1024 * 1024;

  auto engine = create_file_io_engine(cfg);
  assert(engine->open() == Status::OK);

  std::vector<uint8_t> wbuf(4096 * 8, 0x11);
  std::vector<uint8_t> rbuf(4096 * 8, 0);

  IoRequest wr{};
  wr.lba = 10;
  wr.blocks = 8;
  wr.buffer = wbuf.data();
  wr.write = true;
  wr.request_id = 1;
  assert(engine->submit(&wr) == Status::OK);
  assert(engine->wait(&wr) == Status::OK);
  assert(engine->query(&wr) == IoState::COMPLETED);
  assert(engine->release(&wr) == Status::OK);

  IoRequest rd{};
  rd.lba = 10;
  rd.blocks = 8;
  rd.buffer = rbuf.data();
  rd.write = false;
  rd.request_id = 2;
  assert(engine->submit(&rd) == Status::OK);
  assert(engine->wait(&rd) == Status::OK);
  assert(engine->release(&rd) == Status::OK);
  assert(rbuf == wbuf);

  assert(engine->close() == Status::OK);
  std::filesystem::remove(path);
  std::printf("[PASS] IO Engine\n");
  return 0;
}
