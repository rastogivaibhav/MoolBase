#include "graphene/db.hpp"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

using namespace graphene;
namespace fs = std::filesystem;

static void require(Status st, const char* msg) {
  if (!st) {
    std::cerr << "FAIL: " << msg << ": " << st.message << "\n";
    std::abort();
  }
}

static void chmod_path(const fs::path& path, mode_t mode, const char* msg) {
  if (::chmod(path.c_str(), mode) != 0) {
    std::cerr << "FAIL: " << msg << ": " << std::strerror(errno) << "\n";
    std::abort();
  }
}

static NodeInput node(const std::string& content) {
  NodeInput input;
  input.content = content;
  input.vector = {0.1f, 0.2f, 0.3f, 0.4f};
  input.signature = signature_for(7, 9);
  input.incident = 709;
  input.root = true;
  return input;
}

int main() {
  if (::geteuid() == 0) {
    std::cout << "graphenedb_real_filesystem_failures_skipped=root\n";
    return 0;
  }

  fs::path base = fs::temp_directory_path() / "graphenedb_real_filesystem_failures";
  fs::remove_all(base);

  DBOptions opt;
  opt.dimension = 4;
  opt.fsync_on_commit = true;

  fs::path source = base / "source";
  {
    GrapheneDB db;
    require(db.open(source, opt), "open source");
    require(db.put_node(node("durable root")), "put source node");
    require(db.close(), "close source");
  }

  // A real read-only WAL should fail on open before any in-memory state is
  // exposed as writable. This complements deterministic injected WAL failures.
  chmod_path(source / "graphene.wal", 0444, "chmod read-only wal");
  {
    GrapheneDB db;
    auto st = db.open(source, opt);
    assert(!st);
    assert(st.code == ErrorCode::IoError);
  }
  chmod_path(source / "graphene.wal", 0644, "restore wal permissions");
  {
    GrapheneDB db;
    require(db.open(source, opt), "reopen after wal permission restore");
    assert(db.node_count() == 1);
    require(db.validate(), "validate after wal permission restore");
    require(db.close(), "close after wal permission restore");
  }

  // A destination that cannot be written should make backup fail without
  // mutating the source database.
  fs::path denied_parent = base / "denied_parent";
  fs::create_directories(denied_parent);
  chmod_path(denied_parent, 0500, "chmod denied backup parent");
  {
    GrapheneDB db;
    require(db.open(source, opt), "open backup source");
    auto st = db.backup(denied_parent / "backup");
    assert(!st);
    assert(st.code == ErrorCode::IoError);
    assert(db.node_count() == 1);
    require(db.validate(), "validate after denied backup");
    require(db.close(), "close backup source");
  }
  chmod_path(denied_parent, 0700, "restore backup parent permissions");

  fs::remove_all(base);
  std::cout << "graphenedb_real_filesystem_failures_passed=true\n";
  return 0;
}
