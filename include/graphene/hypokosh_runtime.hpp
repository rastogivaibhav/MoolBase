#pragma once

#include "graphene/epistemic_control.hpp"
#include "graphene/escape.hpp"
#include "graphene/model_world.hpp"
#include "graphene/path_verifier.hpp"
#include "graphene/self_healing.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

enum class GovernedEpistemicStatus : uint8_t {
  Resolved,
  ProvisionallyResolved,
  Contested,
  EvidenceRequired,
  Abstain,
  Speculative
};

enum class EpistemicEventSource : uint8_t {
  GrapheneCore,
  HypoKosh,
  DialecticalModelWorlds
};

enum class EpistemicEventType : uint8_t {
  HypothesisSet,
  Decision,
  Challenge,
  Reopen,
  Revision,
  Terminal
};

struct NativeEpistemicEvent {
  uint32_t sequence{0};
  uint32_t round_index{0};
  EpistemicEventSource source{EpistemicEventSource::GrapheneCore};
  EpistemicEventType type{EpistemicEventType::Decision};
  uint32_t previous_hypothesis_node{0};
  uint32_t hypothesis_node{0};
  std::vector<uint32_t> competing_hypotheses;
  std::vector<uint32_t> reopen_nodes;
  std::vector<uint32_t> evidence_edges;
  std::vector<std::string> evidence_family_ids;
  std::string epistemic_state;
  std::string reason;
};

struct RuntimeOptions {
  DialecticOptions dialectic;
  StabilityWeights stability_weights;
  StabilityThresholds stability_thresholds;
  LyapunovWeights lyapunov_weights;
  LyapunovTargets lyapunov_targets;
  const PathVerifier* path_verifier{nullptr};
  uint32_t max_recursive_cycles{2};
  bool enable_opposition_research{false};
  uint32_t unchanged_recovery_patience{2};
  // Production-owned capability boundary for competing-hypothesis reasoning.
  // G0 sets this false and must also disable DWM. Default true preserves the
  // pre-ablation production behaviour.
  bool enable_hypokosh{true};
  // Production-owned capability boundary for the DWM challenge/reopen loop.
  bool enable_dwm{true};
  bool update_model_world{true};
};

struct RecoveryRoundTrace {
  uint32_t round_index{0};
  uint32_t expansion_rounds_before{0};
  bool requires_escape{false};
  bool searchable_escape{false};
  bool recovery_search_requested{false};
  bool opposition_search_requested{false};
  bool generic_expansion_allowed{false};
  bool options_changed{false};
  bool depth_repair_active{false};
  uint32_t previous_max_hops{0};
  uint32_t next_max_hops{0};
  size_t previous_semantic_candidates{0};
  size_t next_semantic_candidates{0};
  size_t previous_max_paths{0};
  size_t next_max_paths{0};
  size_t previous_max_paths_per_root{0};
  size_t next_max_paths_per_root{0};
  size_t previous_max_visited_states{0};
  size_t next_max_visited_states{0};
  double previous_minimum_confidence{0.0};
  double next_minimum_confidence{0.0};
  size_t previous_reopen_nodes{0};
  size_t next_reopen_nodes{0};
  uint64_t previous_bundle_hash{0};
  uint64_t next_bundle_hash{0};
  size_t previous_visited_states{0};
  size_t next_visited_states{0};
  bool previous_truncated{false};
  bool next_truncated{false};

  // Read-only before/after epistemic snapshots for this recovery round.
  // These fields are diagnostic evidence only and are never consumed by
  // production control flow.
  bool previous_has_answer{false};
  bool next_has_answer{false};
  uint32_t previous_primary_node{0};
  uint32_t next_primary_node{0};
  GovernedEpistemicStatus previous_status{GovernedEpistemicStatus::Abstain};
  GovernedEpistemicStatus next_status{GovernedEpistemicStatus::Abstain};
  uint32_t previous_committed_node{0};
  uint32_t next_committed_node{0};
  std::vector<TargetEpistemicTrace> previous_target_ranking;
  std::vector<TargetEpistemicTrace> next_target_ranking;

  bool bundle_changed{false};
  bool frontier_progress{false};
  uint32_t consecutive_unchanged_bundles{0};
  bool lyapunov_limit_cycle{false};
  bool lyapunov_oscillation{false};
  std::string stop_reason;
};

struct ReasoningReceipt {
  uint64_t snapshot_version{0};
  uint64_t initial_bundle_hash{0};
  uint64_t final_bundle_hash{0};
  uint64_t model_world_event_hash{0};
  uint32_t expansion_rounds{0};
  uint32_t frontier_progress_rounds{0};
  uint32_t unchanged_bundle_rounds{0};
  bool stopped_for_no_progress{false};
  std::string recovery_stop_reason;
  std::vector<RecoveryRoundTrace> recovery_trace;
  std::vector<NativeEpistemicEvent> epistemic_events;
  bool opposition_research_enabled{false};
  // Declared capabilities and observed execution are separate evidence.
  bool hypokosh_capability_enabled{true};
  bool dwm_capability_enabled{true};
  bool graphene_executed{false};
  bool path_verifier_executed{false};
  bool fiber_bundle_built{false};
  bool fiber_bundle_authoritative{false};
  bool stability_critic_executed{false};
  bool epistemic_admissibility_executed{false};
  bool lyapunov_trajectory_executed{false};
  bool lyapunov_certificate_valid{false};
  bool lyapunov_goal_reached{false};
  bool semantic_verification_required{true};
  bool escape_considered{false};
  bool convergence_executed{false};
  bool opposition_executed{false};
  bool bounded_recovery_executed{false};
  bool governed_projection_executed{false};
  bool model_world_updated{false};
  bool no_silent_promotion{true};
  std::string terminal_cause;
};

struct HypoKoshRuntimeResult {
  FiberBundle initial_bundle;
  StabilityAssessment initial_stability;
  EpistemicAdmissibility initial_admissibility;
  EscapePlan initial_escape;
  ConvergedAnswer initial_convergence;
  OppositionReport initial_opposition;
  SelfHealingPlan initial_self_healing;

  FiberBundle final_bundle;
  StabilityAssessment final_stability;
  EpistemicAdmissibility final_admissibility;
  ConvergedAnswer final_convergence;
  OppositionReport final_opposition;
  SelfHealingPlan final_self_healing;
  LyapunovTrajectory lyapunov;

  GovernedEpistemicStatus status{GovernedEpistemicStatus::Abstain};
  uint32_t primary_node{0};
  double confidence{0.0};
  std::vector<uint32_t> evidence_edges;
  std::vector<std::string> residual_uncertainty;
  ReasoningReceipt receipt;
};

class CompleteHypoKoshRuntime {
 public:
  explicit CompleteHypoKoshRuntime(const GrapheneDB& db,
                                   ModelWorld* model_world = nullptr);

  HypoKoshRuntimeResult reason(const std::vector<float>& query,
                               uint64_t query_signature,
                               const RuntimeOptions& options = {},
                               uint64_t snapshot_version = kInfVersion) const;

 private:
  const GrapheneDB& db_;
  ModelWorld* model_world_{nullptr};
};

const char* governed_status_name(GovernedEpistemicStatus status);
const char* epistemic_event_source_name(EpistemicEventSource source);
const char* epistemic_event_type_name(EpistemicEventType type);

}  // namespace graphene
