#include "graphene/db.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <random>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static std::vector<float> vec(uint32_t d, float base) { std::vector<float> v(d); for (uint32_t i=0;i<d;i++) v[i]=base+0.001f*i; return v; }
static int get_arg(int argc, char** argv, const std::string& key, int def) { for (int i=1;i+1<argc;i++) if (argv[i] == key) return std::stoi(argv[i+1]); return def; }
static void require(Status st, const char* what) { if (!st) { std::cerr << "FAIL " << what << ": " << st.message << "\n"; std::abort(); } }

int main(int argc, char** argv) {
  int seconds = get_arg(argc, argv, "--seconds", 3);
  uint32_t D = static_cast<uint32_t>(get_arg(argc, argv, "--dim", 16));
  fs::path dir = fs::temp_directory_path() / "graphenedb_rc_soak";
  fs::remove_all(dir);
  DBOptions opt; opt.dimension = D; opt.fsync_on_commit = false; opt.wal_rotate_bytes = 1 << 20;
  GrapheneDB db; require(db.open(dir, opt), "open soak");
  std::vector<uint32_t> ids;
  int writes=0, searches=0, deletes=0, compactions=0;
  auto end = Clock::now() + std::chrono::seconds(seconds);
  for (int i=0; Clock::now() < end; ++i) {
    NodeInput n;
    n.content = "soak memory " + std::to_string(i);
    n.vector = vec(D, 0.4f + (i%13)*0.01f);
    n.signature = signature_for(static_cast<uint32_t>(i%16), static_cast<uint32_t>(i%16));
    n.incident = static_cast<uint32_t>(i);
    n.root = (i % 3 == 0); n.symptom = (i % 3 == 2);
    n.metadata["soak"] = "true";
    uint32_t id; require(db.put_node(n, &id), "soak put"); ids.push_back(id); writes++;
    if (ids.size() > 2 && i % 3 == 2) db.put_edge({ids[ids.size()-3], ids.back(), EdgeOrigin::Observed, EdgeRole::Causal, 0.9});
    if (i % 10 == 0) { auto r = db.vector_search(n.vector, 5); assert(!r.empty()); searches++; }
    if (i % 97 == 0 && ids.size() > 50) { auto st = db.delete_node(ids[ids.size()/3]); if (st) deletes++; }
    if (i % 997 == 0 && i > 0) { require(db.compact(), "soak compact"); compactions++; }
  }
  std::string report; require(db.validate(&report), "soak validate");
  require(db.close(), "soak close");
  GrapheneDB reopened; require(reopened.open(dir, opt), "soak reopen");
  require(reopened.validate(&report), "soak reopened validate");
  std::cout << "rc_soak_gate_passed=true\n";
  std::cout << "seconds=" << seconds << " writes=" << writes << " searches=" << searches << " deletes=" << deletes << " compactions=" << compactions << " nodes=" << reopened.node_count() << " edges=" << reopened.edge_count() << "\n";
  require(reopened.close(), "soak reopened close");
  return 0;
}
