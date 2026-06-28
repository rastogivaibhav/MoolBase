#include "graphene/db.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

using namespace graphene;
namespace fs = std::filesystem;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static uint64_t counter = 0;
  fs::path dir = fs::temp_directory_path() / ("graphenedb_fuzz_wal_" + std::to_string(::getpid()) + "_" + std::to_string(counter++));
  std::error_code ec;
  fs::remove_all(dir, ec);
  fs::create_directories(dir, ec);
  {
    std::ofstream wal(dir / "graphene.wal", std::ios::binary | std::ios::trunc);
    wal.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
  }
  DBOptions opt; opt.dimension = 4; opt.fsync_on_commit = false; opt.recover_stale_lock = true;
  GrapheneDB db;
  auto st = db.open(dir, opt);
  if (st) {
    std::string report;
    db.validate(&report);
    db.close();
  }
  fs::remove_all(dir, ec);
  return 0;
}
