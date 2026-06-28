#include "graphene/db.hpp"
#include "graphene/platform.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> vec(uint32_t d, float base=0.1f) { std::vector<float> v(d); for (uint32_t i=0;i<d;i++) v[i]=base+0.001f*i; return v; }
static void require(Status st, const char* what) { if (!st) { std::cerr << "FAIL " << what << ": " << st.message << "\n"; std::abort(); } }

int main() {
  const uint32_t D = 12;

  // Stale lock recovery: fake dead PID is removed; live PID remains protected.
  fs::path stale = fs::temp_directory_path() / "graphenedb_rc_stale_lock";
  fs::remove_all(stale); fs::create_directories(stale);
  { std::ofstream lock(stale / "LOCK"); lock << "999999999\n"; }
  DBOptions opt; opt.dimension = D; opt.fsync_on_commit = false; opt.recover_stale_lock = true;
  GrapheneDB recovered; require(recovered.open(stale, opt), "recover stale lock"); require(recovered.close(), "close recovered stale lock");

  fs::path busy = fs::temp_directory_path() / "graphenedb_rc_live_lock";
  fs::remove_all(busy); fs::create_directories(busy);
  { std::ofstream lock(busy / "LOCK"); lock << graphene::platform::current_pid() << "\n"; }
  GrapheneDB blocked; auto st = blocked.open(busy, opt); assert(!st && st.code == ErrorCode::LockBusy);
  fs::remove_all(busy);

  // WAL rotation + metadata index.
  fs::path dir = fs::temp_directory_path() / "graphenedb_rc_rotation_metadata";
  fs::remove_all(dir);
  opt.wal_rotate_bytes = 4096;
  GrapheneDB db; require(db.open(dir, opt), "open rotation");
  uint32_t first_a = 0;
  for (int i=0; i<200; ++i) {
    NodeInput n;
    n.content = "rotating metadata node " + std::to_string(i);
    n.vector = vec(D, 0.2f + (i%3)*0.01f);
    n.signature = signature_for(static_cast<uint32_t>(i%4), static_cast<uint32_t>(i%8));
    n.incident = static_cast<uint32_t>(i);
    n.metadata["project"] = (i % 2 == 0) ? "alpha" : "beta";
    n.metadata["kind"] = "soakable";
    uint32_t id; require(db.put_node(n, &id), "put rotating node");
    if (i == 0) first_a = id;
  }
  auto alpha = db.metadata_search("project", "alpha");
  auto beta = db.metadata_search("project", "beta");
  assert(alpha.size() == 100); assert(beta.size() == 100);
  require(db.delete_node(first_a), "delete alpha");
  alpha = db.metadata_search("project", "alpha");
  assert(alpha.size() == 99);
  require(db.close(), "close rotation");

  std::error_code ec;
  auto wal_size = fs::exists(dir / "graphene.wal", ec) ? fs::file_size(dir / "graphene.wal", ec) : 0;
  assert(!ec);
  assert(fs::exists(dir / "graphene.data"));
  assert(wal_size < 4096 * 3); // final tail may contain a few recent transactions after last rotation.

  GrapheneDB reopened; require(reopened.open(dir, opt), "reopen rotation");
  assert(reopened.node_count() == 199);
  assert(reopened.metadata_search("kind", "soakable").size() == 199);
  require(reopened.close(), "close reopened rotation");

  std::cout << "rc_lock_rotation_metadata_gate_passed=true\n";
  return 0;
}
