#include "graphene/db.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <unordered_set>
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

static VectorIndexKind parse_kind(const std::string& s) {
  if (s == "auto") return VectorIndexKind::Auto;
  if (s == "flat") return VectorIndexKind::Flat;
  if (s == "kdtree") return VectorIndexKind::KDTree;
  if (s == "faiss") return VectorIndexKind::Faiss;
  throw std::invalid_argument("unknown vector index kind: " + s);
}

static double percentile(std::vector<double> values, double p) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  size_t idx = static_cast<size_t>((p / 100.0) * static_cast<double>(values.size() - 1));
  return values[idx];
}

static void require(Status st, const char* msg) {
  if (!st) {
    std::cerr << msg << ": " << st.message << "\n";
    std::exit(2);
  }
}

static void populate(GrapheneDB& db, uint32_t nodes, uint32_t dim) {
  for (uint32_t i = 0; i < nodes; ++i) {
    NodeInput n;
    n.content = "vector recall node " + std::to_string(i);
    n.vector = random_vec(dim, 1000000 + i);
    n.signature = signature_for(i % 16, (i * 13) % 16);
    n.incident = i / 4;
    n.root = (i % 4) == 0;
    n.metadata["recall_bucket"] = std::to_string(i % 17);
    require(db.put_node(n), "put recall node");
  }
}

int main(int argc, char** argv) {
  uint32_t nodes = argc > 1 ? static_cast<uint32_t>(std::stoul(argv[1])) : 5000;
  uint32_t queries = argc > 2 ? static_cast<uint32_t>(std::stoul(argv[2])) : 200;
  uint32_t dim = argc > 3 ? static_cast<uint32_t>(std::stoul(argv[3])) : 32;
  size_t k = argc > 4 ? static_cast<size_t>(std::stoul(argv[4])) : 10;
  std::string candidate_name = argc > 5 ? argv[5] : "auto";
  double min_recall = argc > 6 ? std::stod(argv[6]) : 0.999;

  fs::path exact_dir = fs::temp_directory_path() / "graphenedb_vector_recall_exact";
  fs::path candidate_dir = fs::temp_directory_path() / "graphenedb_vector_recall_candidate";
  fs::remove_all(exact_dir);
  fs::remove_all(candidate_dir);

  DBOptions exact_opt;
  exact_opt.dimension = dim;
  exact_opt.fsync_on_commit = false;
  exact_opt.vector_index_kind = VectorIndexKind::Flat;

  DBOptions candidate_opt = exact_opt;
  candidate_opt.vector_index_kind = parse_kind(candidate_name);

  GrapheneDB exact;
  GrapheneDB candidate;
  require(exact.open(exact_dir, exact_opt), "open exact db");
  require(candidate.open(candidate_dir, candidate_opt), "open candidate db");

  populate(exact, nodes, dim);
  populate(candidate, nodes, dim);

  std::vector<double> exact_ms;
  std::vector<double> candidate_ms;
  double recall_sum = 0.0;
  double worst_recall = 1.0;

  for (uint32_t q = 0; q < queries; ++q) {
    auto query = random_vec(dim, 2000000 + q * 7919);

    auto t0 = Clock::now();
    auto expected = exact.vector_search(query, k);
    auto t1 = Clock::now();
    auto actual = candidate.vector_search(query, k);
    auto t2 = Clock::now();

    std::unordered_set<uint32_t> expected_ids;
    for (const auto& r : expected) expected_ids.insert(r.node_id);
    uint32_t hits = 0;
    for (const auto& r : actual) {
      if (expected_ids.count(r.node_id)) ++hits;
    }

    double denom = static_cast<double>(std::max<size_t>(1, expected_ids.size()));
    double recall = static_cast<double>(hits) / denom;
    recall_sum += recall;
    worst_recall = std::min(worst_recall, recall);
    exact_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    candidate_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
  }

  std::string inspect;
  require(candidate.inspect(&inspect), "inspect candidate");

  double mean_recall = queries == 0 ? 0.0 : recall_sum / static_cast<double>(queries);
  std::cout << "vector_index_recall_benchmark=true\n";
  std::cout << "nodes=" << nodes << " queries=" << queries << " dim=" << dim << " k=" << k << "\n";
  std::cout << "candidate_requested=" << candidate_name << "\n";
  for (const auto& line : {"vector_index_requested=", "vector_index="}) {
    auto pos = inspect.find(line);
    if (pos != std::string::npos) {
      auto end = inspect.find('\n', pos);
      std::cout << inspect.substr(pos, end == std::string::npos ? std::string::npos : end - pos) << "\n";
    }
  }
  std::cout << "mean_recall_at_k=" << mean_recall << "\n";
  std::cout << "worst_recall_at_k=" << worst_recall << "\n";
  std::cout << "exact_p95_ms=" << percentile(exact_ms, 95) << "\n";
  std::cout << "candidate_p95_ms=" << percentile(candidate_ms, 95) << "\n";

  require(exact.close(), "close exact db");
  require(candidate.close(), "close candidate db");
  fs::remove_all(exact_dir);
  fs::remove_all(candidate_dir);

  if (mean_recall < min_recall) {
    std::cerr << "mean recall " << mean_recall << " below threshold " << min_recall << "\n";
    return 3;
  }
  return 0;
}
