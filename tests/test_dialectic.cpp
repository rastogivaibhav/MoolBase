#include "graphene/dialectic.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << "FAIL " << operation << ": " << status.message << "\n";
    std::abort();
  }
}

NodeInput node(std::string content,
               std::vector<float> vector,
               uint64_t signature,
               bool root = false,
               bool symptom = false) {
  NodeInput input;
  input.content = std::move(content);
  input.vector = std::move(vector);
  input.signature = signature;
  input.incident = 7001;
  input.root = root;
  input.symptom = symptom;
  input.metadata["source"] = "dialectic-test";
  return input;
}

bool has_root(const BundleSet& bundles, uint32_t root) {
  return std::any_of(bundles.roots.begin(), bundles.roots.end(),
                     [&](const RootBundle& value) { return value.root_node == root; });
}

bool has_challenge(const OppositionReport& report, const std::string& text) {
  return std::any_of(report.challenged_claims.begin(), report.challenged_claims.end(),
                     [&](const std::string& value) {
                       return value.find(text) != std::string::npos;
                     });
}

} // namespace

int main() {
  Rfc3339Instant utc;
  Rfc3339Instant offset;
  require(parse_rfc3339("2026-07-24T12:30:45.123456789Z", &utc),
          "parse RFC3339 UTC");
  require(parse_rfc3339("2026-07-24T14:30:45.123456789+02:00", &offset),
          "parse RFC3339 offset");
  assert(utc.unix_seconds == offset.unix_seconds);
  assert(utc.nanosecond == offset.nanosecond);
  assert(!parse_rfc3339("2026-02-30T00:00:00Z", &utc));
  assert(!parse_rfc3339("2026-07-24 12:30:45", &utc));

  TemporalValidity interval;
  require(parse_temporal_validity(
              {{"valid_from", "2026-07-24T00:00:00Z"},
               {"valid_until", "2026-07-25T00:00:00Z"}},
              &interval),
          "parse temporal interval");
  require(parse_rfc3339("2026-07-24T12:00:00Z", &utc),
          "parse interval query");
  assert(valid_at(interval, utc));
  require(parse_rfc3339("2026-07-24T00:00:00-12:00", &utc),
          "parse lower offset boundary");
  require(parse_rfc3339("2026-07-24T12:00:00Z", &offset),
          "parse normalized boundary");
  assert(utc.unix_seconds == offset.unix_seconds);
  assert(!parse_temporal_validity(
      {{"valid_from", "2026-07-25T00:00:00Z"},
       {"valid_until", "2026-07-24T00:00:00Z"}},
      &interval));

  const fs::path directory = fs::temp_directory_path() / "graphenedb_dialectic_tests";
  fs::remove_all(directory);

  GrapheneDB db;
  DBOptions options;
  options.dimension = 3;
  options.fsync_on_commit = false;
  require(db.open(directory, options), "open");

  const uint64_t signature = signature_for(3, 8);
  uint32_t root_a = 0;
  uint32_t dependency = 0;
  uint32_t symptom = 0;
  uint32_t stale = 0;
  uint32_t root_b = 0;
  uint32_t root_c = 0;
  require(db.put_node(node("Primary deployment root", {1.0f, 0.0f, 0.0f}, signature, true),
                      &root_a),
          "put root a");
  require(db.put_node(node("Connection pool dependency", {0.1f, 0.9f, 0.0f}, signature),
                      &dependency),
          "put dependency");
  require(db.put_node(node("Checkout timeout symptom", {0.0f, 1.0f, 0.0f}, signature, false, true),
                      &symptom),
          "put symptom");
  require(db.put_node(node("Stale database hypothesis", {0.05f, 0.95f, 0.0f}, signature),
                      &stale),
          "put stale");
  require(db.put_node(node("Competing traffic root", {-1.0f, 0.0f, 0.0f}, signature, true),
                      &root_b),
          "put root b");
  require(db.put_node(node("Speculative DNS root", {0.0f, 0.0f, 1.0f}, signature, true),
                      &root_c),
          "put root c");

  uint32_t edge_a_dependency = 0;
  uint32_t edge_dependency_symptom = 0;
  uint32_t edge_shortcut = 0;
  uint32_t edge_contradiction = 0;
  uint32_t edge_b_symptom = 0;
  uint32_t edge_c_symptom = 0;
  uint32_t edge_reinforced = 0;
  uint32_t edge_observed_unsafe_compressed = 0;

  require(db.put_edge(
              {root_a, dependency, EdgeOrigin::Observed, EdgeRole::Mechanistic, 0.97,
               {{"source_id", "deployment-log"}}},
              &edge_a_dependency),
          "put mechanism 1");
  require(db.put_edge(
              {dependency, symptom, EdgeOrigin::Discovered, EdgeRole::Causal, 0.96,
               {{"source_id", "heap-profile"}}},
              &edge_dependency_symptom),
          "put mechanism 2");
  require(db.put_edge(
              {root_a, symptom, EdgeOrigin::Inferred, EdgeRole::Compressed, 0.92, {}},
              &edge_shortcut),
          "put unsafe shortcut");
  require(db.put_edge(
              {root_a, stale, EdgeOrigin::Observed, EdgeRole::Contradicts, 0.88,
               {{"source_id", "postmortem"}}},
              &edge_contradiction),
          "put contradiction");
  require(db.put_edge(
              {root_b, symptom, EdgeOrigin::Observed, EdgeRole::Causal, 0.70,
               {{"source_id", "traffic-dashboard"}, {"valid_until", "2025-12-31T23:59:59Z"}}},
              &edge_b_symptom),
          "put competing root");
  require(db.put_edge(
              {root_c, symptom, EdgeOrigin::Hypothetical, EdgeRole::Analogical, 0.65,
               {{"derived_from", "dns-analogy"}}},
              &edge_c_symptom),
          "put speculative root");
  require(db.put_edge(
              {root_b, stale, EdgeOrigin::Reinforced, EdgeRole::Predictive, 0.68,
               {{"promotion_status", "discovered"},
                {"valid_until", "2025-12-31T23:59:59Z"}}},
              &edge_reinforced),
          "put unsafe reinforcement");
  require(db.put_edge(
              {root_b, symptom, EdgeOrigin::Observed, EdgeRole::Compressed, 0.99,
               {{"source_id", "unexplained-shortcut"},
                {"valid_until", "2025-12-31T23:59:59Z"}}},
              &edge_observed_unsafe_compressed),
          "put observed shortcut without mechanism");
  require(db.put_edge(
              {symptom, dependency, EdgeOrigin::Observed, EdgeRole::Supports, 0.50,
               {{"source_id", "cycle-regression"}}}),
          "put traversal cycle");

  const size_t nodes_before = db.node_count();
  const size_t edges_before = db.edge_count();
  assert(db.incoming_edges(symptom).size() == 5);
  assert(db.outgoing_edges(root_a).size() == 3);

  DialecticEngine engine(db);
  DialecticOptions dialectic_options;
  dialectic_options.mode = QueryMode::Balanced;
  dialectic_options.semantic_candidates = 3;
  dialectic_options.max_hops = 4;
  dialectic_options.max_paths = 16;
  dialectic_options.max_paths_per_root = 8;
  dialectic_options.max_opposition_rounds = 1;
  dialectic_options.reexpansion_threshold = 0.20;

  const std::vector<float> query{0.0f, 1.0f, 0.0f};
  DialecticResult result = engine.reason(query, signature, dialectic_options);

  assert(!result.initial_bundle.roots.empty());
  assert(result.initial_bundle.roots.front().root_node == root_a);
  assert(has_root(result.initial_bundle, root_a));
  assert(has_root(result.initial_bundle, root_b));
  assert(!has_root(result.initial_bundle, root_c));
  assert(result.initial_convergence.has_answer);
  assert(result.initial_convergence.primary_node == root_a);
  assert(!result.initial_convergence.discarded_paths.empty());
  assert(result.initial_convergence.false_promotion_risk > 0.0);
  assert(has_challenge(result.initial_opposition, "contradiction"));
  assert(has_challenge(result.initial_opposition, "provenance"));
  assert(has_challenge(result.initial_opposition, "competing roots"));
  assert(result.initial_opposition.requests_reexpansion);
  assert(result.has_reopened_bundle);
  assert(result.reopened_bundle.snapshot_version ==
         result.initial_bundle.snapshot_version);
  assert(result.rounds == 1);
  assert(result.synthesis.has_answer);
  assert(result.synthesis.primary_node == root_a);
  assert(result.synthesis.epistemic_status == "contested");
  assert(!result.durable_writes);
  assert(db.node_count() == nodes_before);
  assert(db.edge_count() == edges_before);

  DialecticResult repeated = engine.reason(query, signature, dialectic_options);
  assert(repeated.synthesis.primary_node == result.synthesis.primary_node);
  assert(repeated.synthesis.epistemic_status == result.synthesis.epistemic_status);
  assert(repeated.initial_bundle.roots.size() == result.initial_bundle.roots.size());
  assert(repeated.initial_opposition.challenged_claims ==
         result.initial_opposition.challenged_claims);

  DialecticOptions empirical = dialectic_options;
  empirical.mode = QueryMode::Empirical;
  BundleSet empirical_bundle = engine.expand(query, signature, empirical);
  for (const auto& root : empirical_bundle.roots) {
    for (const auto& path : root.paths) {
      assert(std::find(path.edges.begin(), path.edges.end(), edge_shortcut) ==
             path.edges.end());
      assert(std::find(path.edges.begin(), path.edges.end(), edge_c_symptom) ==
             path.edges.end());
      assert(std::find(path.edges.begin(), path.edges.end(), edge_reinforced) ==
             path.edges.end());
      assert(std::find(path.edges.begin(), path.edges.end(),
                       edge_observed_unsafe_compressed) == path.edges.end());
    }
  }

  DialecticOptions theoretical = dialectic_options;
  theoretical.mode = QueryMode::Theoretical;
  BundleSet theoretical_bundle = engine.expand(query, signature, theoretical);
  assert(has_root(theoretical_bundle, root_c));

  DialecticOptions temporal = dialectic_options;
  temporal.as_of = "2026-07-24T00:00:00Z";
  BundleSet temporal_bundle = engine.expand(query, signature, temporal);
  assert(!has_root(temporal_bundle, root_b));

  DialecticOptions budgeted = dialectic_options;
  budgeted.max_visited_states = 1;
  BundleSet budgeted_bundle = engine.expand(query, signature, budgeted);
  assert(budgeted_bundle.truncated);
  assert(std::find(budgeted_bundle.warnings.begin(), budgeted_bundle.warnings.end(),
                   "VISIT_BUDGET_EXHAUSTED") != budgeted_bundle.warnings.end());

  DialecticOptions invalid_time = dialectic_options;
  invalid_time.as_of = "not-a-timestamp";
  BundleSet invalid_time_bundle = engine.expand(query, signature, invalid_time);
  assert(invalid_time_bundle.roots.empty());
  assert(!invalid_time_bundle.warnings.empty());
  assert(invalid_time_bundle.warnings.front().find("INVALID_AS_OF") == 0);

  DialecticResult invalid_query =
      engine.reason({0.0f, 1.0f}, signature, dialectic_options);
  assert(!invalid_query.synthesis.has_answer);
  assert(invalid_query.synthesis.epistemic_status == "abstain");

  require(db.close(), "close");
  fs::remove_all(directory);
  std::cout << "dialectic_contract_passed=true\n";
  return 0;
}
