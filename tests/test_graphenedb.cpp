#include "graphene/db.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <iostream>
#include <random>
#include <thread>

using namespace graphene;
namespace fs = std::filesystem;


static uint64_t test_fnv1a(const std::string& x) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : x) { h ^= c; h *= 1099511628211ull; }
  return h;
}

static std::string frame(const std::string& payload) {
  return std::to_string(payload.size()) + "|" + std::to_string(test_fnv1a(payload)) + "|" + payload + "\n";
}

static std::vector<float> v(uint32_t d, float base) {
  std::vector<float> out(d);
  for (uint32_t i = 0; i < d; ++i) out[i] = base + static_cast<float>(i) * 0.001f;
  return out;
}

static NodeInput node(std::string text, uint32_t d, float base, uint64_t sig, uint32_t incident, bool root=false, bool symptom=false) {
  NodeInput n;
  n.content = std::move(text);
  n.vector = v(d, base);
  n.signature = sig;
  n.incident = incident;
  n.root = root;
  n.symptom = symptom;
  n.metadata["source"] = "test";
  return n;
}

static void require(Status st, const char* msg) {
  if (!st) {
    std::cerr << "FAIL: " << msg << ": " << st.message << "\n";
    std::abort();
  }
}

int main() {
  const uint32_t D = 8;
  fs::path base = fs::temp_directory_path() / "graphenedb_v1_tests";
  fs::remove_all(base);

  // Basic durability: content, vector and metadata must survive close/open.
  {
    GrapheneDB db;
    DBOptions opt; opt.dimension = D;
    require(db.open(base, opt), "open");
    uint64_t sig = signature_for(1, 2);
    uint32_t root=0, dep=0, sym=0;
    require(db.put_node(node("root cause ingress", D, 0.1f, sig, 42, true, false), &root), "put root");
    require(db.put_node(node("dependency gateway", D, 0.2f, sig, 42), &dep), "put dep");
    require(db.put_node(node("symptom checkout timeout", D, 0.2f, sig, 42, false, true), &sym), "put sym");
    EdgeInput e1{root, dep, EdgeOrigin::Observed, EdgeRole::Causal, 0.95};
    EdgeInput e2{dep, sym, EdgeOrigin::Discovered, EdgeRole::Causal, 0.93};
    require(db.put_edge(e1), "put edge 1");
    require(db.put_edge(e2), "put edge 2");
    auto b = db.causal_search(v(D, 0.2f), sig, QueryMode::Empirical);
    assert(!b.abstain);
    assert(b.target_node == root);
    require(db.close(), "close");
  }
  {
    GrapheneDB db;
    DBOptions opt; opt.dimension = D;
    require(db.open(base, opt), "reopen");
    auto n = db.get_node(0);
    assert(n.has_value());
    assert(n->content == "root cause ingress");
    assert(n->vector.size() == D);
    assert(n->metadata.at("source") == "test");
    assert(db.node_count() == 3);
    assert(db.edge_count() == 2);
    require(db.close(), "close 2");
  }

  // Dimension mismatch rejected, not memory-unsafe.
  {
    GrapheneDB db;
    DBOptions opt; opt.dimension = D;
    require(db.open(base, opt), "open dim check");
    NodeInput bad = node("bad", D, 0.0f, signature_for(1,1), 1);
    bad.vector.push_back(99.0f);
    auto st = db.put_node(bad);
    assert(!st && st.code == ErrorCode::DimensionMismatch);
    auto results = db.vector_search(std::vector<float>(D + 1, 1.0f), 10);
    assert(results.empty());
    require(db.close(), "close dim");
  }

  // Invalid edge rejected.
  {
    GrapheneDB db;
    DBOptions opt; opt.dimension = D;
    require(db.open(base, opt), "open edge check");
    EdgeInput bad{0, 999999, EdgeOrigin::Observed, EdgeRole::Causal, 0.9};
    auto st = db.put_edge(bad);
    assert(!st && st.code == ErrorCode::EdgeInvalid);
    require(db.close(), "close edge");
  }

  // Delete snapshot behavior and compact.
  {
    GrapheneDB db;
    DBOptions opt; opt.dimension = D;
    require(db.open(base, opt), "open delete");
    auto before = db.snapshot();
    require(db.delete_node(2), "delete node");
    assert(db.get_node(2).has_value() == false);
    assert(db.get_node(2, before).has_value() == true);
    require(db.compact(), "compact");
    std::string report;
    require(db.validate(&report), "validate");
    require(db.close(), "close compact");
  }

  // Reopen after compact: deleted node should stay deleted; live data survives.
  {
    GrapheneDB db;
    DBOptions opt; opt.dimension = D;
    require(db.open(base, opt), "reopen compact");
    assert(db.node_count() == 2);
    assert(db.edge_count() == 1); // edge to deleted node no longer visible after compact.
    require(db.close(), "close reopen compact");
  }

  // Torn WAL frame ignored safely.
  {
    std::ofstream wal(base / "graphene.wal", std::ios::app);
    wal << "999|123|TORN";
  }
  {
    GrapheneDB db;
    DBOptions opt; opt.dimension = D;
    require(db.open(base, opt), "open torn");
    assert(db.node_count() == 2);
    require(db.close(), "close torn");
  }


  // Valid but uncommitted WAL transaction ignored safely.
  {
    std::ofstream wal(base / "graphene.wal", std::ios::app);
    wal << frame("BEGIN\t777");
    wal << frame("PUT_NODE\t777\t999\t999\t18446744073709551615\t65538\t77\t0\t1\t0\t756e636f6d6d6974746564\t0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8\t");
  }
  {
    GrapheneDB db;
    DBOptions opt; opt.dimension = D;
    require(db.open(base, opt), "open uncommitted");
    assert(db.get_node(999).has_value() == false);
    assert(db.node_count() == 2);
    require(db.close(), "close uncommitted");
  }

  // Lock prevents two concurrent writable opens.
  {
    GrapheneDB db1, db2;
    DBOptions opt; opt.dimension = D;
    require(db1.open(base, opt), "open lock first");
    auto st = db2.open(base, opt);
    assert(!st && st.code == ErrorCode::LockBusy);
    require(db1.close(), "close lock first");
  }

  // Backup produces a re-openable copy.
  {
    GrapheneDB db;
    DBOptions opt; opt.dimension = D;
    require(db.open(base, opt), "open backup source");
    fs::path backup = fs::temp_directory_path() / "graphenedb_v1_backup";
    fs::remove_all(backup);
    require(db.backup(backup), "backup");
    require(db.close(), "close backup source");
    GrapheneDB copy;
    require(copy.open(backup, opt), "open backup copy");
    assert(copy.node_count() == 2);
    require(copy.close(), "close backup copy");
  }

  // Vector index policy is explicit and inspectable.
  {
    auto run_index_case = [&](VectorIndexKind kind, const char* expected_name, const fs::path& dir) {
      fs::remove_all(dir);
      GrapheneDB indexed;
      DBOptions opt; opt.dimension = D; opt.vector_index_kind = kind; opt.fsync_on_commit = false;
      require(indexed.open(dir, opt), "open vector index case");
      uint32_t expected = 0;
      for (int i = 0; i < 6; ++i) {
        auto n = node("indexed node " + std::to_string(i), D, 0.05f * static_cast<float>(i + 1), signature_for(3, i), static_cast<uint32_t>(i));
        require(indexed.put_node(n, i == 3 ? &expected : nullptr), "put indexed node");
      }
      auto results = indexed.vector_search(v(D, 0.2f), 3);
      assert(!results.empty());
      assert(results.front().node_id == expected);
      std::string out;
      require(indexed.inspect(&out), "inspect vector index case");
      assert(out.find(std::string("vector_index=") + expected_name) != std::string::npos);
      require(indexed.close(), "close vector index case");
    };
    run_index_case(VectorIndexKind::Flat, "flat", fs::temp_directory_path() / "graphenedb_v1_flat_index");
    run_index_case(VectorIndexKind::KDTree, "kdtree", fs::temp_directory_path() / "graphenedb_v1_kdtree_index");
#ifdef GRAPHENEDB_HAS_FAISS
    run_index_case(VectorIndexKind::Faiss, "faiss", fs::temp_directory_path() / "graphenedb_v1_faiss_index");
#else
    {
      fs::path faiss = fs::temp_directory_path() / "graphenedb_v1_faiss_unavailable";
      fs::remove_all(faiss);
      GrapheneDB indexed;
      DBOptions opt; opt.dimension = D; opt.vector_index_kind = VectorIndexKind::Faiss;
      auto st = indexed.open(faiss, opt);
      assert(!st && st.code == ErrorCode::UnsupportedMode);
    }
#endif

    fs::path high = fs::temp_directory_path() / "graphenedb_v1_auto_high_index";
    fs::remove_all(high);
    GrapheneDB indexed;
    DBOptions opt; opt.dimension = 64; opt.vector_index_kind = VectorIndexKind::Auto;
    require(indexed.open(high, opt), "open auto high index");
    std::string out;
    require(indexed.inspect(&out), "inspect auto high index");
    assert(out.find("vector_index_requested=auto") != std::string::npos);
    assert(out.find("vector_index=flat") != std::string::npos);
    require(indexed.close(), "close auto high index");
  }

  // Concurrent readers/writers through coarse DB lock.
  fs::path conc = fs::temp_directory_path() / "graphenedb_v1_concurrency";
  fs::remove_all(conc);
  GrapheneDB db;
  DBOptions opt; opt.dimension = D; opt.fsync_on_commit = false;
  require(db.open(conc, opt), "open concurrency");
  std::atomic<int> writes{0}, reads{0}, errors{0};
  std::vector<std::thread> threads;
  for (int t=0;t<4;t++) {
    threads.emplace_back([&, t] {
      for (int i=0;i<100;i++) {
        NodeInput n = node("cw", D, 0.01f * (i+1), signature_for(t, i), i + t*1000, i%20==0, i%20==1);
        if (db.put_node(n)) writes++; else errors++;
      }
    });
  }
  for (int t=0;t<4;t++) {
    threads.emplace_back([&] {
      for (int i=0;i<200;i++) {
        auto r = db.vector_search(v(D, 0.5f), 10);
        (void)r;
        reads++;
      }
    });
  }
  for (auto& th : threads) th.join();
  assert(errors == 0);
  assert(writes == 400);
  assert(reads == 800);
  require(db.validate(), "validate concurrency");
  require(db.close(), "close concurrency");

  std::cout << "graphenedb_tests_passed=true\n";
  return 0;
}
