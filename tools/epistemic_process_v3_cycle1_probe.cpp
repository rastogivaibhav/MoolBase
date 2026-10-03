#include "graphene/hypokosh_runtime.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

struct ScenarioResult {
  std::string name;
  uint32_t h1_node{0};
  uint32_t h2_node{0};
  bool has_answer{false};
  uint32_t selected_node{0};
  std::string selected_label;
  GovernedEpistemicStatus status{GovernedEpistemicStatus::Abstain};
  FiberBundle final_bundle;
  ReasoningReceipt receipt;
};

void require(Status status, const std::string& op) {
  if (!status) throw std::runtime_error(op + ": " + status.message);
}

NodeInput node(std::string content, std::vector<float> vector, bool root) {
  NodeInput input;
  input.content = std::move(content);
  input.vector = std::move(vector);
  input.signature = 0;
  input.root = root;
  input.incident = 3001;
  input.metadata["source"] = "v3-cycle1-forensic-probe";
  return input;
}

std::vector<std::string> families(const FiberBundle& bundle) {
  std::vector<std::string> out;
  for (const auto& fiber : bundle.fibers) {
    for (const auto& path : fiber.paths) {
      out.insert(
          out.end(),
          path.evidence_family_lineage.begin(),
          path.evidence_family_lineage.end());
    }
  }
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

void add_support(
    GrapheneDB& db,
    uint32_t target,
    const std::string& evidence_id,
    const std::string& family) {
  uint32_t evidence_node = 0;
  NodeInput evidence = node(
      evidence_id,
      {0.0f, 1.0f, 0.0f},
      false);
  evidence.metadata["event_id"] = evidence_id;
  evidence.metadata["family"] = family;
  require(db.put_node(evidence, &evidence_node), "put evidence " + evidence_id);

  EdgeInput edge;
  edge.from = target;
  edge.to = evidence_node;
  edge.origin = EdgeOrigin::Observed;
  edge.role = EdgeRole::Supports;
  edge.confidence = 0.90;
  edge.metadata["source_id"] = evidence_id;
  edge.metadata["evidence_family_id"] = family;
  edge.metadata["event_kind"] = "support";
  uint32_t edge_id = 0;
  require(db.put_edge(edge, &edge_id), "put support " + evidence_id);
}

RuntimeOptions runtime_options(bool hypokosh, bool dwm) {
  RuntimeOptions options;
  options.dialectic.mode = QueryMode::Empirical;
  options.dialectic.semantic_candidates = 32;
  options.dialectic.max_hops = 2;
  options.dialectic.max_paths = 128;
  options.dialectic.max_paths_per_root = 32;
  options.dialectic.max_visited_states = 4096;
  options.dialectic.minimum_confidence = 0.10;
  options.dialectic.reexpansion_threshold = 0.25;
  options.max_recursive_cycles = 2;
  options.update_model_world = false;
  options.enable_hypokosh = hypokosh;
  options.enable_dwm = dwm;
  options.enable_opposition_research = dwm;
  return options;
}

ScenarioResult run_tie(
    const std::string& name,
    bool h2_root_first,
    bool h2_evidence_first,
    const std::string& h1_family,
    const std::string& h2_family) {
  const fs::path dir =
      fs::temp_directory_path() / ("moolbase-v3-cycle1-" + name);
  fs::remove_all(dir);

  GrapheneDB db;
  DBOptions db_options;
  db_options.dimension = 3;
  db_options.fsync_on_commit = false;
  require(db.open(dir, db_options), "open " + name);

  uint32_t h1 = 0;
  uint32_t h2 = 0;
  if (h2_root_first) {
    require(db.put_node(node("H2", {-1.0f, 0.0f, 0.0f}, true), &h2), "put H2");
    require(db.put_node(node("H1", {1.0f, 0.0f, 0.0f}, true), &h1), "put H1");
  } else {
    require(db.put_node(node("H1", {1.0f, 0.0f, 0.0f}, true), &h1), "put H1");
    require(db.put_node(node("H2", {-1.0f, 0.0f, 0.0f}, true), &h2), "put H2");
  }

  if (h2_evidence_first) {
    add_support(db, h2, name + "_H2_E", h2_family);
    add_support(db, h1, name + "_H1_E", h1_family);
  } else {
    add_support(db, h1, name + "_H1_E", h1_family);
    add_support(db, h2, name + "_H2_E", h2_family);
  }

  CompleteHypoKoshRuntime runtime(db);
  const auto result = runtime.reason(
      {0.0f, 1.0f, 0.0f}, 0, runtime_options(true, false));

  ScenarioResult out;
  out.name = name;
  out.h1_node = h1;
  out.h2_node = h2;
  out.has_answer = result.final_convergence.has_answer;
  out.selected_node =
      result.final_convergence.has_answer
          ? result.final_convergence.primary_node
          : 0;
  out.selected_label =
      !out.has_answer
          ? "ABSTAIN"
          : (out.selected_node == h1 ? "H1" :
             (out.selected_node == h2 ? "H2" : "UNKNOWN"));
  out.status = result.status;
  out.final_bundle = result.final_bundle;
  out.receipt = result.receipt;

  require(db.close(), "close " + name);
  fs::remove_all(dir);
  return out;
}

ScenarioResult run_single_support_g2() {
  const std::string name = "dwm_single_support";
  const fs::path dir =
      fs::temp_directory_path() / ("moolbase-v3-cycle1-" + name);
  fs::remove_all(dir);

  GrapheneDB db;
  DBOptions db_options;
  db_options.dimension = 3;
  db_options.fsync_on_commit = false;
  require(db.open(dir, db_options), "open dwm probe");

  uint32_t h1 = 0;
  uint32_t h2 = 0;
  require(db.put_node(node("H1", {1.0f, 0.0f, 0.0f}, true), &h1), "put H1");
  require(db.put_node(node("H2", {-1.0f, 0.0f, 0.0f}, true), &h2), "put H2");
  add_support(db, h1, "DWM_E1", "DWM_F1");

  CompleteHypoKoshRuntime runtime(db);
  const auto result = runtime.reason(
      {0.0f, 1.0f, 0.0f}, 0, runtime_options(true, true));

  ScenarioResult out;
  out.name = name;
  out.h1_node = h1;
  out.h2_node = h2;
  out.has_answer = result.final_convergence.has_answer;
  out.selected_node =
      result.final_convergence.has_answer
          ? result.final_convergence.primary_node
          : 0;
  out.selected_label =
      !out.has_answer
          ? "ABSTAIN"
          : (out.selected_node == h1 ? "H1" :
             (out.selected_node == h2 ? "H2" : "UNKNOWN"));
  out.status = result.status;
  out.final_bundle = result.final_bundle;
  out.receipt = result.receipt;

  require(db.close(), "close dwm probe");
  fs::remove_all(dir);
  return out;
}

ScenarioResult run_g0_family_probe() {
  const std::string name = "g0_family_namespace";
  const fs::path dir =
      fs::temp_directory_path() / ("moolbase-v3-cycle1-" + name);
  fs::remove_all(dir);

  GrapheneDB db;
  DBOptions db_options;
  db_options.dimension = 3;
  db_options.fsync_on_commit = false;
  require(db.open(dir, db_options), "open G0 family probe");

  uint32_t h1 = 0;
  uint32_t h2 = 0;
  require(db.put_node(node("H1", {1.0f, 0.0f, 0.0f}, true), &h1), "put H1");
  require(db.put_node(node("H2", {-1.0f, 0.0f, 0.0f}, true), &h2), "put H2");
  add_support(db, h1, "G0_E1", "PLAIN_FAMILY");

  CompleteHypoKoshRuntime runtime(db);
  const auto result = runtime.reason(
      {0.0f, 1.0f, 0.0f}, 0, runtime_options(false, false));

  ScenarioResult out;
  out.name = name;
  out.h1_node = h1;
  out.h2_node = h2;
  out.has_answer = false;
  out.selected_label = "ABSTAIN";
  out.status = result.status;
  out.final_bundle = result.final_bundle;
  out.receipt = result.receipt;

  require(db.close(), "close G0 family probe");
  fs::remove_all(dir);
  return out;
}

void expect(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error("forensic assertion failed: " + message);
}

}  // namespace

int main() {
  try {
    const ScenarioResult baseline =
        run_tie("tie_baseline", false, false, "F1", "F2");
    const ScenarioResult root_swap =
        run_tie("tie_root_swap", true, false, "F1", "F2");
    const ScenarioResult evidence_swap =
        run_tie("tie_evidence_swap", false, true, "F1", "F2");
    const ScenarioResult family_rename =
        run_tie("tie_family_rename", false, false, "RENAMED_X", "RENAMED_Y");

    expect(baseline.h1_node < baseline.h2_node, "baseline node order");
    expect(baseline.selected_label == "H1", "baseline selects lower-id H1");
    expect(root_swap.h2_node < root_swap.h1_node, "root swap reverses node IDs");
    expect(root_swap.selected_label == "H2", "root swap selects lower-id H2");
    expect(evidence_swap.selected_label == "H1", "evidence order does not change tie result");
    expect(family_rename.selected_label == "H1", "family rename does not change tie result");

    for (int i = 0; i < 8; ++i) {
      const auto repeat =
          run_tie("tie_repeat_" + std::to_string(i), false, i % 2 == 1,
                  "RF1", "RF2");
      expect(repeat.selected_label == "H1", "deterministic repeated tie selection");
    }

    const ScenarioResult dwm = run_single_support_g2();
    expect(!dwm.receipt.recovery_trace.empty(), "DWM recovery trace exists");
    const RecoveryRoundTrace& round = dwm.receipt.recovery_trace.front();
    expect(round.opposition_search_requested, "DWM requested opposition search");
    expect(round.next_reopen_nodes > 0, "DWM supplied reopen nodes");
    expect(round.options_changed, "DWM expanded search options");
    expect(round.previous_bundle_hash == round.next_bundle_hash,
           "DWM reopen produced identical bundle");
    expect(round.previous_visited_states == round.next_visited_states,
           "DWM reopen discovered no new search states");
    expect(round.stop_reason == "no_progress_after_expansion",
           "DWM stops only after ineffective expansion");

    const ScenarioResult g0 = run_g0_family_probe();
    const auto g0_families = families(g0.final_bundle);
    expect(std::find(g0_families.begin(), g0_families.end(),
                     "family:PLAIN_FAMILY") != g0_families.end(),
           "G0 family lineage is namespaced with family: prefix");
    expect(std::find(g0_families.begin(), g0_families.end(),
                     "PLAIN_FAMILY") == g0_families.end(),
           "unprefixed family id is not emitted by FiberBundle");

    std::cout
        << "{\"schema\":\"epistemic-process-v3-cycle1-forensic-probe-v1\""
        << ",\"production_behavior_modified\":false"
        << ",\"tie_bias\":{"
        << "\"baseline_h1_node\":" << baseline.h1_node
        << ",\"baseline_h2_node\":" << baseline.h2_node
        << ",\"baseline_selected\":\"" << baseline.selected_label << "\""
        << ",\"root_swap_h1_node\":" << root_swap.h1_node
        << ",\"root_swap_h2_node\":" << root_swap.h2_node
        << ",\"root_swap_selected\":\"" << root_swap.selected_label << "\""
        << ",\"evidence_swap_selected\":\"" << evidence_swap.selected_label << "\""
        << ",\"family_rename_selected\":\"" << family_rename.selected_label << "\""
        << ",\"repeat_count\":8"
        << "}"
        << ",\"dwm_noop_reopen\":{"
        << "\"opposition_search_requested\":true"
        << ",\"reopen_nodes_after\":" << round.next_reopen_nodes
        << ",\"bundle_unchanged\":true"
        << ",\"visited_states_unchanged\":true"
        << ",\"stop_reason\":\"" << round.stop_reason << "\""
        << "}"
        << ",\"g0_family_namespace\":[";
    for (size_t i = 0; i < g0_families.size(); ++i) {
      if (i) std::cout << ',';
      std::cout << "\"" << g0_families[i] << "\"";
    }
    std::cout << "]}" << std::endl;
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "cycle1_forensic_probe_error=" << error.what() << "\n";
    return 1;
  }
}
