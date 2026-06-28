#include "graphene/db.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::high_resolution_clock;

static std::vector<float> make_vec(uint32_t d, int seed) {
  std::vector<float> v(d);
  uint32_t x = static_cast<uint32_t>(seed * 2654435761u);
  for (uint32_t i = 0; i < d; ++i) {
    x = x * 1664525u + 1013904223u;
    v[i] = static_cast<float>((x % 10000) + 1) / 10000.0f;
  }
  return v;
}
static double percentile(std::vector<double> v, double pct) {
  if (v.empty()) return 0.0;
  std::sort(v.begin(), v.end());
  size_t idx = static_cast<size_t>(pct * static_cast<double>(v.size() - 1));
  return v[idx];
}
static int get_arg(int argc, char** argv, const std::string& key, int def) {
  for (int i=1;i+1<argc;i++) if (argv[i] == key) return std::stoi(argv[i+1]);
  return def;
}
static void require(Status st, const char* what) { if (!st) { std::cerr << "FAIL " << what << ": " << st.message << "\n"; std::abort(); } }

int main(int argc, char** argv) {
  int incidents = get_arg(argc, argv, "--incidents", 5000); // ctest smoke default; full gate uses 33334+.
  int queries = get_arg(argc, argv, "--queries", 200);
  uint32_t D = static_cast<uint32_t>(get_arg(argc, argv, "--dim", 64));
  bool reopen_check = get_arg(argc, argv, "--reopen", 1) != 0;
  fs::path dir = fs::temp_directory_path() / ("graphenedb_rc_stress_" + std::to_string(incidents) + "_" + std::to_string(D));
  fs::remove_all(dir);

  DBOptions opt; opt.dimension = D; opt.fsync_on_commit = false;
  GrapheneDB db; require(db.open(dir, opt), "open stress");
  std::vector<uint32_t> roots, syms;
  std::map<uint64_t, size_t> plane_node_counts;
  auto t0 = Clock::now();
  for (int i=0;i<incidents;i++) {
    auto sig = signature_for(static_cast<uint32_t>(i % 16), static_cast<uint32_t>(i % 16));
    NodeInput r{"root cause for incident " + std::to_string(i), make_vec(D, 100000+i), sig, static_cast<uint32_t>(i), true, false, false, {{"type","root"}}};
    NodeInput d{"dependency for incident " + std::to_string(i), make_vec(D, 200000+i), sig, static_cast<uint32_t>(i), false, false, false, {{"type","dependency"}}};
    NodeInput s{"symptom checkout timeout incident " + std::to_string(i), make_vec(D, 300000+i), sig, static_cast<uint32_t>(i), false, true, false, {{"type","symptom"}}};
    uint32_t rid,did,sid;
    require(db.put_node(r, &rid), "put root");
    require(db.put_node(d, &did), "put dep");
    require(db.put_node(s, &sid), "put symptom");
    require(db.put_edge({rid,did,EdgeOrigin::Observed,EdgeRole::Causal,0.95}), "edge root dep");
    require(db.put_edge({did,sid,EdgeOrigin::Discovered,EdgeRole::Causal,0.92}), "edge dep sym");
    roots.push_back(rid); syms.push_back(sid); plane_node_counts[sig] += 3;
  }
  auto t1 = Clock::now();
  if (incidents >= 100000) { std::cerr << "progress=ingest_done incidents=" << incidents << " nodes=" << db.node_count() << " edges=" << db.edge_count() << "\n"; }

  std::vector<double> flat_ms, causal_ms;
  int hits = 0;
  double candidate_pct_sum = 0.0;
  for (int q=0; q<queries; ++q) {
    int inc = (q * 7919) % incidents;
    auto n = db.get_node(syms[inc]).value();
    uint64_t query_sig = n.signature;
    size_t candidate_nodes = 0;
    int required = std::max(1, __builtin_popcountll(query_sig) - 1);
    for (const auto& kv : plane_node_counts) if (__builtin_popcountll(kv.first & query_sig) >= required) candidate_nodes += kv.second;
    candidate_pct_sum += static_cast<double>(candidate_nodes) / static_cast<double>(std::max<size_t>(1, db.node_count()));

    auto a = Clock::now();
    auto flat = db.vector_search(n.vector, 10);
    auto b = Clock::now();
    auto bundle = db.causal_search(n.vector, query_sig, QueryMode::Empirical);
    auto c = Clock::now();
    (void)flat;
    flat_ms.push_back(std::chrono::duration<double, std::milli>(b-a).count());
    causal_ms.push_back(std::chrono::duration<double, std::milli>(c-b).count());
    if (!bundle.abstain && bundle.target_node == roots[inc]) ++hits;
  }

  if (incidents >= 100000) { std::cerr << "progress=queries_done queries=" << queries << "\n"; }
  std::string validation;
  require(db.validate(&validation), "validate stress");
  if (incidents >= 100000) { std::cerr << "progress=validate_done\n"; }
  auto ingest_ms = std::chrono::duration<double, std::milli>(t1-t0).count();
  double hit_rate = static_cast<double>(hits) / std::max(1, queries);
  std::cout << "rc_stress_gate=true\n";
  std::cout << "incidents=" << incidents << " nodes=" << db.node_count() << " edges=" << db.edge_count() << " dim=" << D << "\n";
  std::cout << "ingest_ms=" << ingest_ms << " ingest_nodes_per_sec=" << (db.node_count()*1000.0/std::max(1.0,ingest_ms)) << "\n";
  std::cout << "vector_p50_ms=" << percentile(flat_ms,0.50) << " vector_p95_ms=" << percentile(flat_ms,0.95) << " vector_p99_ms=" << percentile(flat_ms,0.99) << "\n";
  std::cout << "causal_p50_ms=" << percentile(causal_ms,0.50) << " causal_p95_ms=" << percentile(causal_ms,0.95) << " causal_p99_ms=" << percentile(causal_ms,0.99) << "\n";
  std::cout << "causal_hit_rate=" << hit_rate << " avg_candidate_pct=" << (candidate_pct_sum/std::max(1,queries))*100.0 << "\n";
  assert(hit_rate >= 0.99);
  assert((candidate_pct_sum/std::max(1,queries)) < 0.10);
  require(db.close(), "close stress");
  if (incidents >= 100000) { std::cerr << "progress=close_done\n"; }

  if (reopen_check) {
    auto r0 = Clock::now();
    GrapheneDB reopened; require(reopened.open(dir, opt), "reopen stress");
    auto r1 = Clock::now();
    assert(reopened.node_count() == static_cast<size_t>(incidents) * 3);
    assert(reopened.edge_count() == static_cast<size_t>(incidents) * 2);
    std::cout << "reopen_ms=" << std::chrono::duration<double, std::milli>(r1-r0).count() << "\n";
    require(reopened.close(), "close reopened stress");
  }
  return 0;
}
