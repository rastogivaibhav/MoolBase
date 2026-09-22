#include "graphene/epistemic_control.hpp"
#include "graphene/hypokosh_runtime.hpp"

#include <algorithm>
#include <cassert>
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
    std::cerr << "FAIL " << operation << ": " << status.message << "\n";
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
      std::move(source), "canonical-demo-span", "", std::move(family));
  return value;
}

struct PhaseResult {
  FiberBundle bundle;
  StabilityAssessment stability;
  EpistemicAdmissibility admissibility;
  ConvergedAnswer answer;
  OppositionReport opposition;
};

PhaseResult evaluate(BundleSet raw) {
  PhaseResult result;
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

size_t raw_paths(const FiberBundle& bundle) {
  size_t total = 0;
  for (const auto& fiber : bundle.fibers) total += fiber.raw_path_count;
  return total;
}

size_t independent_families(const FiberBundle& bundle) {
  size_t total = 0;
  for (const auto& fiber : bundle.fibers) {
    total += fiber.independent_evidence_family_count;
  }
  return total;
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
  input.incident = 3301;
  input.root = root;
  input.symptom = symptom;
  input.metadata["source"] = "flagship-proof";
  return input;
}

void emit_phase(const char* name, const PhaseResult& phase) {
  std::cout
      << "PHASE|" << name
      << "|bundle_hash=" << phase.bundle.immutable_hash
      << "|raw_paths=" << raw_paths(phase.bundle)
      << "|independent_families=" << independent_families(phase.bundle)
      << "|answer=" << boolean(phase.answer.has_answer)
      << "|primary=" << phase.answer.primary_node
      << "|admissible=" << boolean(phase.admissibility.evidence_admissible)
      << "|sufficient_independent_support="
      << boolean(phase.admissibility.sufficient_independent_support)
      << "|contradiction_blocks_resolution="
      << boolean(phase.admissibility.contradiction_blocks_resolution)
      << "|requires_external_verification="
      << boolean(phase.admissibility.requires_external_verification)
      << "|stable=" << boolean(phase.stability.stable)
      << "|opposition_requests_reexpansion="
      << boolean(phase.opposition.requests_reexpansion)
      << "|reopen_count=" << phase.opposition.reopen_nodes.size()
      << "|discarded_paths=" << phase.answer.discarded_paths.size()
      << "\n";
}

}  // namespace

int main() {
  constexpr uint32_t h1 = 101;
  constexpr uint32_t h2 = 202;
  constexpr uint32_t anchor = 900;

  // Phase A: two graph-distinct paths cite one evidence family. Raw path
  // multiplicity must not become fabricated independent corroboration.
  BundleSet phase_a_raw;
  phase_a_raw.snapshot_version = 1;
  RootBundle phase_a_h1;
  phase_a_h1.root_node = h1;
  phase_a_h1.paths.push_back(evidence_path(
      h1, anchor, {1, 2}, "deployment-report", "release-family", 0.96));
  phase_a_h1.paths.push_back(evidence_path(
      h1, anchor, {3, 4}, "deployment-report", "release-family", 0.94));
  phase_a_raw.roots.push_back(phase_a_h1);
  const PhaseResult phase_a = evaluate(phase_a_raw);
  const TargetFiber* phase_a_fiber = find_fiber(phase_a.bundle, h1);
  assert(phase_a_fiber);
  assert(phase_a_fiber->raw_path_count == 2);
  assert(phase_a_fiber->independent_evidence_family_count == 1);
  assert(!phase_a.admissibility.sufficient_independent_support);

  // Phase B: H1 gains independent corroboration while H2 also acquires
  // admissible support. H2 must survive convergence as a competing target.
  BundleSet phase_b_raw;
  phase_b_raw.snapshot_version = 2;
  RootBundle phase_b_h1;
  phase_b_h1.root_node = h1;
  phase_b_h1.paths.push_back(evidence_path(
      h1, anchor, {10, 11}, "deployment-report", "release-family", 0.97));
  phase_b_h1.paths.push_back(evidence_path(
      h1, anchor, {12, 13}, "heap-profile", "runtime-family", 0.95));
  RootBundle phase_b_h2;
  phase_b_h2.root_node = h2;
  phase_b_h2.paths.push_back(evidence_path(
      h2, anchor, {20, 21}, "traffic-dashboard", "traffic-family", 0.86));
  phase_b_raw.roots = {phase_b_h1, phase_b_h2};
  const PhaseResult phase_b = evaluate(phase_b_raw);
  assert(phase_b.answer.has_answer);
  assert(phase_b.answer.primary_node == h1);
  assert(!phase_b.answer.discarded_paths.empty());
  assert(phase_b.opposition.requests_reexpansion);
  assert(std::find(
      phase_b.opposition.reopen_nodes.begin(),
      phase_b.opposition.reopen_nodes.end(), h2) !=
      phase_b.opposition.reopen_nodes.end());

  // Phase C: material opposition is retained as opposition rather than erased.
  // This is the negative control: a blocking contradiction must prevent final
  // resolution at the admissibility boundary.
  BundleSet phase_c_raw = phase_b_raw;
  phase_c_raw.snapshot_version = 3;
  phase_c_raw.roots.front().paths.push_back(evidence_path(
      h1, anchor, {30, 31}, "rollback-observation", "rollback-family",
      0.99, true));
  const PhaseResult phase_c = evaluate(phase_c_raw);
  assert(phase_c.admissibility.contradiction_blocks_resolution);
  assert(!phase_c.admissibility.evidence_admissible);
  assert(phase_c.opposition.requests_reexpansion);

  // Phase E: discriminating evidence is represented without deleting the
  // earlier receipt. This phase demonstrates a changed governed evidence state,
  // not durable cross-run DWM belief promotion.
  BundleSet phase_e_raw;
  phase_e_raw.snapshot_version = 4;
  RootBundle phase_e_h1;
  phase_e_h1.root_node = h1;
  phase_e_h1.paths.push_back(evidence_path(
      h1, anchor, {40, 41}, "deployment-report", "release-family", 0.97));
  phase_e_h1.paths.push_back(evidence_path(
      h1, anchor, {42, 43}, "heap-profile", "runtime-family", 0.96));
  phase_e_h1.paths.push_back(evidence_path(
      h1, anchor, {44, 45}, "controlled-reproduction", "experiment-family",
      0.98));
  RootBundle phase_e_h2;
  phase_e_h2.root_node = h2;
  phase_e_h2.paths.push_back(evidence_path(
      h2, anchor, {50, 51}, "traffic-dashboard", "traffic-family", 0.60));
  phase_e_raw.roots = {phase_e_h1, phase_e_h2};
  const PhaseResult phase_e = evaluate(phase_e_raw);
  assert(phase_e.answer.has_answer);
  assert(phase_e.answer.primary_node == h1);
  assert(phase_e.admissibility.sufficient_independent_support);
  assert(!phase_e.admissibility.contradiction_blocks_resolution);

  emit_phase("A", phase_a);
  emit_phase("B", phase_b);
  emit_phase("C", phase_c);

  // Phase D: exercise the actual bounded dialectic reopen path against a
  // durable GrapheneDB instance.
  const fs::path dwm_dir =
      fs::temp_directory_path() / "graphenedb_flagship_dwm";
  fs::remove_all(dwm_dir);
  GrapheneDB dwm_db;
  DBOptions db_options;
  db_options.dimension = 3;
  db_options.fsync_on_commit = false;
  require(dwm_db.open(dwm_dir, db_options), "open DWM database");

  const uint64_t signature = signature_for(3, 9);
  uint32_t root_a = 0;
  uint32_t dependency = 0;
  uint32_t symptom = 0;
  uint32_t root_b = 0;
  require(dwm_db.put_node(
      node("Deployment changed pool timeout", {1.0f, 0.0f, 0.0f},
           signature, true), &root_a), "put DWM root A");
  require(dwm_db.put_node(
      node("Connection pool exhaustion", {0.1f, 0.9f, 0.0f}, signature),
      &dependency), "put DWM dependency");
  require(dwm_db.put_node(
      node("Checkout timeout", {0.0f, 1.0f, 0.0f}, signature, false, true),
      &symptom), "put DWM symptom");
  require(dwm_db.put_node(
      node("Traffic spike alternative", {-1.0f, 0.0f, 0.0f},
           signature, true), &root_b), "put DWM root B");

  require(dwm_db.put_edge(
      {root_a, dependency, EdgeOrigin::Observed, EdgeRole::Mechanistic, 0.97,
       {{"source_id", "deployment-report"}}}), "put DWM edge 1");
  require(dwm_db.put_edge(
      {dependency, symptom, EdgeOrigin::Discovered, EdgeRole::Causal, 0.96,
       {{"source_id", "heap-profile"}}}), "put DWM edge 2");
  require(dwm_db.put_edge(
      {root_b, symptom, EdgeOrigin::Observed, EdgeRole::Causal, 0.82,
       {{"source_id", "traffic-dashboard"}}}), "put DWM alternative");

  const size_t nodes_before = dwm_db.node_count();
  const size_t edges_before = dwm_db.edge_count();
  DialecticEngine engine(dwm_db);
  DialecticOptions dialectic_options;
  dialectic_options.mode = QueryMode::Balanced;
  dialectic_options.semantic_candidates = 3;
  dialectic_options.max_hops = 4;
  dialectic_options.max_paths = 16;
  dialectic_options.max_paths_per_root = 8;
  dialectic_options.max_opposition_rounds = 1;
  dialectic_options.reexpansion_threshold = 0.20;
  const DialecticResult dwm = engine.reason(
      {0.0f, 1.0f, 0.0f}, signature, dialectic_options);
  assert(dwm.initial_opposition.requests_reexpansion);
  assert(dwm.has_reopened_bundle);
  assert(dwm.rounds == 1);
  assert(!dwm.durable_writes);
  assert(dwm_db.node_count() == nodes_before);
  assert(dwm_db.edge_count() == edges_before);
  std::cout
      << "DWM"
      << "|initial_primary=" << dwm.initial_convergence.primary_node
      << "|reopen_count=" << dwm.initial_opposition.reopen_nodes.size()
      << "|has_reopened_bundle=" << boolean(dwm.has_reopened_bundle)
      << "|rounds=" << dwm.rounds
      << "|synthesis_status=" << dwm.synthesis.epistemic_status
      << "|durable_writes=" << boolean(dwm.durable_writes)
      << "\n";
  require(dwm_db.close(), "close DWM database");
  fs::remove_all(dwm_dir);

  // Recovery receipt: a four-edge hidden chain must advance depth one hop per
  // round without broad semantic widening.
  const fs::path recovery_dir =
      fs::temp_directory_path() / "graphenedb_flagship_recovery";
  fs::remove_all(recovery_dir);
  GrapheneDB recovery_db;
  require(recovery_db.open(recovery_dir, db_options), "open recovery database");

  const uint64_t deep_signature = signature_for(10, 11);
  uint32_t deep_root = 0;
  uint32_t bridge_one = 0;
  uint32_t bridge_two = 0;
  uint32_t bridge_three = 0;
  uint32_t deep_symptom = 0;
  require(recovery_db.put_node(
      node("Deep causal root", {1.0f, 0.0f, 0.0f}, deep_signature, true),
      &deep_root), "put deep root");
  require(recovery_db.put_node(
      node("Deep bridge one", {0.8f, 0.2f, 0.0f}, deep_signature),
      &bridge_one), "put bridge one");
  require(recovery_db.put_node(
      node("Deep bridge two", {0.4f, 0.6f, 0.0f}, deep_signature),
      &bridge_two), "put bridge two");
  require(recovery_db.put_node(
      node("Deep bridge three", {0.1f, 0.3f, 0.6f}, deep_signature),
      &bridge_three), "put bridge three");
  require(recovery_db.put_node(
      node("Deep observed symptom", {0.0f, 0.0f, 1.0f}, deep_signature,
           false, true), &deep_symptom), "put deep symptom");

  require(recovery_db.put_edge(
      {deep_root, bridge_one, EdgeOrigin::Observed, EdgeRole::Causal, 0.99,
       {{"source_id", "deep-root-source"}}}), "put deep edge 1");
  require(recovery_db.put_edge(
      {bridge_one, bridge_two, EdgeOrigin::Discovered,
       EdgeRole::Mechanistic, 0.99,
       {{"source_id", "deep-bridge-one"}}}), "put deep edge 2");
  require(recovery_db.put_edge(
      {bridge_two, bridge_three, EdgeOrigin::Discovered,
       EdgeRole::Mechanistic, 0.99,
       {{"source_id", "deep-bridge-two"}}}), "put deep edge 3");
  require(recovery_db.put_edge(
      {bridge_three, deep_symptom, EdgeOrigin::Observed,
       EdgeRole::Causal, 0.99,
       {{"source_id", "deep-symptom-source"}}}), "put deep edge 4");

  RuntimeOptions recovery_options;
  recovery_options.dialectic.mode = QueryMode::Empirical;
  recovery_options.dialectic.semantic_candidates = 1;
  recovery_options.dialectic.max_hops = 1;
  recovery_options.dialectic.max_paths = 8;
  recovery_options.dialectic.max_paths_per_root = 4;
  recovery_options.dialectic.max_visited_states = 64;
  recovery_options.dialectic.minimum_confidence = 0.35;
  recovery_options.max_recursive_cycles = 3;
  recovery_options.unchanged_recovery_patience = 2;
  recovery_options.enable_opposition_research = false;
  recovery_options.update_model_world = false;

  CompleteHypoKoshRuntime runtime(recovery_db);
  const HypoKoshRuntimeResult recovery = runtime.reason(
      {0.0f, 0.0f, 1.0f}, deep_signature, recovery_options);
  assert(recovery.primary_node == deep_root);
  assert(recovery.receipt.expansion_rounds == 3);
  assert(recovery.receipt.recovery_trace.size() == 3);
  for (size_t index = 0; index < recovery.receipt.recovery_trace.size();
       ++index) {
    const auto& trace = recovery.receipt.recovery_trace[index];
    assert(trace.previous_semantic_candidates == 1);
    assert(trace.next_semantic_candidates == 1);
    assert(trace.next_max_hops == trace.previous_max_hops + 1);
    std::cout
        << "TRACE"
        << "|round=" << trace.round_index
        << "|max_hops_before=" << trace.previous_max_hops
        << "|max_hops_after=" << trace.next_max_hops
        << "|semantic_candidates_before=" << trace.previous_semantic_candidates
        << "|semantic_candidates_after=" << trace.next_semantic_candidates
        << "|bundle_changed=" << boolean(trace.bundle_changed)
        << "|frontier_progress=" << boolean(trace.frontier_progress)
        << "|visited_before=" << trace.previous_visited_states
        << "|visited_after=" << trace.next_visited_states
        << "|stop_reason="
        << (trace.stop_reason.empty() ? "continue" : trace.stop_reason)
        << "\n";
  }
  std::cout
      << "RECOVERY"
      << "|primary=" << recovery.primary_node
      << "|expansion_rounds=" << recovery.receipt.expansion_rounds
      << "|frontier_progress_rounds="
      << recovery.receipt.frontier_progress_rounds
      << "|stop_reason="
      << (recovery.receipt.recovery_stop_reason.empty()
              ? "none"
              : recovery.receipt.recovery_stop_reason)
      << "|final_bundle_hash=" << recovery.receipt.final_bundle_hash
      << "\n";
  require(recovery_db.close(), "close recovery database");
  fs::remove_all(recovery_dir);

  emit_phase("E", phase_e);
  std::cout
      << "HISTORY"
      << "|phase_c_bundle_hash=" << phase_c.bundle.immutable_hash
      << "|phase_e_bundle_hash=" << phase_e.bundle.immutable_hash
      << "|prior_receipt_preserved=true"
      << "|durable_cross_run_belief_revision_claim=false"
      << "\n";
  std::cout << "FLAGSHIP|contract_passed=true|schema_version=1\n";
  return 0;
}
