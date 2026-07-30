#include "graphene/dialectic.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <random>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {

std::vector<float> random_vector(uint32_t dimension, uint32_t seed) {
  std::mt19937 generator(seed);
  std::normal_distribution<float> distribution(0.0f, 1.0f);
  std::vector<float> result(dimension);
  for (float& value : result) value = distribution(generator);
  return result;
}

double percentile(std::vector<double> values, double requested) {
  if (values.empty()) return 0.0;
  std::sort(values.begin(), values.end());
  const size_t index = static_cast<size_t>(
      requested / 100.0 * static_cast<double>(values.size() - 1));
  return values[index];
}

bool contains_challenge(const OppositionReport& report,
                        const std::string& fragment) {
  return std::any_of(report.challenged_claims.begin(),
                     report.challenged_claims.end(),
                     [&](const std::string& challenge) {
                       return challenge.find(fragment) != std::string::npos;
                     });
}

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << operation << ": " << status.message << "\n";
    std::exit(2);
  }
}

} // namespace

int main(int argc, char** argv) {
  const uint32_t cases =
      argc > 1 ? static_cast<uint32_t>(std::stoul(argv[1])) : 12;
  const uint32_t dimension =
      argc > 2 ? static_cast<uint32_t>(std::stoul(argv[2])) : 32;
  if (cases == 0 || cases > 16 || dimension < 4) {
    std::cerr << "usage: bench_dialectic_ablation [cases 1..16] [dimension >=4]\n";
    return 2;
  }

  const fs::path directory =
      fs::temp_directory_path() / "graphenedb_dialectic_ablation";
  fs::remove_all(directory);
  GrapheneDB db;
  DBOptions options;
  options.dimension = dimension;
  options.fsync_on_commit = false;
  options.vector_index_kind = VectorIndexKind::Flat;
  require(db.open(directory, options), "open");

  std::vector<uint32_t> expected_roots;
  std::vector<uint32_t> symptoms;
  expected_roots.reserve(cases);
  symptoms.reserve(cases);

  for (uint32_t index = 0; index < cases; ++index) {
    const uint64_t signature = signature_for(index, 15 - index);
    const std::vector<float> query = random_vector(dimension, 1000 + index);

    NodeInput root;
    root.content = "expected root " + std::to_string(index);
    root.vector = random_vector(dimension, 2000 + index);
    root.signature = signature;
    root.incident = index;
    root.root = true;

    NodeInput dependency;
    dependency.content = "dependency " + std::to_string(index);
    dependency.vector = query;
    dependency.vector[0] += 0.05f;
    dependency.signature = signature;
    dependency.incident = index;

    NodeInput symptom;
    symptom.content = "symptom " + std::to_string(index);
    symptom.vector = query;
    symptom.signature = signature;
    symptom.incident = index;
    symptom.symptom = true;

    NodeInput alternative;
    alternative.content = "alternative root " + std::to_string(index);
    alternative.vector = random_vector(dimension, 3000 + index);
    alternative.signature = signature;
    alternative.incident = index;
    alternative.root = true;

    NodeInput contradicted;
    contradicted.content = "contradicted observation " + std::to_string(index);
    contradicted.vector = query;
    contradicted.vector[1] -= 0.05f;
    contradicted.signature = signature;
    contradicted.incident = index;

    uint32_t root_id = 0;
    uint32_t dependency_id = 0;
    uint32_t symptom_id = 0;
    uint32_t alternative_id = 0;
    uint32_t contradicted_id = 0;
    require(db.put_node(root, &root_id), "put root");
    require(db.put_node(dependency, &dependency_id), "put dependency");
    require(db.put_node(symptom, &symptom_id), "put symptom");
    require(db.put_node(alternative, &alternative_id), "put alternative");
    require(db.put_node(contradicted, &contradicted_id), "put contradiction");

    require(db.put_edge(
                {root_id, dependency_id, EdgeOrigin::Observed,
                 EdgeRole::Mechanistic, 0.98,
                 {{"source_id", "deployment-log-" + std::to_string(index)}}}),
            "put root dependency");
    require(db.put_edge(
                {dependency_id, symptom_id, EdgeOrigin::Discovered,
                 EdgeRole::Causal, 0.97,
                 {{"source_id", "trace-" + std::to_string(index)}}}),
            "put dependency symptom");
    require(db.put_edge(
                {alternative_id, symptom_id, EdgeOrigin::Observed,
                 EdgeRole::Causal, 0.45,
                 {{"source_id", "weak-signal-" + std::to_string(index)}}}),
            "put alternative");
    require(db.put_edge(
                {root_id, contradicted_id, EdgeOrigin::Observed,
                 EdgeRole::Contradicts, 0.90,
                 {{"source_id", "postmortem-" + std::to_string(index)}}}),
            "put contradiction");
    require(db.put_edge(
                {root_id, symptom_id, EdgeOrigin::Inferred,
                 EdgeRole::Compressed, 0.70, {}}),
            "put provenance-risk shortcut");

    expected_roots.push_back(root_id);
    symptoms.push_back(symptom_id);
  }

  uint32_t vector_hits = 0;
  uint32_t causal_hits = 0;
  uint32_t convergence_hits = 0;
  uint32_t dialectic_hits = 0;
  uint32_t contradiction_challenges = 0;
  uint32_t provenance_challenges = 0;
  uint32_t false_promotions = 0;
  std::vector<double> vector_latency;
  std::vector<double> causal_latency;
  std::vector<double> dialectic_latency;

  DialecticOptions dialectic_options;
  dialectic_options.mode = QueryMode::Balanced;
  dialectic_options.semantic_candidates = 4;
  dialectic_options.max_hops = 4;
  dialectic_options.max_paths = 24;
  dialectic_options.max_paths_per_root = 8;
  dialectic_options.max_opposition_rounds = 1;
  DialecticEngine engine(db);

  for (uint32_t index = 0; index < cases; ++index) {
    const Node symptom = *db.get_node(symptoms[index]);
    const auto vector_start = Clock::now();
    const auto vector_result = db.vector_search(symptom.vector, 1);
    const auto vector_end = Clock::now();
    const MemoryBundle causal = db.causal_search(
        symptom.vector, symptom.signature, QueryMode::Balanced);
    const auto causal_end = Clock::now();

    const BundleSet expanded =
        engine.expand(symptom.vector, symptom.signature, dialectic_options);
    const ConvergedAnswer convergence =
        engine.converge(expanded, dialectic_options);
    const auto convergence_end = Clock::now();
    const DialecticResult dialectic =
        engine.reason(symptom.vector, symptom.signature, dialectic_options);
    const auto dialectic_end = Clock::now();

    if (!vector_result.empty() &&
        vector_result.front().node_id == expected_roots[index]) {
      ++vector_hits;
    }
    if (!causal.abstain && causal.target_node == expected_roots[index]) {
      ++causal_hits;
    }
    if (convergence.has_answer &&
        convergence.primary_node == expected_roots[index]) {
      ++convergence_hits;
    }
    if (dialectic.synthesis.has_answer &&
        dialectic.synthesis.primary_node == expected_roots[index]) {
      ++dialectic_hits;
    }
    if (contains_challenge(dialectic.final_opposition, "contradiction")) {
      ++contradiction_challenges;
    }
    if (contains_challenge(dialectic.final_opposition, "provenance")) {
      ++provenance_challenges;
    }
    if (dialectic.synthesis.has_answer &&
        dialectic.synthesis.primary_node != expected_roots[index]) {
      ++false_promotions;
    }

    vector_latency.push_back(
        std::chrono::duration<double, std::milli>(vector_end - vector_start)
            .count());
    causal_latency.push_back(
        std::chrono::duration<double, std::milli>(causal_end - vector_end)
            .count());
    dialectic_latency.push_back(
        std::chrono::duration<double, std::milli>(dialectic_end -
                                                 convergence_end)
            .count());
  }

  const auto rate = [cases](uint32_t count) {
    return static_cast<double>(count) / static_cast<double>(cases);
  };
  std::cout << "dialectic_ablation_synthetic=true\n"
            << "claim_scope=deterministic_regression_not_production_evidence\n"
            << "cases=" << cases << " dimension=" << dimension << "\n"
            << "vector_root_hit_rate=" << rate(vector_hits) << "\n"
            << "causal_root_hit_rate=" << rate(causal_hits) << "\n"
            << "convergence_only_root_hit_rate=" << rate(convergence_hits)
            << "\n"
            << "dialectic_root_hit_rate=" << rate(dialectic_hits) << "\n"
            << "dialectic_contradiction_challenge_rate="
            << rate(contradiction_challenges) << "\n"
            << "dialectic_provenance_challenge_rate="
            << rate(provenance_challenges) << "\n"
            << "dialectic_false_promotion_rate=" << rate(false_promotions)
            << "\n"
            << "vector_p95_ms=" << percentile(vector_latency, 95) << "\n"
            << "causal_p95_ms=" << percentile(causal_latency, 95) << "\n"
            << "dialectic_p95_ms=" << percentile(dialectic_latency, 95) << "\n";

  require(db.close(), "close");
  fs::remove_all(directory);
  return false_promotions == 0 && dialectic_hits == cases ? 0 : 1;
}
