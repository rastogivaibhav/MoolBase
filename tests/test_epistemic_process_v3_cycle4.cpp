#include "graphene/hypokosh_runtime.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

void require(Status status, const std::string& operation) {
  if (!status) {
    std::cerr << "cycle4_contract_failure=" << operation
              << " detail=" << status.message << "\n";
    std::abort();
  }
}

NodeInput make_node(std::string content,
                    std::vector<float> vector,
                    bool root = false) {
  NodeInput input;
  input.content = std::move(content);
  input.vector = std::move(vector);
  input.signature = 0;
  input.root = root;
  input.incident = 4404;
  input.metadata["source"] = "v3-cycle4-contract";
  return input;
}

uint32_t add_node(GrapheneDB& db, const std::string& name,
                  std::vector<float> vector, bool root = false) {
  uint32_t id = 0;
  require(db.put_node(make_node(name, std::move(vector), root), &id),
          "put node " + name);
  return id;
}

void add_edge(GrapheneDB& db, uint32_t from, uint32_t to,
              EdgeRole role, double confidence,
              const std::string& source,
              const std::string& family) {
  EdgeInput edge;
  edge.from = from;
  edge.to = to;
  edge.origin = EdgeOrigin::Observed;
  edge.role = role;
  edge.confidence = confidence;
  edge.metadata["source_id"] = source;
  edge.metadata["evidence_family_id"] = family;
  uint32_t id = 0;
  require(db.put_edge(edge, &id), "put edge " + source);
}

RuntimeOptions options(size_t semantic_candidates = 8) {
  RuntimeOptions out;
  out.enable_hypokosh = true;
  out.enable_dwm = true;
  out.enable_opposition_research = true;
  out.update_model_world = false;
  out.max_recursive_cycles = 1;
  out.dialectic.mode = QueryMode::Empirical;
  out.dialectic.semantic_candidates = semantic_candidates;
  out.dialectic.max_hops = 2;
  out.dialectic.max_paths = 32;
  out.dialectic.max_paths_per_root = 8;
  out.dialectic.max_visited_states = 256;
  out.dialectic.minimum_confidence = 0.20;
  out.dialectic.reexpansion_threshold = 0.15;
  return out;
}

bool has_event(const HypoKoshRuntimeResult& result,
               EpistemicEventType type) {
  for (const auto& event : result.receipt.epistemic_events) {
    if (event.type == type) return true;
  }
  return false;
}

GrapheneDB open_db(const fs::path& dir) {
  fs::remove_all(dir);
  GrapheneDB db;
  DBOptions db_options;
  db_options.dimension = 3;
  db_options.fsync_on_commit = false;
  require(db.open(dir, db_options), "open db");
  return db;
}

}  // namespace

int main() {
  const fs::path base =
      fs::temp_directory_path() / "moolbase-v3-cycle4-contracts";
  fs::remove_all(base);
  fs::create_directories(base);

  // Contract 1: an exact semantic tie is Open, not target-id-selected.
  {
    GrapheneDB db = open_db(base / "tie");
    const uint32_t h1 = add_node(db, "H1", {-1.0f, 0.0f, 0.0f}, true);
    const uint32_t h2 = add_node(db, "H2", {-1.0f, 0.0f, 0.0f}, true);
    const uint32_t e1 = add_node(db, "H1 evidence", {1.0f, 0.0f, 0.0f});
    const uint32_t e2 = add_node(db, "H2 evidence", {1.0f, 0.0f, 0.0f});
    add_edge(db, h1, e1, EdgeRole::Supports, 0.90, "tie-h1", "tie-h1-family");
    add_edge(db, h2, e2, EdgeRole::Supports, 0.90, "tie-h2", "tie-h2-family");

    RuntimeOptions run = options(2);
    run.enable_dwm = false;
    run.enable_opposition_research = false;
    CompleteHypoKoshRuntime runtime(db);
    const auto result = runtime.reason({1.0f, 0.0f, 0.0f}, 0, run);
    assert(!result.final_convergence.has_answer);
    assert(result.primary_node == 0);
    assert(result.status == GovernedEpistemicStatus::Open);
    assert(result.receipt.terminal_cause == "unresolved_semantic_tie");
    require(db.close(), "close tie db");
  }

  // Contract 2: insufficient corroboration is not a DWM challenge.
  {
    GrapheneDB db = open_db(base / "corroboration");
    const uint32_t root =
        add_node(db, "single support hypothesis", {-1.0f, 0.0f, 0.0f}, true);
    const uint32_t evidence =
        add_node(db, "single support evidence", {1.0f, 0.0f, 0.0f});
    add_edge(db, root, evidence, EdgeRole::Supports, 0.95,
             "corroboration-one", "corroboration-family-one");

    CompleteHypoKoshRuntime runtime(db);
    const auto result =
        runtime.reason({1.0f, 0.0f, 0.0f}, 0, options(1));
    assert(has_event(result, EpistemicEventType::CorroborationSearch));
    assert(!has_event(result, EpistemicEventType::Challenge));
    assert(!has_event(result, EpistemicEventType::Reopen));
    require(db.close(), "close corroboration db");
  }

  // Contract 3: real opposition with an exhausted frontier emits Challenge
  // but no Reopen.
  {
    GrapheneDB db = open_db(base / "exhausted");
    const uint32_t root =
        add_node(db, "exhausted hypothesis", {-1.0f, 0.0f, 0.0f}, true);
    const uint32_t s1 =
        add_node(db, "support A", {1.0f, 0.00f, 0.0f});
    const uint32_t s2 =
        add_node(db, "support B", {0.99f, 0.01f, 0.0f});
    const uint32_t opp =
        add_node(db, "material opposition", {0.98f, 0.02f, 0.0f});
    add_edge(db, root, s1, EdgeRole::Supports, 0.95, "ex-s1", "ex-fa");
    add_edge(db, root, s2, EdgeRole::Supports, 0.94, "ex-s2", "ex-fb");
    add_edge(db, root, opp, EdgeRole::Contradicts, 0.90, "ex-opp", "ex-fo");

    CompleteHypoKoshRuntime runtime(db);
    const auto result =
        runtime.reason({1.0f, 0.0f, 0.0f}, 0, options(3));
    assert(has_event(result, EpistemicEventType::Challenge));
    assert(!has_event(result, EpistemicEventType::Reopen));
    assert(!result.receipt.recovery_trace.empty());
    const auto& trace = result.receipt.recovery_trace.front();
    assert(trace.dialectical_challenge_present);
    assert(!trace.expansion_opportunity_available);
    assert(trace.expansion_opportunity_class ==
           "NO_EXPANSION_OPPORTUNITY");
    assert(!trace.opposition_search_requested);
    require(db.close(), "close exhausted db");
  }

  // Contract 4: opposition plus a genuinely unseen downstream observation
  // makes the opportunity observable and permits Reopen.
  {
    GrapheneDB db = open_db(base / "latent");
    const uint32_t root =
        add_node(db, "latent hypothesis", {-1.0f, 0.0f, 0.0f}, true);
    const uint32_t s1 =
        add_node(db, "visible support A", {1.0f, 0.00f, 0.0f});
    const uint32_t s2 =
        add_node(db, "visible support B", {0.99f, 0.01f, 0.0f});
    const uint32_t opp =
        add_node(db, "visible opposition", {0.98f, 0.02f, 0.0f});
    const uint32_t latent =
        add_node(db, "latent support C", {-0.95f, 0.0f, 0.0f});
    add_edge(db, root, s1, EdgeRole::Supports, 0.95, "lat-s1", "lat-fa");
    add_edge(db, root, s2, EdgeRole::Supports, 0.94, "lat-s2", "lat-fb");
    add_edge(db, root, opp, EdgeRole::Contradicts, 0.90, "lat-opp", "lat-fo");
    add_edge(db, root, latent, EdgeRole::Supports, 0.92, "lat-hidden", "lat-fc");

    CompleteHypoKoshRuntime runtime(db);
    const auto result =
        runtime.reason({1.0f, 0.0f, 0.0f}, 0, options(3));
    assert(has_event(result, EpistemicEventType::Challenge));
    assert(has_event(result, EpistemicEventType::Reopen));
    assert(!result.receipt.recovery_trace.empty());
    const auto& trace = result.receipt.recovery_trace.front();
    assert(trace.dialectical_challenge_present);
    assert(trace.expansion_opportunity_available);
    assert(trace.expansion_opportunity_class ==
           "NEW_ELIGIBLE_NODE_AVAILABLE");
    assert(trace.opposition_search_requested);
    assert(trace.next_reopen_nodes > 0);
    require(db.close(), "close latent db");
  }

  fs::remove_all(base);
  std::cout << "v3_cycle4_runtime_contracts=passed\n";
  return 0;
}
