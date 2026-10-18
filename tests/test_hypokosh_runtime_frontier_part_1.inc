#include "graphene/hypokosh_runtime.hpp"
#include "graphene/epistemic_receipt.hpp"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
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

  // Pattern-lock is meaningful only for a support-eligible path. Keep the
  // >0.70 invariant, but give this fixture admissible evidence so it reaches
  // the pattern-lock calculation instead of the no-support early return.
  BundleSet narrow_raw;
  narrow_raw.snapshot_version = 7;
  RootBundle narrow_root;
  narrow_root.root_node = 10;
  narrow_root.paths.push_back(path(10, 40, {1, 2}, {"deployment", "metrics"}, 0.99));
  narrow_raw.roots.push_back(narrow_root);
  const FiberBundle narrow = builder.build(narrow_raw);
  const StabilityAssessment locked = critic.assess(narrow, QueryMode::Empirical);
  assert(locked.pattern_lock_score > 0.70);
  assert(locked.requires_escape);

  // Missing evidence is a distinct invariant. An evidence-free path must not
  // masquerade as pattern lock: it should be rejected as unsupported and
  // drive escape/abstention through the explicit missing-evidence branch.
  BundleSet unsupported_raw;
  unsupported_raw.snapshot_version = 7;
  RootBundle unsupported_root;
  unsupported_root.root_node = 10;
  unsupported_root.paths.push_back(path(10, 40, {1, 2}, {}, 0.99));
  unsupported_raw.roots.push_back(unsupported_root);
  const FiberBundle unsupported_bundle = builder.build(unsupported_raw);
  const StabilityAssessment unsupported_assessment =
      critic.assess(unsupported_bundle, QueryMode::Empirical);
  assert(unsupported_assessment.pattern_lock_score == 0.0);
  assert(unsupported_assessment.missing_evidence_penalty > 0.0);
  assert(unsupported_assessment.requires_escape);
  assert(unsupported_assessment.requires_abstention);

  CorrectiveEscape escape;
  const EscapePlan escape_plan =
      escape.plan(unsupported_bundle, unsupported_assessment, QueryMode::Empirical);
  assert(!escape_plan.tasks.empty());
  assert(escape_plan.requires_human_evidence || unsupported_assessment.requires_abstention);

  ModelWorld model_world;

  // P0 regression: direct insertion cannot bypass verification policy.
  // Rejections must leave the node/event log and next ID unchanged.
  ModelWorldNode fabricated;
  fabricated.type = ModelWorldNodeType::Hypothesis;
  fabricated.status = ModelWorldStatus::Verified;
  fabricated.origin = EdgeOrigin::Hypothetical;
  fabricated.statement = "unsupported claim declared verified";
  const size_t original_nodes = model_world.nodes().size();
  const size_t original_events = model_world.events().size();
  assert(model_world.add(fabricated, "reject fabricated truth") == 0);
  assert(model_world.nodes().size() == original_nodes);
  assert(model_world.events().size() == original_events);

  // Evidence links cannot legitimise hypothetical or inferred provenance.
  fabricated.evidence_edges = {17};
  assert(model_world.add(fabricated, "reject hypothetical with evidence") == 0);
  fabricated.origin = EdgeOrigin::Inferred;
  assert(model_world.add(fabricated, "reject inferred with evidence") == 0);
  assert(model_world.nodes().size() == original_nodes);
  assert(model_world.events().size() == original_events);

  fabricated.origin = EdgeOrigin::Observed;
  fabricated.evidence_edges.clear();
  assert(model_world.add(fabricated, "reject missing evidence") == 0);
  assert(model_world.nodes().size() == original_nodes);
  assert(model_world.events().size() == original_events);

  // Positive control: origin + explicit evidence reference can enter as
  // verified under the existing policy (not external proof of source quality).
  fabricated.evidence_edges = {17};
  const uint64_t valid_verified_id =
      model_world.add(fabricated, "accept evidence-referenced observation");
  assert(valid_verified_id == 1);
  assert(model_world.get(valid_verified_id).has_value());
  assert(model_world.get(valid_verified_id)->status == ModelWorldStatus::Verified);
  assert(model_world.events().size() == original_events + 1);

  // P0 regression: a checksum-free old/forged snapshot cannot load a
  // hypothetical verified node with no evidence. Failed load stays atomic.
  {
    const fs::path invalid_world_path =
        fs::temp_directory_path() / "moolbase-model-world-invalid-verified-test.mwv";
    {
      std::ofstream forged(invalid_world_path, std::ios::trunc);
      assert(forged.good());
      forged << "MWV1\nNODE 1 2 3 4 \"fabricated verified hypothesis\" 0 0 0\n";
      assert(forged.good());
    }
    ModelWorld loaded_world;
    const Status invalid_status = loaded_world.load(invalid_world_path);
    assert(!invalid_status && invalid_status.code == ErrorCode::DataCorrupt);
    assert(loaded_world.nodes().empty());
    assert(loaded_world.events().empty());
    std::error_code cleanup_error;
    fs::remove(invalid_world_path, cleanup_error);
    assert(!cleanup_error);
  }

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