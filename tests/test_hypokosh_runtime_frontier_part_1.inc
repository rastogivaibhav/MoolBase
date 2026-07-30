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
