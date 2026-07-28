#include "graphene/self_healing.hpp"

#include <algorithm>
#include <set>

namespace graphene {

SelfHealingPlan RecursiveSelfHealingController::plan(
    const StabilityAssessment& stability,
    const OppositionReport& opposition,
    const EscapePlan& escape) const {
  SelfHealingPlan output;
  output.observations = stability.reasons;
  output.observations.push_back(
      "Lyapunov energy=" + std::to_string(stability.lyapunov_energy) +
      ", state_norm=" + std::to_string(stability.lyapunov_state_norm));
  output.critiques = opposition.challenged_claims;
  output.discovery_questions = opposition.falsification_questions;
  std::set<SafeRepairAction> added;
  auto add = [&](SafeRepairAction action, const std::string& reason) {
    if (added.insert(action).second) output.repairs.push_back({action, reason, false});
  };

  if (stability.pattern_lock_score > 0.55 || stability.path_diversity < 0.25) {
    add(SafeRepairAction::PreserveMinorityPath,
        "retain minority paths and widen the bounded expansion before convergence");
  }
  if (stability.missing_evidence_penalty > 0.0 || stability.provenance_score < 0.75) {
    add(SafeRepairAction::RequestSourceEvidence,
        "request source evidence for unsupported or weakly provenanced edges");
  }
  if (stability.contradiction_score > 0.0 || opposition.opposition_score >= 0.50) {
    add(SafeRepairAction::MarkContested,
        "preserve the conclusion as contested until discriminating evidence arrives");
  }
  if (stability.temporal_consistency < 1.0) {
    add(SafeRepairAction::SearchTemporalWindow,
        "rerun retrieval at the requested validity window");
  }
  if (!opposition.falsification_questions.empty() ||
      !stability.lyapunov_goal_reached) {
    add(SafeRepairAction::GenerateIndependentTest,
        "execute or request an independent test that can reduce the dominant Lyapunov error coordinate");
  }
  if (stability.requires_abstention) {
    add(SafeRepairAction::StopAndAbstain,
        "stop rather than converting missing evidence into a fluent answer");
    output.human_review_required = true;
  }
  for (const auto& task : escape.tasks) {
    if (task.action == EscapeAction::RequestHumanEvidence) output.human_review_required = true;
  }
  output.rerun_required = stability.requires_escape || opposition.requests_reexpansion;
  return output;
}

}  // namespace graphene
