#include "graphene/db.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static std::vector<float> random_vec(uint32_t dim, uint32_t seed) {
  std::mt19937 rng(seed);
  std::normal_distribution<float> dist(0.0f, 1.0f);
  std::vector<float> out(dim);
  for (auto& v : out) v = dist(rng);
  return out;
}

static double percentile(std::vector<double> values, double p) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  size_t idx = static_cast<size_t>((p / 100.0) * static_cast<double>(values.size() - 1));
  return values[idx];
}

int main(int argc, char** argv) {
  uint32_t incidents = argc > 1 ? static_cast<uint32_t>(std::stoul(argv[1])) : 5000;
  uint32_t queries = argc > 2 ? static_cast<uint32_t>(std::stoul(argv[2])) : 200;
  uint32_t dim = argc > 3 ? static_cast<uint32_t>(std::stoul(argv[3])) : 64;

  fs::path dir = fs::temp_directory_path() / "graphenedb_vector_baseline_comparison";
  fs::remove_all(dir);

  DBOptions opt;
  opt.dimension = dim;
  opt.fsync_on_commit = false;
  opt.vector_index_kind = VectorIndexKind::Flat;

  GrapheneDB db;
  auto st = db.open(dir, opt);
  if (!st) { std::cerr << st.message << "\n"; return 2; }

  std::vector<uint32_t> roots;
  std::vector<uint32_t> symptoms;
  roots.reserve(incidents);
  symptoms.reserve(incidents);

  auto ingest_start = Clock::now();
  for (uint32_t i = 0; i < incidents; ++i) {
    auto sig = signature_for(i % 16, (i * 7) % 16);
    auto base = random_vec(dim, 100000 + i);
    auto symptom_vec = base;
    if (!symptom_vec.empty()) symptom_vec[0] += 0.01f;

    NodeInput root;
    root.content = "incident root " + std::to_string(i);
    root.vector = random_vec(dim, 200000 + i);
    root.signature = sig;
    root.incident = i;
    root.root = true;
    root.metadata["role"] = "root";

    NodeInput distractor;
    distractor.content = "vector-similar distractor " + std::to_string(i);
    distractor.vector = symptom_vec;
    distractor.signature = signature_for((i + 3) % 16, (i + 11) % 16);
    distractor.incident = i + incidents;
    distractor.metadata["role"] = "distractor";

    NodeInput symptom;
    symptom.content = "incident symptom " + std::to_string(i);
    symptom.vector = base;
    symptom.signature = sig;
    symptom.incident = i;
    symptom.symptom = true;
    symptom.metadata["role"] = "symptom";

    uint32_t rid = 0, did = 0, sid = 0;
    st = db.put_node(root, &rid);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    st = db.put_node(distractor, &did);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    st = db.put_node(symptom, &sid);
    if (!st) { std::cerr << st.message << "\n"; return 2; }

    EdgeInput e{rid, sid, EdgeOrigin::Observed, EdgeRole::Causal, 0.95};
    st = db.put_edge(e);
    if (!st) { std::cerr << st.message << "\n"; return 2; }

    roots.push_back(rid);
    symptoms.push_back(sid);
    (void)did;
  }
  auto ingest_end = Clock::now();

  std::vector<double> vector_ms;
  std::vector<double> causal_ms;
  uint32_t vector_root_hits = 0;
  uint32_t causal_root_hits = 0;

  for (uint32_t q = 0; q < queries; ++q) {
    uint32_t inc = (q * 7919) % incidents;
    auto symptom = db.get_node(symptoms[inc]).value();

    auto v0 = Clock::now();
    auto vector = db.vector_search(symptom.vector, 1);
    auto v1 = Clock::now();
    auto bundle = db.causal_search(symptom.vector, symptom.signature, QueryMode::Empirical);
    auto v2 = Clock::now();

    if (!vector.empty() && vector.front().node_id == roots[inc]) ++vector_root_hits;
    if (!bundle.abstain && bundle.target_node == roots[inc]) ++causal_root_hits;
    vector_ms.push_back(std::chrono::duration<double, std::milli>(v1 - v0).count());
    causal_ms.push_back(std::chrono::duration<double, std::milli>(v2 - v1).count());
  }

  double ingest_ms = std::chrono::duration<double, std::milli>(ingest_end - ingest_start).count();
  std::cout << "vector_baseline_comparison=true\n";
  std::cout << "incidents=" << incidents << " nodes=" << db.node_count() << " edges=" << db.edge_count() << " dim=" << dim << " queries=" << queries << "\n";
  std::cout << "ingest_ms=" << ingest_ms << "\n";
  std::cout << "vector_root_hit_rate=" << (static_cast<double>(vector_root_hits) / queries) << "\n";
  std::cout << "causal_root_hit_rate=" << (static_cast<double>(causal_root_hits) / queries) << "\n";
  std::cout << "vector_p95_ms=" << percentile(vector_ms, 95) << "\n";
  std::cout << "causal_p95_ms=" << percentile(causal_ms, 95) << "\n";

  db.close();
  return 0;
}
