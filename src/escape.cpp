#include "graphene/escape.hpp"

#include <set>

namespace graphene {

EscapePlan CorrectiveEscape::plan(const FiberBundle& bundle,
                                  const StabilityAssessment& assessment,
                                  QueryMode mode) const {
  EscapePlan output;
  std::set<EscapeAction> added;
  const uint32_t target = bundle.fibers.empty() ? 0 : bundle.fibers.front().target_node;
  auto add = [&](EscapeAction action, const std::string& reason) {
    if (added.insert(action).second) output.tasks.push_back({action, target, reason});
  };

  if (assessment.path_diversity < 0.25 || assessment.pattern_lock_score > 0.55) {
    add(EscapeAction::ExpandMinorityPath, "reopen paths hidden by dominant-pattern convergence");
    add(EscapeAction::SeekIndependentEvidence, "seek evidence from a source-independent route");
  }
  if (assessment.contradiction_score > 0.0) {
    add(EscapeAction::SearchContradiction, "resolve or preserve the strongest contradiction");
    add(EscapeAction::GenerateFalsificationQuestion, "define a test that discriminates competing explanations");
  }
  if (assessment.temporal_consistency < 1.0) {
    add(EscapeAction::SearchTemporalNeighbour, "find evidence valid at the requested query time");
  }
  if (assessment.missing_evidence_penalty > 0.0 || assessment.provenance_score < 0.75) {
    add(EscapeAction::GenerateMissingEvidenceQuery, "identify the unsupported edge or missing source reference");
  }
  if (!assessment.lyapunov_goal_reached || assessment.lyapunov_energy > 0.25) {
    add(EscapeAction::SeekIndependentEvidence,
        "reduce Lyapunov energy by adding a source-independent path rather than reinforcing the current pattern");
  }
  if (mode == QueryMode::Theoretical && assessment.path_diversity < 0.50) {
    add(EscapeAction::ExploreAnalogy, "explore a labelled analogical bridge without promoting it to truth");
  }
  if (assessment.requires_abstention || assessment.lyapunov_energy > 0.70) {
    add(EscapeAction::RequestHumanEvidence,
        "available evidence remains outside the governed Lyapunov stability set");
    output.requires_human_evidence = true;
  }
  return output;
}

}  // namespace graphene
