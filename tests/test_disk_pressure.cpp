#include "graphene/db.hpp"

#include <cassert>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <limits>
#include <sys/resource.h>
#include <unistd.h>

using namespace graphene;
namespace fs = std::filesystem;

static void require(Status st, const char* msg) {
  if (!st) {
    std::cerr << "FAIL: " << msg << ": " << st.message << "\n";
    std::abort();
  }
}

static NodeInput node(const std::string& content, float base) {
  NodeInput input;
  input.content = content;
  input.vector = {base, base + 0.01f, base + 0.02f, base + 0.03f};
  input.signature = signature_for(8, 11);
  input.incident = 811;
  input.root = true;
  input.metadata["disk_pressure"] = "true";
  return input;
}

static rlim_t add_limit(rlim_t base, rlim_t delta) {
  if (base == RLIM_INFINITY) return RLIM_INFINITY;
  if (base > std::numeric_limits<rlim_t>::max() - delta) return RLIM_INFINITY;
  return base + delta;
}

int main() {
  if (::geteuid() == 0) {
    std::cout << "graphenedb_disk_pressure_skipped=root\n";
    return 0;
  }

  fs::path base = fs::temp_directory_path() / "graphenedb_disk_pressure";
  fs::remove_all(base);

  DBOptions opt;
  opt.dimension = 4;
  opt.fsync_on_commit = true;

  GrapheneDB db;
  require(db.open(base, opt), "open disk-pressure db");
  require(db.put_node(node("baseline durable node", 0.1f)), "put baseline node");
  assert(db.node_count() == 1);

  struct rlimit old_limit {};
  if (::getrlimit(RLIMIT_FSIZE, &old_limit) != 0) {
    std::cerr << "graphenedb_disk_pressure_skipped=getrlimit:" << std::strerror(errno) << "\n";
    require(db.close(), "close after getrlimit skip");
    return 0;
  }

  auto old_sig = std::signal(SIGXFSZ, SIG_IGN);

  std::error_code ec;
  uint64_t wal_size = fs::exists(base / "graphene.wal", ec) ? fs::file_size(base / "graphene.wal", ec) : 0;
  if (ec || wal_size > static_cast<uint64_t>(std::numeric_limits<rlim_t>::max() / 2)) {
    std::signal(SIGXFSZ, old_sig);
    require(db.close(), "close after wal size skip");
    std::cout << "graphenedb_disk_pressure_skipped=wal_size\n";
    return 0;
  }

  struct rlimit limited = old_limit;
  limited.rlim_cur = add_limit(static_cast<rlim_t>(wal_size), static_cast<rlim_t>(4096));
  if (old_limit.rlim_max != RLIM_INFINITY && limited.rlim_cur > old_limit.rlim_max) {
    std::signal(SIGXFSZ, old_sig);
    require(db.close(), "close after hard limit skip");
    std::cout << "graphenedb_disk_pressure_skipped=hard_limit\n";
    return 0;
  }
  if (::setrlimit(RLIMIT_FSIZE, &limited) != 0) {
    std::signal(SIGXFSZ, old_sig);
    require(db.close(), "close after setrlimit skip");
    std::cout << "graphenedb_disk_pressure_skipped=setrlimit:" << std::strerror(errno) << "\n";
    return 0;
  }

  std::string large_content(128 * 1024, 'x');
  auto st = db.put_node(node(large_content, 0.2f));

  if (::setrlimit(RLIMIT_FSIZE, &old_limit) != 0) {
    std::cerr << "FAIL: restore RLIMIT_FSIZE: " << std::strerror(errno) << "\n";
    std::abort();
  }
  std::signal(SIGXFSZ, old_sig);

  assert(!st);
  assert(st.code == ErrorCode::IoError);
  assert(db.node_count() == 1);
  require(db.validate(), "validate after disk-pressure write failure");
  require(db.close(), "close after disk-pressure write failure");

  GrapheneDB reopened;
  require(reopened.open(base, opt), "reopen after disk-pressure write failure");
  assert(reopened.node_count() == 1);
  auto found = reopened.metadata_search("disk_pressure", "true");
  assert(found.size() == 1);
  require(reopened.validate(), "validate reopened after disk-pressure write failure");
  require(reopened.close(), "close reopened disk-pressure db");

  fs::remove_all(base);
  std::cout << "graphenedb_disk_pressure_passed=true\n";
  return 0;
}
