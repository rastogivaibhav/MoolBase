#include "graphene/db.hpp"
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>

using namespace graphene;
namespace fs = std::filesystem;

static uint64_t fnv1a_test(const std::string& s) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : s) { h ^= c; h *= 1099511628211ull; }
  return h;
}
static std::string frame(const std::string& payload) { return std::to_string(payload.size()) + "|" + std::to_string(fnv1a_test(payload)) + "|" + payload + "\n"; }
static std::vector<float> vec(uint32_t d, float base=0.2f) { std::vector<float> v(d); for (uint32_t i=0;i<d;i++) v[i]=base+i*0.001f; return v; }
static NodeInput node(uint32_t d) { NodeInput n; n.content="fuzz-base"; n.vector=vec(d); n.signature=signature_for(1,1); n.incident=1; return n; }
static void require(Status st, const char* what) { if (!st) { std::cerr << "FAIL " << what << ": " << st.message << "\n"; std::abort(); } }

int main() {
  const uint32_t D = 16;
  DBOptions opt; opt.dimension = D; opt.fsync_on_commit = false;

  // Bad open options are controlled.
  {
    GrapheneDB db; DBOptions bad; bad.dimension = 0;
    auto st = db.open(fs::temp_directory_path() / "graphenedb_rc_fuzz_dim0", bad);
    assert(!st && st.code == ErrorCode::InvalidOption);
  }

  fs::path dir = fs::temp_directory_path() / "graphenedb_rc_fuzz_inputs";
  fs::remove_all(dir);
  GrapheneDB db; require(db.open(dir, opt), "open fuzz inputs");
  uint32_t id = 0; require(db.put_node(node(D), &id), "put base");

  // Input fuzz: wrong dimensions, NaN, infinity, bad edge endpoints/confidence, deleted endpoint.
  NodeInput wrong = node(D); wrong.vector.push_back(1.0f);
  auto st = db.put_node(wrong); assert(!st && st.code == ErrorCode::DimensionMismatch);
  NodeInput nan = node(D); nan.vector[3] = std::numeric_limits<float>::quiet_NaN();
  st = db.put_node(nan); assert(!st && st.code == ErrorCode::DimensionMismatch);
  NodeInput inf = node(D); inf.vector[2] = std::numeric_limits<float>::infinity();
  st = db.put_node(inf); assert(!st && st.code == ErrorCode::DimensionMismatch);
  st = db.put_edge({id, 999999, EdgeOrigin::Observed, EdgeRole::Causal, 0.9}); assert(!st && st.code == ErrorCode::EdgeInvalid);
  st = db.put_edge({id, id, EdgeOrigin::Observed, EdgeRole::Causal, std::numeric_limits<double>::quiet_NaN()}); assert(!st && st.code == ErrorCode::InvalidInput);
  st = db.put_edge({id, id, EdgeOrigin::Observed, EdgeRole::Causal, 1.5}); assert(!st && st.code == ErrorCode::InvalidInput);
  auto bad_vec_results = db.vector_search(std::vector<float>(D+7, 1.0f), 10); assert(bad_vec_results.empty());
  auto bad_bundle = db.causal_search(std::vector<float>(D+1, 1.0f), signature_for(1,1)); assert(bad_bundle.abstain);
  require(db.delete_node(id), "delete base");
  st = db.put_edge({id, id, EdgeOrigin::Observed, EdgeRole::Causal, 0.9}); assert(!st && st.code == ErrorCode::EdgeInvalid);
  require(db.close(), "close fuzz inputs");

  // WAL fuzz: random torn/corrupt tails must not crash and must preserve compacted data state.
  fs::path walbase = fs::temp_directory_path() / "graphenedb_rc_fuzz_wal";
  fs::remove_all(walbase);
  {
    GrapheneDB seed; require(seed.open(walbase, opt), "open seed");
    uint32_t keep; require(seed.put_node(node(D), &keep), "put keep");
    require(seed.compact(), "compact seed");
    require(seed.close(), "close seed");
  }
  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> len_dist(1, 128);
  std::uniform_int_distribution<int> char_dist(32, 126);
  for (int i=0; i<100; ++i) {
    fs::path case_dir = fs::temp_directory_path() / ("graphenedb_rc_fuzz_wal_case_" + std::to_string(i));
    fs::remove_all(case_dir);
    fs::create_directories(case_dir);
    fs::copy_file(walbase / "graphene.data", case_dir / "graphene.data");
    fs::copy_file(walbase / "MANIFEST", case_dir / "MANIFEST");
    {
      std::ofstream wal(case_dir / "graphene.wal", std::ios::binary | std::ios::trunc);
      int len = len_dist(rng);
      for (int j=0; j<len; ++j) wal << static_cast<char>(char_dist(rng));
      if (i % 7 == 0) wal << "\n" << frame("BEGIN\t900") << frame("BROKEN\t900");
    }
    GrapheneDB check;
    auto ost = check.open(case_dir, opt);
    // Random raw garbage at the WAL tail may be ignored; valid but semantically bad frames may fail controlled.
    if (ost) {
      assert(check.node_count() == 1);
      require(check.close(), "close fuzz wal case");
    } else {
      assert(ost.code == ErrorCode::DataCorrupt || ost.code == ErrorCode::WalCorrupt || ost.code == ErrorCode::TransactionIncomplete);
    }
  }

  std::cout << "rc_fuzz_gate_passed=true\n";
  return 0;
}
