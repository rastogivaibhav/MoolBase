#include "graphene/hypokosh_runtime.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << "FAIL " << operation << ": " << status.message << "\n";
    std::abort();
  }
}

DialecticPath path(uint32_t root, uint32_t anchor,
                   std::vector<uint32_t> edges,
                   std::vector<std::string> sources,
                   double score,
                   bool contradiction = false) {
  DialecticPath output;
  output.root_node = root;
  output.anchor_node = anchor;
  output.nodes = {root, anchor};
  output.edges = std::move(edges);
  output.score = score;
  output.contains_contradiction = contradiction;
  output.temporal_consistent = true;
  for (const auto& source : sources) output.evidence.push_back({source, "", ""});
  return output;
}

NodeInput node(std::string content, std::vector<float> vector,
               uint64_t signature, bool root = false, bool symptom = false) {
  NodeInput input;
  input.content = std::move(content);
  input.vector = std::move(vector);
  input.signature = signature;
  input.incident = 991;
  input.root = root;
  input.symptom = symptom;
  input.metadata["source"] = "hypokosh-runtime-test";
  return input;
}

}  // namespace

int main() {
  BundleSet raw;
  raw.snapshot_version = 7;
  RootBundle root;
  root.root_node = 10;
  root.paths.push_back(path(10, 40, {1, 2}, {"deployment", "metrics"}, 0.91));
  root.paths.push_back(path(10, 40, {3, 4}, {"config", "traces"}, 0.88));
  // Exact duplicate must not inflate degeneracy.
  root.paths.push_back(path(10, 40, {3, 4}, {"config", "traces"}, 0.88));
  raw.roots.push_back(root);

  FiberBundleBuilder builder;
  const FiberBundle bundle = builder.build(raw);
  assert(bundle.fibers.size() == 1);
  assert(bundle.fibers.front().raw_path_count == 3);
  assert(bundle.fibers.front().paths.size() == 2);
  assert(bundle.fibers.front().independent_path_count == 2);
  assert(bundle.immutable_hash == FiberBundleBuilder::hash(bundle));
  const uint64_t immutable_before = bundle.immutable_hash;

  StabilityCriticV0 critic;
  const StabilityAssessment stable = critic.assess(bundle);
  assert(stable.temporal_consistency == 1.0);
  assert(stable.provenance_score == 1.0);
  assert(stable.path_diversity > 0.9);
  assert(stable.degeneracy_score > 0.70);
  assert(!stable.requires_abstention);
  assert(bundle.immutable_hash == immutable_before);

  BundleSet narrow_raw;
  narrow_raw.snapshot_version = 7;
  RootBundle narrow_root;
  narrow_root.root_node = 10;
  narrow_root.paths.push_back(path(10, 40, {1, 2}, {}, 0.99));
  narrow_raw.roots.push_back(narrow_root);
  const FiberBundle narrow = builder.build(narrow_raw);
  const StabilityAssessment locked = critic.assess(narrow, QueryMode::Empirical);
  assert(locked.pattern_lock_score > 0.70);
  assert(locked.missing_evidence_penalty > 0.0);
  assert(locked.requires_escape);

  CorrectiveEscape escape;
  const EscapePlan escape_plan = escape.plan(narrow, locked, QueryMode::Empirical);
  assert(!escape_plan.tasks.empty());
  assert(escape_plan.requires_human_evidence || locked.requires_abstention == false);

  ModelWorld model_world;
  ModelWorldNode unsupported;
  unsupported.type = ModelWorldNodeType::Hypothesis;
  unsupported.status = ModelWorldStatus::Active;
  unsupported.statement = "unsupported candidate";
  unsupported.origin = EdgeOrigin::Hypothetical;
  const uint64_t unsupported_id = model_world.add(unsupported, "test unsupported hypothesis");
  assert(!model_world.update_status(unsupported_id, ModelWorldStatus::Verified,
                                    "must not silently promote"));
  assert(!model_world.events().empty());
  assert(!model_world.audit().empty());
  ModelWorldScheduler scheduler;
  const auto scheduler_report = scheduler.run(model_world, 16);
  assert(scheduler_report.jobs_run.size() == 5);
  assert(!scheduler_report.findings.empty());

  const fs::path directory = fs::temp_directory_path() / "graphenedb_complete_hypokosh_runtime";
  fs::remove_all(directory);
  GrapheneDB db;
  DBOptions options;
  options.dimension = 3;
  options.fsync_on_commit = false;
  require(db.open(directory, options), "open runtime database");

  const uint64_t signature = signature_for(3, 8);
  uint32_t root_node = 0;
  uint32_t dependency_a = 0;
  uint32_t dependency_b = 0;
  uint32_t symptom = 0;
  uint32_t alternative = 0;
  require(db.put_node(node("Release changed pool timeout", {1.0f, 0.0f, 0.0f}, signature, true), &root_node), "put root");
  require(db.put_node(node("Connection pool exhausted", {0.2f, 0.8f, 0.0f}, signature), &dependency_a), "put dep a");
  require(db.put_node(node("Database wait queue increased", {0.1f, 0.9f, 0.0f}, signature), &dependency_b), "put dep b");
  require(db.put_node(node("Checkout failures increased", {0.0f, 1.0f, 0.0f}, signature, false, true), &symptom), "put symptom");
  require(db.put_node(node("Traffic spike hypothesis", {-1.0f, 0.0f, 0.0f}, signature, true), &alternative), "put alternative");

  require(db.put_edge({root_node, dependency_a, EdgeOrigin::Observed, EdgeRole::Mechanistic, 0.96,
                       {{"source_id", "release-log"}}}), "edge root-dep-a");
  require(db.put_edge({dependency_a, symptom, EdgeOrigin::Discovered, EdgeRole::Causal, 0.95,
                       {{"source_id", "heap-profile"}}}), "edge dep-a-symptom");
  require(db.put_edge({root_node, dependency_b, EdgeOrigin::Observed, EdgeRole::Mechanistic, 0.92,
                       {{"source_id", "config-diff"}}}), "edge root-dep-b");
  require(db.put_edge({dependency_b, symptom, EdgeOrigin::Observed, EdgeRole::Supports, 0.90,
                       {{"source_id", "database-metrics"}}}), "edge dep-b-symptom");
  require(db.put_edge({alternative, symptom, EdgeOrigin::Observed, EdgeRole::Causal, 0.55,
                       {{"source_id", "traffic-dashboard"}}}), "edge alternative");

  RuntimeOptions runtime_options;
  runtime_options.dialectic.mode = QueryMode::Empirical;
  runtime_options.dialectic.semantic_candidates = 4;
  runtime_options.dialectic.max_hops = 4;
  runtime_options.dialectic.max_paths = 32;
  runtime_options.dialectic.max_paths_per_root = 8;
  runtime_options.dialectic.minimum_confidence = 0.35;
  runtime_options.dialectic.reexpansion_threshold = 0.20;
  runtime_options.max_recursive_cycles = 2;

  ModelWorld runtime_world;
  CompleteHypoKoshRuntime runtime(db, &runtime_world);
  const HypoKoshRuntimeResult result =
      runtime.reason({0.0f, 1.0f, 0.0f}, signature, runtime_options);

  assert(result.receipt.graphene_executed);
  assert(result.receipt.fiber_bundle_built);
  assert(result.receipt.stability_critic_executed);
  assert(result.receipt.lyapunov_trajectory_executed);
  assert(result.receipt.lyapunov_certificate_valid);
  assert(!result.lyapunov.observations.empty());
  assert(result.lyapunov.certificate.weights_positive);
  assert(result.lyapunov.certificate.state_bounded);
  assert(result.lyapunov.certificate.energy_nonnegative);
  assert(result.lyapunov.certificate.quadratic_bounds_valid);
  assert(result.lyapunov.certificate.final_energy >= 0.0);
  assert(result.lyapunov.certificate.final_energy <= 1.0);
  assert(result.receipt.escape_considered);
  assert(result.receipt.convergence_executed);
  assert(result.receipt.opposition_executed);
  assert(result.receipt.governed_projection_executed);
  assert(result.receipt.initial_bundle_hash != 0);
  assert(result.receipt.final_bundle_hash != 0);
  assert(result.primary_node == root_node);
  assert(result.status != GovernedEpistemicStatus::Abstain);
  assert(result.status != GovernedEpistemicStatus::EvidenceRequired);
  assert(!result.evidence_edges.empty());
  assert(!result.final_self_healing.repairs.empty());
  assert(result.final_self_healing.rerun_required || !result.final_stability.requires_escape);
  assert(!runtime_world.nodes().empty());
  assert(runtime_world.audit().empty());
  const fs::path model_world_path = directory / "model-world.snapshot";
  require(runtime_world.save(model_world_path), "save model world");
  ModelWorld reloaded_world;
  require(reloaded_world.load(model_world_path), "load model world");
  assert(reloaded_world.nodes().size() == runtime_world.nodes().size());
  assert(reloaded_world.events().size() == runtime_world.events().size());
  assert(reloaded_world.event_log_hash() == runtime_world.event_log_hash());
  assert(reloaded_world.audit().empty());

  const HypoKoshRuntimeResult repeated =
      runtime.reason({0.0f, 1.0f, 0.0f}, signature, runtime_options);
  assert(repeated.primary_node == result.primary_node);
  assert(repeated.status == result.status);
  assert(repeated.receipt.initial_bundle_hash == result.receipt.initial_bundle_hash);
  assert(repeated.receipt.final_bundle_hash == result.receipt.final_bundle_hash);
  assert(repeated.lyapunov.certificate.final_energy ==
         result.lyapunov.certificate.final_energy);
  assert(repeated.lyapunov.certificate.violations ==
         result.lyapunov.certificate.violations);

  require(db.close(), "close runtime database");
  fs::remove_all(directory);
  std::cout << "complete_hypokosh_runtime_contract_passed=true\n";
  return 0;
}
