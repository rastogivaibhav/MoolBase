#include "graphene/db.hpp"
#include "graphene/platform.hpp"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#ifdef NDEBUG
#error "GrapheneDB test targets must keep assertions enabled"
#endif

using namespace graphene;
namespace fs = std::filesystem;

static fs::path unique_dir(const char* label) {
  const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
  return fs::temp_directory_path() /
         (std::string(label) + "-" + std::to_string(platform::current_pid()) + "-" + std::to_string(tick));
}

static NodeInput node(std::optional<LatticeCoord> lattice = std::nullopt) {
  NodeInput input;
  input.content = "p0 contract node";
  input.vector = {1.0f, 0.0f};
  input.signature = signature_for(1, 1);
  input.lattice = lattice;
  return input;
}

static void set_env(const char* name, const char* value) {
#ifdef _WIN32
  _putenv_s(name, value);
#else
  setenv(name, value, 1);
#endif
}

static void unset_env(const char* name) {
#ifdef _WIN32
  _putenv_s(name, "");
#else
  unsetenv(name);
#endif
}

int main() {
  DBOptions options;
  options.dimension = 2;

  // The platform primitive itself must provide create-if-absent semantics.
  {
    const auto dir = unique_dir("graphenedb-exclusive-create");
    fs::create_directories(dir);
    const auto lock = dir / "LOCK";
    assert(platform::create_file_exclusive(lock, "first\n"));
    const auto second = platform::create_file_exclusive(lock, "second\n");
    assert(!second && second.code == ErrorCode::LockBusy);
    std::error_code ec;
    fs::remove_all(dir, ec);
  }

  // create_if_missing=false must not mutate the filesystem.
  {
    const auto dir = unique_dir("graphenedb-no-create");
    std::error_code ec;
    fs::remove_all(dir, ec);
    DBOptions no_create = options;
    no_create.create_if_missing = false;
    GrapheneDB db;
    const auto st = db.open(dir, no_create);
    assert(!st && st.code == ErrorCode::InvalidOption);
    assert(!fs::exists(dir));
  }

  // A complete corrupt WAL frame is corruption, not a torn tail.
  {
    const auto dir = unique_dir("graphenedb-complete-corrupt-wal");
    GrapheneDB seed;
    assert(seed.open(dir, options));
    assert(seed.put_node(node()));
    assert(seed.close());
    {
      std::ofstream wal(dir / "graphene.wal", std::ios::binary | std::ios::app);
      wal << "1|0|X\n";
    }
    GrapheneDB reopened;
    const auto st = reopened.open(dir, options);
    assert(!st && st.code == ErrorCode::WalCorrupt);
    std::error_code ec;
    fs::remove_all(dir, ec);
  }

  // An unterminated final fragment remains a recoverable torn tail.
  {
    const auto dir = unique_dir("graphenedb-torn-wal");
    GrapheneDB seed;
    assert(seed.open(dir, options));
    assert(seed.put_node(node()));
    assert(seed.close());
    {
      std::ofstream wal(dir / "graphene.wal", std::ios::binary | std::ios::app);
      wal << "unterminated-tail";
    }
    GrapheneDB reopened;
    assert(reopened.open(dir, options));
    assert(reopened.node_count() == 1);
    assert(reopened.close());
    std::error_code ec;
    fs::remove_all(dir, ec);
  }

  // Failed pre-commit writes must not consume public IDs or snapshot versions.
  {
    const auto dir = unique_dir("graphenedb-precommit");
    GrapheneDB db;
    assert(db.open(dir, options));
    set_env("GRAPHENEDB_TEST_FAIL_WAL_APPEND", "1");
    const auto failed = db.put_node(node());
    unset_env("GRAPHENEDB_TEST_FAIL_WAL_APPEND");
    assert(!failed);
    assert(db.node_count() == 0);
    assert(db.snapshot() == 0);
    uint32_t id = UINT32_MAX;
    assert(db.put_node(node(), &id));
    assert(id == 0);
    assert(db.snapshot() == 1);
    assert(db.close());
    std::error_code ec;
    fs::remove_all(dir, ec);
  }

  // Physical-primary addressing must reject negative and out-of-radius cells
  // before appending a transaction.
  {
    const auto dir = unique_dir("graphenedb-physical-bounds");
    DBOptions physical = options;
    physical.require_lattice = true;
    physical.physical_lattice_storage = true;
    physical.physical_lattice_primary = true;
    physical.physical_lattice_radius = 2;
    GrapheneDB db;
    assert(db.open(dir, physical));
    auto st = db.put_node(node(LatticeCoord{0, 0, -1}));
    assert(!st && st.code == ErrorCode::InvalidInput);
    st = db.put_node(node(LatticeCoord{3, 0, 0}));
    assert(!st && st.code == ErrorCode::InvalidInput);
    assert(db.node_count() == 0);
    assert(db.close());
    std::error_code ec;
    fs::remove_all(dir, ec);
  }

  // An automatic checkpoint failure occurs after the WAL commit. The write is
  // successful and inspect() exposes the required maintenance.
  {
    const auto dir = unique_dir("graphenedb-maintenance");
    DBOptions rotating = options;
    rotating.wal_rotate_bytes = 1;
    GrapheneDB db;
    assert(db.open(dir, rotating));
    set_env("GRAPHENEDB_TEST_FAIL_CHECKPOINT_WRITE", "1");
    uint32_t id = UINT32_MAX;
    const auto committed = db.put_node(node(), &id);
    unset_env("GRAPHENEDB_TEST_FAIL_CHECKPOINT_WRITE");
    assert(committed);
    assert(id == 0 && db.node_count() == 1);
    std::string inspect;
    assert(db.inspect(&inspect));
    assert(inspect.find("maintenance_required=true") != std::string::npos);
    assert(db.compact());
    assert(db.inspect(&inspect));
    assert(inspect.find("maintenance_required=false") != std::string::npos);
    assert(db.close());

    GrapheneDB reopened;
    assert(reopened.open(dir, rotating));
    assert(reopened.node_count() == 1);
    assert(reopened.close());
    std::error_code ec;
    fs::remove_all(dir, ec);
  }

  std::cout << "p0_contract_tests_passed=true\n";
  return 0;
}
