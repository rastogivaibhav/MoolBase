#include "graphene/epistemic_control.hpp"
#include "graphene/hypokosh_runtime.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << "HARNESS_ERROR " << operation << ": " << status.message << "\n";
    std::exit(2);
  }
}

const char* boolean(bool value) { return value ? "true" : "false"; }

DialecticPath evidence_path(
    uint32_t root,
    uint32_t anchor,
    std::vector<uint32_t> edges,
    std::string source,
    std::string family,
    double score,
    bool contradiction = false,
    SemanticVerificationStatus verification =
        SemanticVerificationStatus::Verified) {
  DialecticPath value;
  value.root_node = root;
  value.anchor_node = anchor;
  value.nodes = {root, anchor};
  value.edges = std::move(edges);
  value.score = score;
  value.query_relevance = 1.0;
  value.target_consistency = 1.0;
  value.completeness = 1.0;
  value.contains_contradiction = contradiction;
  value.semantic_verification = verification;
  value.evidence.emplace_back(
      std::move(source), "perturbation-span", "", std::move(family));
  return value;
}

struct Evaluation {
  FiberBundle bundle;
  StabilityAssessment stability;
  EpistemicAdmissibility admissibility;
  ConvergedAnswer answer;
  OppositionReport opposition;
};

Evaluation evaluate(BundleSet raw) {
  Evaluation result;
  result.bundle = FiberBundleBuilder().build(raw);
  LyapunovCritic critic;
  EpistemicController controller;
  result.stability = critic.assess(result.bundle, QueryMode::Empirical);
  result.admissibility =
      controller.assess(result.bundle, result.stability, QueryMode::Empirical);
  DialecticOptions options;
  options.mode = QueryMode::Empirical;
  options.minimum_confidence = 0.35;
  options.reexpansion_threshold = 0.20;
  result.answer = controller.converge(
      result.bundle, result.admissibility, result.stability, options);
  result.opposition = controller.oppose(
      result.bundle, result.answer, result.admissibility,
      result.stability, options);
  return result;
}

const TargetFiber* find_fiber(const FiberBundle& bundle, uint32_t target) {
  const auto it = std::find_if(
      bundle.fibers.begin(), bundle.fibers.end(),
      [&](const TargetFiber& fiber) { return fiber.target_node == target; });
  return it == bundle.fibers.end() ? nullptr : &*it;
}

size_t target_raw_paths(const FiberBundle& bundle, uint32_t target) {
  const TargetFiber* fiber = find_fiber(bundle, target);
  return fiber ? fiber->raw_path_count : 0;
}

size_t target_independent_families(const FiberBundle& bundle, uint32_t target) {
  const TargetFiber* fiber = find_fiber(bundle, target);
  return fiber ? fiber->independent_evidence_family_count : 0;
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
  input.incident = 3601;
  input.root = root;
  input.symptom = symptom;
  input.metadata["source"] = "flagship-perturbation-proof";
  return input;
}

}  // namespace

int main() {
  constexpr uint32_t h1 = 101;
  constexpr uint32_t h2 = 202;
  constexpr uint32_t anchor = 900;

  // P1: add graph-distinct support that belongs to the same evidence family.
  BundleSet p1_raw;
  p1_raw.snapshot_version = 101;
  RootBundle p1_h1;
  p1_h1.root_node = h1;
  p1_h1.paths.push_back(evidence_path(
      h1, anchor, {1, 2}, "deployment-report-a", "release-family", 0.96));
  p1_h1.paths.push_back(evidence_path(
      h1, anchor, {3, 4}, "deployment-report-b", "release-family", 0.95));
  p1_h1.paths.push_back(evidence_path(
      h1, anchor, {5, 6}, "deployment-report-c", "release-family", 0.94));
  p1_raw.roots.push_back(p1_h1);
  const Evaluation p1 = evaluate(p1_raw);
  std::cout
      << "PERTURB|id=P1"
      << "|raw_paths=" << target_raw_paths(p1.bundle, h1)
      << "|independent_families="
      << target_independent_families(p1.bundle, h1)
      << "|sufficient_independent_support="
      << boolean(p1.admissibility.sufficient_independent_support)
      << "|admissible=" << boolean(p1.admissibility.evidence_admissible)
      << "|requires_external_verification="
      << boolean(p1.admissibility.requires_external_verification)
      << "|opposition_requests_reexpansion="
      << boolean(p1.opposition.requests_reexpansion)
      << "|answer=" << boolean(p1.answer.has_answer)
      << "\n";

  // P2: establish a two-family corroborated baseline, then remove one
  // independent family. The harness records the actual result even if the
  // baseline or perturbation falsifies the pre-registered expectation.
  BundleSet p2_baseline_raw;
  p2_baseline_raw.snapshot_version = 201;
  RootBundle p2_baseline_h1;
  p2_baseline_h1.root_node = h1;
  p2_baseline_h1.paths.push_back(evidence_path(
      h1, anchor, {10, 11}, "deployment-report", "release-family", 0.97));
  p2_baseline_h1.paths.push_back(evidence_path(
      h1, anchor, {12, 13}, "heap-profile", "runtime-family", 0.96));
  p2_baseline_raw.roots.push_back(p2_baseline_h1);
  const Evaluation p2_baseline = evaluate(p2_baseline_raw);

  BundleSet p2_removed_raw;
  p2_removed_raw.snapshot_version = 202;
  RootBundle p2_removed_h1;
  p2_removed_h1.root_node = h1;
  p2_removed_h1.paths.push_back(evidence_path(
      h1, anchor, {10, 11}, "deployment-report", "release-family", 0.97));
  p2_removed_raw.roots.push_back(p2_removed_h1);
  const Evaluation p2_removed = evaluate(p2_removed_raw);
  std::cout
      << "PERTURB|id=P2"
      << "|baseline_independent_families="
      << target_independent_families(p2_baseline.bundle, h1)
      << "|perturbed_independent_families="
      << target_independent_families(p2_removed.bundle, h1)
      << "|baseline_sufficient_independent_support="
      << boolean(p2_baseline.admissibility.sufficient_independent_support)
      << "|perturbed_sufficient_independent_support="
      << boolean(p2_removed.admissibility.sufficient_independent_support)
      << "|baseline_admissible="
      << boolean(p2_baseline.admissibility.evidence_admissible)
      << "|perturbed_admissible="
      << boolean(p2_removed.admissibility.evidence_admissible)
      << "|baseline_requires_external_verification="
      << boolean(p2_baseline.admissibility.requires_external_verification)
      << "|perturbed_requires_external_verification="
      << boolean(p2_removed.admissibility.requires_external_verification)
      << "|perturbed_opposition_requests_reexpansion="
      << boolean(p2_removed.opposition.requests_reexpansion)
      << "\n";

  // P3: inject material contradiction into an otherwise independently
  // supported hypothesis.
  BundleSet p3_raw = p2_baseline_raw;
  p3_raw.snapshot_version = 301;
  p3_raw.roots.front().paths.push_back(evidence_path(
      h1, anchor, {30, 31}, "rollback-observation", "rollback-family",
      0.99, true));
  const Evaluation p3 = evaluate(p3_raw);
  std::cout
      << "PERTURB|id=P3"
      << "|contradiction_blocks_resolution="
      << boolean(p3.admissibility.contradiction_blocks_resolution)
      << "|admissible=" << boolean(p3.admissibility.evidence_admissible)
      << "|opposition_requests_reexpansion="
      << boolean(p3.opposition.requests_reexpansion)
      << "|answer=" << boolean(p3.answer.has_answer)
      << "|reopen_count=" << p3.opposition.reopen_nodes.size()
      << "\n";

  // P4: identical semantic evidence, different root/path insertion order.
  BundleSet p4_a_raw;
  p4_a_raw.snapshot_version = 401;
  RootBundle p4_a_h1;
  p4_a_h1.root_node = h1;
  p4_a_h1.paths.push_back(evidence_path(
      h1, anchor, {40, 41}, "deployment-report", "release-family", 0.97));
  p4_a_h1.paths.push_back(evidence_path(
      h1, anchor, {42, 43}, "heap-profile", "runtime-family", 0.95));
  RootBundle p4_a_h2;
  p4_a_h2.root_node = h2;
  p4_a_h2.paths.push_back(evidence_path(
      h2, anchor, {50, 51}, "traffic-dashboard", "traffic-family", 0.86));
  p4_a_raw.roots = {p4_a_h1, p4_a_h2};

  BundleSet p4_b_raw;
  p4_b_raw.snapshot_version = 401;
  RootBundle p4_b_h1;
  p4_b_h1.root_node = h1;
  p4_b_h1.paths.push_back(evidence_path(
      h1, anchor, {42, 43}, "heap-profile", "runtime-family", 0.95));
  p4_b_h1.paths.push_back(evidence_path(
      h1, anchor, {40, 41}, "deployment-report", "release-family", 0.97));
  RootBundle p4_b_h2;
  p4_b_h2.root_node = h2;
  p4_b_h2.paths.push_back(evidence_path(
      h2, anchor, {50, 51}, "traffic-dashboard", "traffic-family", 0.86));
  p4_b_raw.roots = {p4_b_h2, p4_b_h1};

  const Evaluation p4_a = evaluate(p4_a_raw);
  const Evaluation p4_b = evaluate(p4_b_raw);
  const bool p4_semantic_equal =
      p4_a.answer.has_answer == p4_b.answer.has_answer &&
      p4_a.answer.primary_node == p4_b.answer.primary_node &&
      p4_a.admissibility.evidence_admissible ==
          p4_b.admissibility.evidence_admissible &&
      p4_a.admissibility.sufficient_independent_support ==
          p4_b.admissibility.sufficient_independent_support &&
      p4_a.admissibility.contradiction_blocks_resolution ==
          p4_b.admissibility.contradiction_blocks_resolution &&
      p4_a.opposition.requests_reexpansion ==
          p4_b.opposition.requests_reexpansion &&
      p4_a.stability.stable == p4_b.stability.stable;
  std::cout
      << "PERTURB|id=P4"
      << "|bundle_hash_equal="
      << boolean(p4_a.bundle.immutable_hash == p4_b.bundle.immutable_hash)
      << "|semantic_equal=" << boolean(p4_semantic_equal)
      << "|primary_a=" << p4_a.answer.primary_node
      << "|primary_b=" << p4_b.answer.primary_node
      << "|admissible_a="
      << boolean(p4_a.admissibility.evidence_admissible)
      << "|admissible_b="
      << boolean(p4_b.admissibility.evidence_admissible)
      << "|hash_a=" << p4_a.bundle.immutable_hash
      << "|hash_b=" << p4_b.bundle.immutable_hash
      << "\n";

  // P5: same hidden four-edge chain as the canonical flagship recovery fixture,
  // but only one recursive recovery cycle is permitted.
  const fs::path recovery_dir =
      fs::temp_directory_path() / "graphenedb_flagship_perturbation_p5";
  fs::remove_all(recovery_dir);
  GrapheneDB recovery_db;
  DBOptions db_options;
  db_options.dimension = 3;
  db_options.fsync_on_commit = false;
  require(recovery_db.open(recovery_dir, db_options), "open P5 database");

  const uint64_t deep_signature = signature_for(10, 11);
  uint32_t deep_root = 0;
  uint32_t bridge_one = 0;
  uint32_t bridge_two = 0;
  uint32_t bridge_three = 0;
  uint32_t deep_symptom = 0;
  require(recovery_db.put_node(
      node("Deep causal root", {1.0f, 0.0f, 0.0f}, deep_signature, true),
      &deep_root), "put P5 root");
  require(recovery_db.put_node(
      node("Deep bridge one", {0.8f, 0.2f, 0.0f}, deep_signature),
      &bridge_one), "put P5 bridge one");
  require(recovery_db.put_node(
      node("Deep bridge two", {0.4f, 0.6f, 0.0f}, deep_signature),
      &bridge_two), "put P5 bridge two");
  require(recovery_db.put_node(
      node("Deep bridge three", {0.1f, 0.3f, 0.6f}, deep_signature),
      &bridge_three), "put P5 bridge three");
  require(recovery_db.put_node(
      node("Deep observed symptom", {0.0f, 0.0f, 1.0f}, deep_signature,
           false, true), &deep_symptom), "put P5 symptom");

  require(recovery_db.put_edge(
      {deep_root, bridge_one, EdgeOrigin::Observed, EdgeRole::Causal, 0.99,
       {{"source_id", "deep-root-source"}}}), "put P5 edge 1");
  require(recovery_db.put_edge(
      {bridge_one, bridge_two, EdgeOrigin::Discovered,
       EdgeRole::Mechanistic, 0.99,
       {{"source_id", "deep-bridge-one"}}}), "put P5 edge 2");
  require(recovery_db.put_edge(
      {bridge_two, bridge_three, EdgeOrigin::Discovered,
       EdgeRole::Mechanistic, 0.99,
       {{"source_id", "deep-bridge-two"}}}), "put P5 edge 3");
  require(recovery_db.put_edge(
      {bridge_three, deep_symptom, EdgeOrigin::Observed,
       EdgeRole::Causal, 0.99,
       {{"source_id", "deep-symptom-source"}}}), "put P5 edge 4");

  RuntimeOptions recovery_options;
  recovery_options.dialectic.mode = QueryMode::Empirical;
  recovery_options.dialectic.semantic_candidates = 1;
  recovery_options.dialectic.max_hops = 1;
  recovery_options.dialectic.max_paths = 8;
  recovery_options.dialectic.max_paths_per_root = 4;
  recovery_options.dialectic.max_visited_states = 64;
  recovery_options.dialectic.minimum_confidence = 0.35;
  recovery_options.max_recursive_cycles = 1;
  recovery_options.unchanged_recovery_patience = 2;
  recovery_options.enable_opposition_research = false;
  recovery_options.update_model_world = false;

  CompleteHypoKoshRuntime runtime(recovery_db);
  const HypoKoshRuntimeResult p5 = runtime.reason(
      {0.0f, 0.0f, 1.0f}, deep_signature, recovery_options);

  bool semantic_widening = false;
  bool hop_step_valid = true;
  uint32_t final_max_hops = recovery_options.dialectic.max_hops;
  for (const auto& trace : p5.receipt.recovery_trace) {
    if (trace.previous_semantic_candidates != 1 ||
        trace.next_semantic_candidates != 1) {
      semantic_widening = true;
    }
    if (trace.next_max_hops != trace.previous_max_hops + 1) {
      hop_step_valid = false;
    }
    final_max_hops = trace.next_max_hops;
  }

  std::cout
      << "PERTURB|id=P5"
      << "|budget_max_recursive_cycles=1"
      << "|expansion_rounds=" << p5.receipt.expansion_rounds
      << "|trace_count=" << p5.receipt.recovery_trace.size()
      << "|semantic_widening=" << boolean(semantic_widening)
      << "|hop_step_valid=" << boolean(hop_step_valid)
      << "|final_max_hops=" << final_max_hops
      << "|primary=" << p5.primary_node
      << "|stop_reason="
      << (p5.receipt.recovery_stop_reason.empty()
              ? "none"
              : p5.receipt.recovery_stop_reason)
      << "\n";

  require(recovery_db.close(), "close P5 database");
  fs::remove_all(recovery_dir);

  std::cout << "PERTURBATION_HARNESS|schema_version=1|completed=true\n";
  return 0;
}
