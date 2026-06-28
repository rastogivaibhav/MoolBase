#include "graphene/db.hpp"
#include <cassert>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <iostream>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace graphene;
namespace fs = std::filesystem;

static std::vector<float> vec(uint32_t d, float base) { std::vector<float> v(d); for (uint32_t i=0;i<d;i++) v[i]=base+0.001f*i; return v; }
static void require(Status st, const char* what) { if (!st) { std::cerr << "FAIL " << what << ": " << st.message << "\n"; std::abort(); } }

static void child_writer(fs::path dir, int mode) {
  DBOptions opt; opt.dimension = 8; opt.fsync_on_commit = false; opt.wal_rotate_bytes = (mode == 1 ? 8192 : 0); opt.recover_stale_lock = true;
  GrapheneDB db; auto st = db.open(dir, opt); if (!st) _exit(11);
  uint32_t prev = UINT32_MAX;
  for (int i=0; i<200000; ++i) {
    NodeInput n;
    n.content = "kill-matrix-node-" + std::to_string(mode) + "-" + std::to_string(i);
    n.vector = vec(8, 0.3f + (i % 5) * 0.01f);
    n.signature = signature_for(static_cast<uint32_t>(i%8), static_cast<uint32_t>(i%8));
    n.incident = static_cast<uint32_t>(i);
    n.metadata["mode"] = std::to_string(mode);
    uint32_t id = 0;
    if (!db.put_node(n, &id)) _exit(12);
    if (prev != UINT32_MAX && i % 3 == 0) db.put_edge({prev, id, EdgeOrigin::Observed, EdgeRole::Causal, 0.8});
    prev = id;
    if (mode == 2 && i % 100 == 0) db.compact();
  }
  db.close();
  _exit(0);
}

static void run_kill_case(int mode) {
  fs::path dir = fs::temp_directory_path() / ("graphenedb_rc_process_kill_mode_" + std::to_string(mode));
  fs::remove_all(dir);
  pid_t pid = fork();
  if (pid == 0) child_writer(dir, mode);
  assert(pid > 0);
  std::this_thread::sleep_for(std::chrono::milliseconds(35 + 25 * mode));
  kill(pid, SIGKILL);
  int status = 0; waitpid(pid, &status, 0);

  DBOptions opt; opt.dimension = 8; opt.fsync_on_commit = false; opt.wal_rotate_bytes = (mode == 1 ? 8192 : 0); opt.recover_stale_lock = true;
  GrapheneDB db; auto st = db.open(dir, opt);
  assert(st && "DB must recover after process kill and stale LOCK");
  std::string report; require(db.validate(&report), "validate after process kill");
  assert(db.node_count() <= 200000);
  auto res = db.metadata_search("mode", std::to_string(mode));
  assert(res.size() == db.node_count());
  require(db.close(), "close recovered kill case");
}

int main() {
  run_kill_case(0); // kill during normal WAL append workload
  run_kill_case(1); // kill while WAL rotation/checkpoint is enabled
  run_kill_case(2); // kill while repeated explicit compaction is occurring
  std::cout << "rc_process_kill_matrix_gate_passed=true\n";
  return 0;
}
