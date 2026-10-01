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
              const std::string& family,
              const std::string& evidence_state = "active") {
  EdgeInput edge;
  edge.from = from;
  edge.to = to;
  edge.origin = EdgeOrigin::Observed;
  edge.role = role;
  edge.confidence = confidence;
  edge.metadata["source_id"] = source;
  edge.metadata["evidence_family_id"] = family;
  edge.metadata["evidence_state"] = evidence_state;
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

bool has_transition(const HypoKoshRuntimeResult& result,
                    EpistemicEventType type,
                    uint32_t from,
                    uint32_t to) {
  for (const auto& event : result.receipt.epistemic_events) {
    if (event.type == type &&
        event.previous_hypothesis_node == from &&
        event.hypothesis_node == to) {
      return true;
    }
  }
  return false;
}

bool status_commits_for_test(GovernedEpistemicStatus status) {
  return status == GovernedEpistemicStatus::Resolved ||
         status == GovernedEpistemicStatus::ProvisionallyResolved;
}

PriorEpistemicState prior_from(const HypoKoshRuntimeResult& result,
                               uint32_t last_committed = 0) {
  PriorEpistemicState prior;
  prior.available = true;
  prior.has_answer = result.final_convergence.has_answer;
  prior.operative_node =
      prior.has_answer ? result.final_convergence.primary_node : 0;
  prior.committed_node =
      prior.has_answer && status_commits_for_test(result.status)
          ? result.final_convergence.primary_node
          : 0;
  prior.last_committed_node =
      prior.committed_node != 0 ? prior.committed_node : last_committed;
  prior.status = result.status;
  prior.bundle_hash = result.final_bundle.immutable_hash;
  prior.stability = result.final_stability;
  return prior;
}

class AlwaysVerifiedPathVerifier final : public PathVerifier {
 public:
  PathVerificationResult verify(
      const DialecticPath&,
      const PathVerificationContext&) const override {
    PathVerificationResult out;
    out.semantic_verification = SemanticVerificationStatus::Verified;
    out.verifier_version = "v3-cycle4.2-test-verifier";
    return out;
  }
};

bool has_evidence_source(const FiberBundle& bundle,
                         const std::string& source_id) {
  for (const auto& fiber : bundle.fibers) {
    for (const auto& path : fiber.paths) {
      for (const auto& evidence : path.evidence) {
        if (evidence.source_id == source_id) return true;
      }
    }
  }
  return false;
}

void open_db(GrapheneDB* db, const fs::path& dir) {
  fs::remove_all(dir);
  DBOptions db_options;
  db_options.dimension = 3;
  db_options.fsync_on_commit = false;
  require(db->open(dir, db_options), "open db");
}

}  // namespace

int main() {
  const fs::path base =
      fs::temp_directory_path() / "moolbase-v3-cycle4-contracts";
  fs::remove_all(base);
  fs::create_directories(base);

  // Contract 1: an exact semantic tie is Open, not target-id-selected.
  {
    GrapheneDB db;
    open_db(&db, base / "tie");
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
    GrapheneDB db;
    open_db(&db, base / "corroboration");
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
    GrapheneDB db;
    open_db(&db, base / "exhausted");
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
    GrapheneDB db;
    open_db(&db, base / "latent");
    const uint32_t root =
        add_node(db, "latent hypothesis", {-1.0f, 0.0f, 0.0f}, true);
    const uint32_t s1 =
        add_node(db, "visible support A", {1.0f, 0.00f, 0.0f});
    const uint32_t s2 =
        add_node(db, "visible support B", {0.99f, 0.01f, 0.0f});
    const uint32_t opp =
        add_node(db, "visible opposition", {0.98f, 0.02f, 0.0f});
    const uint32_t replacement =
        add_node(db, "replacement hypothesis", {0.0f, -1.0f, 0.0f}, true);
    const uint32_t latent =
        add_node(db, "latent support C", {0.0f, -1.0f, 0.0f});
    add_edge(db, root, s1, EdgeRole::Supports, 0.95, "lat-s1", "lat-fa");
    add_edge(db, root, s2, EdgeRole::Supports, 0.94, "lat-s2", "lat-fb");
    add_edge(db, root, opp, EdgeRole::Contradicts, 0.90, "lat-opp", "lat-fo");
    add_edge(db, replacement, latent, EdgeRole::Supports, 0.92,
             "lat-hidden", "lat-fc");
    add_edge(db, opp, latent, EdgeRole::Mechanistic, 0.90,
             "lat-bridge", "lat-bridge-family");

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
    assert(has_evidence_source(result.final_bundle, "lat-hidden"));
    assert(trace.bundle_changed || trace.frontier_progress);
    require(db.close(), "close latent db");
  }

  // Contract 5: superseded evidence remains auditable in FiberBundle
  // lineage but cannot contribute active independent support.
  {
    GrapheneDB db;
    open_db(&db, base / "lifecycle");
    const uint32_t root =
        add_node(db, "lifecycle hypothesis", {-1.0f, 0.0f, 0.0f}, true);
    const uint32_t active =
        add_node(db, "active evidence", {1.0f, 0.0f, 0.0f});
    const uint32_t superseded =
        add_node(db, "superseded evidence", {0.99f, 0.01f, 0.0f});
    add_edge(db, root, active, EdgeRole::Supports, 0.95,
             "life-active", "life-active-family", "active");
    add_edge(db, root, superseded, EdgeRole::Supports, 0.94,
             "life-old", "life-old-family", "superseded");

    GrapheneEvidenceExpander expander(db);
    DialecticOptions expansion_options;
    expansion_options.mode = QueryMode::Empirical;
    expansion_options.semantic_candidates = 2;
    expansion_options.max_hops = 2;
    expansion_options.max_paths = 16;
    expansion_options.max_paths_per_root = 8;
    expansion_options.max_visited_states = 128;
    const BundleSet raw =
        expander.expand({1.0f, 0.0f, 0.0f}, 0, expansion_options);
    const FiberBundle bundle = FiberBundleBuilder().build(raw);
    assert(bundle.fibers.size() == 1);
    const TargetFiber& fiber = bundle.fibers.front();
    assert(fiber.independent_evidence_family_count == 1);

    bool saw_superseded_audit = false;
    bool superseded_was_active = false;
    for (const FiberPath& path : fiber.paths) {
      for (const EvidenceRef& evidence : path.evidence) {
        if (evidence.lifecycle_state != "superseded") continue;
        saw_superseded_audit = true;
        superseded_was_active =
            superseded_was_active || path.eligible_for_support ||
            path.eligible_for_opposition;
      }
    }
    assert(saw_superseded_audit);
    assert(!superseded_was_active);

    require(db.close(), "close lifecycle db");
  }

  // Contract 6: native epistemic transitions span sequential production
  // calls over the same persistent evidence store.
  {
    GrapheneDB db;
    open_db(&db, base / "native-transitions");
    add_node(db, "reserved null sentinel", {0.0f, -1.0f, 0.0f});
    const uint32_t h1 =
        add_node(db, "transition H1", {-1.0f, 0.0f, 0.0f}, true);
    const uint32_t h2 =
        add_node(db, "transition H2", {0.0f, -1.0f, 0.0f}, true);
    const uint32_t h1a =
        add_node(db, "H1 support A", {1.0f, 0.00f, 0.0f});
    const uint32_t h1b =
        add_node(db, "H1 support B", {0.99f, 0.01f, 0.0f});
    add_edge(db, h1, h1a, EdgeRole::Supports, 0.95, "tr-h1-a", "tr-h1-fa");
    add_edge(db, h1, h1b, EdgeRole::Supports, 0.94, "tr-h1-b", "tr-h1-fb");

    RuntimeOptions run = options(8);
    run.enable_dwm = false;
    run.enable_opposition_research = false;
    CompleteHypoKoshRuntime runtime(db);

    const auto first = runtime.reason({1.0f, 0.0f, 0.0f}, 0, run);
    assert(first.final_convergence.has_answer);
    assert(first.final_convergence.primary_node == h1);
    assert(!has_event(first, EpistemicEventType::Revision));
    assert(!has_event(first, EpistemicEventType::Decommitment));
    const PriorEpistemicState first_prior = prior_from(first);
    assert(first_prior.committed_node == h1);

    const uint32_t refute =
        add_node(db, "H1 refutation", {0.98f, 0.02f, 0.0f});
    add_edge(db, h1, refute, EdgeRole::Contradicts, 0.95,
             "tr-refute", "tr-refute-family");
    // The frozen clear-incumbent policy requires a different semantic leader
    // whose replacement corroboration is still insufficient. Refutation
    // alone may leave a unique operative leader inspectable while contested.
    const uint32_t h2a =
        add_node(db, "H2 support A", {0.97f, 0.03f, 0.0f});
    add_edge(db, h2, h2a, EdgeRole::Supports, 0.96, "tr-h2-a", "tr-h2-fa");
    RuntimeOptions second_options = run;
    second_options.prior_epistemic_state = first_prior;
    const auto second =
        runtime.reason({1.0f, 0.0f, 0.0f}, 0, second_options);
    assert(!second.final_convergence.has_answer);
    assert(second.status == GovernedEpistemicStatus::Contested);
    assert(has_transition(
        second, EpistemicEventType::Revision, h1, 0));
    assert(has_transition(
        second, EpistemicEventType::Decommitment, h1, 0));

    const PriorEpistemicState second_prior =
        prior_from(second, first_prior.last_committed_node);
    assert(second_prior.committed_node == 0);
    assert(second_prior.last_committed_node == h1);

    const uint32_t h2b =
        add_node(db, "H2 support B", {0.96f, 0.04f, 0.0f});
    add_edge(db, h2, h2b, EdgeRole::Supports, 0.95, "tr-h2-b", "tr-h2-fb");

    RuntimeOptions third_options = run;
    third_options.prior_epistemic_state = second_prior;
    const auto third =
        runtime.reason({1.0f, 0.0f, 0.0f}, 0, third_options);
    assert(third.final_convergence.has_answer);
    assert(third.final_convergence.primary_node == h2);
    assert(has_transition(
        third, EpistemicEventType::Recommitment, h1, h2));

    RuntimeOptions unchanged_options = run;
    unchanged_options.prior_epistemic_state = prior_from(third);
    const auto unchanged =
        runtime.reason({1.0f, 0.0f, 0.0f}, 0, unchanged_options);
    assert(unchanged.final_convergence.primary_node == h2);
    assert(!has_event(unchanged, EpistemicEventType::Revision));
    assert(!has_event(unchanged, EpistemicEventType::Decommitment));
    assert(!has_event(unchanged, EpistemicEventType::Recommitment));

    require(db.close(), "close native transition db");
  }

  // Contract 7: verified stable evidence can earn Resolution across calls
  // using the production Lyapunov dwell requirement rather than a synthetic
  // status override.
  {
    GrapheneDB db;
    open_db(&db, base / "native-resolution");
    add_node(db, "reserved null sentinel", {0.0f, -1.0f, 0.0f});
    const uint32_t root =
        add_node(db, "resolution hypothesis", {-1.0f, 0.0f, 0.0f}, true);
    const uint32_t a =
        add_node(db, "resolution support A", {1.0f, 0.00f, 0.0f});
    const uint32_t b =
        add_node(db, "resolution support B", {0.99f, 0.01f, 0.0f});
    add_edge(db, root, a, EdgeRole::Supports, 0.97, "rs-a", "rs-fa");
    add_edge(db, root, b, EdgeRole::Supports, 0.96, "rs-b", "rs-fb");

    AlwaysVerifiedPathVerifier verifier;
    RuntimeOptions run = options(4);
    run.enable_dwm = false;
    run.enable_opposition_research = false;
    run.path_verifier = &verifier;
    CompleteHypoKoshRuntime runtime(db);

    const auto first = runtime.reason({1.0f, 0.0f, 0.0f}, 0, run);
    assert(first.final_convergence.has_answer);
    assert(first.final_admissibility.semantic_verification ==
           SemanticVerificationStatus::Verified);

    RuntimeOptions second_options = run;
    second_options.prior_epistemic_state = prior_from(first);
    const auto second =
        runtime.reason({1.0f, 0.0f, 0.0f}, 0, second_options);
    assert(second.final_convergence.has_answer);
    assert(second.status == GovernedEpistemicStatus::Resolved);
    assert(has_event(second, EpistemicEventType::Resolution));
    assert(second.lyapunov.certificate.practical_stability_observed);

    RuntimeOptions unchanged_options = run;
    unchanged_options.prior_epistemic_state = prior_from(second);
    const auto unchanged =
        runtime.reason({1.0f, 0.0f, 0.0f}, 0, unchanged_options);
    assert(unchanged.status == GovernedEpistemicStatus::Resolved);
    assert(!has_event(unchanged, EpistemicEventType::Resolution));

    require(db.close(), "close native resolution db");
  }

  fs::remove_all(base);
  std::cout << "v3_cycle4_runtime_contracts=passed\n";
  return 0;
}
