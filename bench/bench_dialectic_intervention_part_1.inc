#include "graphene/dialectic.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

namespace {

enum class EscapeKind {
  MissingHop,
  SeekIndependent,
  ExpandMinority,
  None
};

enum class Policy {
  NoCycle,
  TargetedCurrentStop,
  TargetedFrontierAware,
  TargetedForced,
  BroadForced
};

struct Scenario {
  std::string family;
  EscapeKind escape{EscapeKind::None};
  uint32_t chain_hops{1};
  bool minority_path{false};
  bool independent_candidate{false};
  bool noise_trap{false};
  bool initial_is_correct{false};
};

struct CycleObservation {
  uint32_t cycle{0};
  uint32_t primary{0};
  bool has_answer{false};
  bool correct{false};
  bool opposition_requested{false};
  size_t roots{0};
  size_t paths{0};
  size_t visited_states{0};
  bool truncated{false};
  uint64_t bundle_signature{0};
  double confidence{0.0};
};

struct RunResult {
  std::string family;
  std::string policy;
  uint32_t seed{0};
  uint32_t expected_root{0};
  bool initial_correct{false};
  bool final_correct{false};
  bool corrected{false};
  bool degraded{false};
  bool useful_new_information{false};
  uint32_t cycles_executed{0};
  size_t total_visited_states{0};
  uint32_t first_correct_cycle{999};
  uint32_t oracle_cycle{0};
  uint32_t stop_regret_cycles{0};
  std::vector<CycleObservation> observations;
};

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << "FAIL " << operation << ": " << status.message << "\n";
    std::exit(2);
  }
}

std::vector<float> unit_vector(size_t dimension, size_t axis, float scale = 1.0f) {
  std::vector<float> value(dimension, 0.0f);
  value.at(axis % dimension) = scale;
  return value;
}

std::vector<float> near_query(size_t dimension, float cosine_like, size_t axis) {
  std::vector<float> value(dimension, 0.0f);
  value[0] = cosine_like;
  const float remainder = std::sqrt(std::max(0.0f, 1.0f - cosine_like * cosine_like));
  value[axis % dimension] = remainder;
  return value;
}

NodeInput make_node(std::string content,
                    std::vector<float> vector,
                    uint64_t signature,
                    bool root = false,
                    bool symptom = false) {
  NodeInput input;
  input.content = std::move(content);
  input.vector = std::move(vector);
  input.signature = signature;
  input.incident = 9001;
  input.root = root;
  input.symptom = symptom;
  input.metadata["source"] = "dialectic-intervention-benchmark";
  return input;
}

size_t path_count(const BundleSet& bundle) {
  size_t total = 0;
  for (const auto& root : bundle.roots) total += root.paths.size();
  return total;
}

uint64_t append_hash(uint64_t hash, uint64_t value) {
  for (int shift = 0; shift < 8; ++shift) {
    hash ^= static_cast<unsigned char>((value >> (shift * 8)) & 0xffU);
    hash *= 1099511628211ULL;
  }
  return hash;
}

uint64_t bundle_signature(const BundleSet& bundle) {
  uint64_t hash = 1469598103934665603ULL;
  hash = append_hash(hash, bundle.snapshot_version);
  hash = append_hash(hash, bundle.truncated ? 1 : 0);
  for (const auto& root : bundle.roots) {
    hash = append_hash(hash, root.root_node);
    for (const auto& path : root.paths) {
      hash = append_hash(hash, path.root_node);
      hash = append_hash(hash, path.anchor_node);
      for (uint32_t node : path.nodes) hash = append_hash(hash, node);
      for (uint32_t edge : path.edges) hash = append_hash(hash, edge);
