#include "graphene/db.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <numeric>
#include <random>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::high_resolution_clock;

static std::vector<float> make_vec(uint32_t d, int seed) {
  std::mt19937 rng(seed);
  std::normal_distribution<float> n(0.0f, 1.0f);
  std::vector<float> v(d);
  for (auto& x : v) x = n(rng);
  return v;
}

static double p95(std::vector<double> v) {
  if (v.empty()) return 0;
  std::sort(v.begin(), v.end());
  return v[static_cast<size_t>(0.95 * (v.size() - 1))];
}

int main(int argc, char** argv) {
  int incidents = argc > 1 ? std::stoi(argv[1]) : 5000;
  uint32_t D = argc > 2 ? static_cast<uint32_t>(std::stoul(argv[2])) : 64;
  fs::path dir = fs::temp_directory_path() / "graphenedb_v1_bench";
  fs::remove_all(dir);
  GrapheneDB db;
  DBOptions opt; opt.dimension = D; opt.fsync_on_commit = false;
  auto st = db.open(dir, opt);
  if (!st) { std::cerr << st.message << "\n"; return 2; }
  std::vector<uint32_t> roots, syms;
  auto t0 = Clock::now();
  for (int i=0;i<incidents;i++) {
    auto sig = signature_for(i % 16, i % 16);
    NodeInput r{"root", make_vec(D, 10000+i), sig, static_cast<uint32_t>(i), true, false, false, {{"type","root"}}};
    NodeInput d{"dependency", make_vec(D, 20000+i), sig, static_cast<uint32_t>(i), false, false, false, {{"type","dep"}}};
    NodeInput s{"symptom checkout timeout", make_vec(D, 30000+i), sig, static_cast<uint32_t>(i), false, true, false, {{"type","symptom"}}};
    uint32_t rid,did,sid;
    db.put_node(r,&rid); db.put_node(d,&did); db.put_node(s,&sid);
    db.put_edge({rid,did,EdgeOrigin::Observed,EdgeRole::Causal,0.95});
    db.put_edge({did,sid,EdgeOrigin::Discovered,EdgeRole::Causal,0.92});
    roots.push_back(rid); syms.push_back(sid);
  }
  auto t1 = Clock::now();
  std::vector<double> tv, tc;
  int hits = 0;
  for (int q=0;q<200;q++) {
    int inc = (q * 41) % incidents;
    auto n = db.get_node(syms[inc]).value();
    auto a = Clock::now();
    auto flat = db.vector_search(n.vector, 10);
    auto b = Clock::now();
    auto bundle = db.causal_search(n.vector, n.signature, QueryMode::Empirical);
    auto c = Clock::now();
    tv.push_back(std::chrono::duration<double, std::milli>(b-a).count());
    tc.push_back(std::chrono::duration<double, std::milli>(c-b).count());
    if (!bundle.abstain && bundle.target_node == roots[inc]) hits++;
  }
  auto ingest_ms = std::chrono::duration<double, std::milli>(t1-t0).count();
  std::cout << "nodes=" << db.node_count() << " edges=" << db.edge_count() << "\n";
  std::cout << "ingest_ms=" << ingest_ms << "\n";
  std::cout << "vector_p95_ms=" << p95(tv) << " causal_p95_ms=" << p95(tc) << " causal_hit_rate=" << (double)hits/200.0 << "\n";
  db.close();
  return 0;
}
