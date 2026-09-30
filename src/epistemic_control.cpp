#include "graphene/epistemic_control.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace graphene {
namespace {

struct Candidate {
  const TargetFiber* fiber{nullptr};
  const FiberPath* path{nullptr};
  size_t path_index{0};
  double score{0.0};
};

struct TargetCandidate {
  const TargetFiber* fiber{nullptr};
  double support_strength{0.0};
  double opposition_strength{0.0};
  double belief_strength{0.0};
  SemanticVerificationStatus semantic_verification{
      SemanticVerificationStatus::Unverified};
  size_t independent_support_count{0};
  double best_support_score{0.0};
};

double candidate_score(const FiberPath& path) {
  return std::clamp(path.confidence * path.query_relevance *
                        path.target_consistency * path.completeness *
                        path.provenance_quality,
                    0.0, 1.0);
}

std::vector<Candidate> support_candidates(const FiberBundle& bundle) {
  std::vector<Candidate> output;
  for (const auto& fiber : bundle.fibers) {
    std::set<uint64_t> representatives;
    for (const auto& group : fiber.correlation_groups) {
      if (group.role == FiberPathRole::Support &&
          group.independent_support) {
        representatives.insert(group.representative_path_id);
      }
    }
    for (size_t index = 0; index < fiber.paths.size(); ++index) {
      const FiberPath& path = fiber.paths[index];
      if (!path.eligible_for_support ||
          representatives.count(path.id) == 0) {
        continue;
      }
      output.push_back(
          {&fiber, &path, index, candidate_score(path)});
    }
  }
  std::sort(output.begin(), output.end(),
            [](const Candidate& left, const Candidate& right) {
    if (left.score != right.score) return left.score > right.score;
    if (left.fiber->target_node != right.fiber->target_node)
      return left.fiber->target_node < right.fiber->target_node;
    return left.path->id < right.path->id;
  });
  return output;
}

std::vector<Candidate> candidates_for_target(
    const std::vector<Candidate>& candidates,
    uint32_t target_node) {
  std::vector<Candidate> selected;
  for (const Candidate& candidate : candidates) {
    if (candidate.fiber->target_node == target_node) {
      selected.push_back(candidate);
    }
  }
  return selected;
}

const TargetFiber* find_target_fiber(const FiberBundle& bundle,
                                     uint32_t target_node) {
  const auto it = std::find_if(
      bundle.fibers.begin(), bundle.fibers.end(),
      [&](const TargetFiber& fiber) {
        return fiber.target_node == target_node;
      });
  return it == bundle.fibers.end() ? nullptr : &*it;
}

SemanticVerificationStatus strongest_semantic_status(
    const std::vector<Candidate>& candidates) {
  bool verified = false;
  bool contradicted = false;
  bool not_applicable = false;
  for (const auto& candidate : candidates) {
    switch (candidate.path->semantic_verification) {
      case SemanticVerificationStatus::Verified: verified = true; break;
      case SemanticVerificationStatus::Contradicted: contradicted = true; break;
      case SemanticVerificationStatus::NotApplicable: not_applicable = true; break;
      case SemanticVerificationStatus::Unverified: break;
    }
  }
  if (contradicted) return SemanticVerificationStatus::Contradicted;
  if (verified) return SemanticVerificationStatus::Verified;
  if (not_applicable) return SemanticVerificationStatus::NotApplicable;
  return SemanticVerificationStatus::Unverified;
}

int verification_rank(SemanticVerificationStatus status) {
  switch (status) {
    case SemanticVerificationStatus::Verified: return 3;
    case SemanticVerificationStatus::NotApplicable: return 2;
    case SemanticVerificationStatus::Unverified: return 1;
    case SemanticVerificationStatus::Contradicted: return 0;
  }
  return 0;
}

double independent_union_strength(const std::vector<double>& scores) {
  double residual = 1.0;
  for (double score : scores) {
    residual *= 1.0 - std::clamp(score, 0.0, 1.0);
  }
  return std::clamp(1.0 - residual, 0.0, 1.0);
}

constexpr double kSemanticTieAbsoluteTolerance = 1e-9;
constexpr double kSemanticTieRelativeTolerance = 1e-6;

bool semantic_double_equal(double left, double right) {
  const double scale = std::max(std::abs(left), std::abs(right));
  return std::abs(left - right) <=
         std::max(kSemanticTieAbsoluteTolerance,
                  kSemanticTieRelativeTolerance * scale);
}

bool semantic_target_equal(const TargetCandidate& left,
                           const TargetCandidate& right) {
  return semantic_double_equal(left.belief_strength, right.belief_strength) &&
         semantic_double_equal(left.support_strength, right.support_strength) &&
         semantic_double_equal(left.opposition_strength,
                               right.opposition_strength) &&
         left.semantic_verification == right.semantic_verification &&
         left.independent_support_count == right.independent_support_count &&
         semantic_double_equal(left.best_support_score,
                               right.best_support_score);
}

std::vector<TargetCandidate> target_candidates(const FiberBundle& bundle) {
  const auto supports = support_candidates(bundle);
  std::vector<TargetCandidate> output;
  for (const auto& fiber : bundle.fibers) {
    const auto selected = candidates_for_target(supports, fiber.target_node);
    if (selected.empty()) continue;

    std::vector<double> support_scores;
    support_scores.reserve(selected.size());
    double best_support = 0.0;
    for (const Candidate& candidate : selected) {
      support_scores.push_back(candidate.score);
      best_support = std::max(best_support, candidate.score);
    }

    std::vector<double> opposition_scores;
    for (const auto& group : fiber.correlation_groups) {
      if (group.role != FiberPathRole::Opposition) continue;
      const auto path_it = std::find_if(
          fiber.paths.begin(), fiber.paths.end(),
          [&](const FiberPath& path) {
            return path.id == group.representative_path_id;
          });
      if (path_it == fiber.paths.end() || !path_it->eligible_for_opposition) {
        continue;
      }
      opposition_scores.push_back(candidate_score(*path_it));
    }

    TargetCandidate target;
    target.fiber = &fiber;
    target.support_strength = independent_union_strength(support_scores);
    target.opposition_strength =
        independent_union_strength(opposition_scores);
    // Independent support is combined as a probability-union style strength.
    // Independent opposition then attenuates that support multiplicatively.
    // This makes duplicated/correlated paths neutral because only correlation
    // group representatives enter either side of the calculation.
    target.belief_strength = std::clamp(
        target.support_strength * (1.0 - target.opposition_strength),
        0.0, 1.0);
    target.semantic_verification = strongest_semantic_status(selected);
    target.independent_support_count =
        fiber.independent_evidence_family_count;
    target.best_support_score = best_support;
    output.push_back(target);
  }

  std::sort(output.begin(), output.end(),
            [](const TargetCandidate& left, const TargetCandidate& right) {
    if (left.belief_strength != right.belief_strength)
      return left.belief_strength > right.belief_strength;
    const int left_verification =
        verification_rank(left.semantic_verification);
    const int right_verification =
        verification_rank(right.semantic_verification);
    if (left_verification != right_verification)
      return left_verification > right_verification;
    if (left.independent_support_count != right.independent_support_count)
      return left.independent_support_count > right.independent_support_count;
    if (left.best_support_score != right.best_support_score)
      return left.best_support_score > right.best_support_score;
    return left.fiber->target_node < right.fiber->target_node;
  });
  return output;
}

const TargetCandidate* select_primary_target(
    const std::vector<TargetCandidate>& ranked_targets,
    const StabilityThresholds& thresholds = StabilityThresholds{}) {
  if (ranked_targets.empty()) return nullptr;

  const TargetCandidate* belief_leader = &ranked_targets.front();

  // V3 semantic contract: storage identity is not epistemic evidence.
  // The sort remains deterministically ordered for receipts/diagnostics, but
  // an exact semantic tie must not be converted into an operative hypothesis
  // by the final target_node ordering.
  if (ranked_targets.size() > 1 &&
      semantic_target_equal(ranked_targets[0], ranked_targets[1])) {
    return nullptr;
  }

  const TargetCandidate* support_leader = belief_leader;
  bool support_leader_tied = false;
  for (const TargetCandidate& candidate : ranked_targets) {
    if (candidate.support_strength >
        support_leader->support_strength +
            std::max(kSemanticTieAbsoluteTolerance,
                     kSemanticTieRelativeTolerance *
                         std::max(std::abs(candidate.support_strength),
                                  std::abs(support_leader->support_strength)))) {
      support_leader = &candidate;
      support_leader_tied = false;
    } else if (&candidate != support_leader &&
               semantic_double_equal(candidate.support_strength,
                                     support_leader->support_strength)) {
      support_leader_tied = true;
    }
  }

  if (belief_leader->fiber->target_node ==
          support_leader->fiber->target_node ||
      support_leader_tied) {
    return belief_leader;
  }

  if (support_leader->support_strength >
      belief_leader->support_strength) {
    if (belief_leader->independent_support_count >= 2) {
      return belief_leader;
    }

    // V3 frozen incumbent/replacement policy:
    // a materially refuted historical support leader is not restored merely
    // because the replacement has not yet earned two independent families.
    // The governed state must remain contested with no operative target until
    // the replacement earns corroboration.
    if (support_leader->opposition_strength >=
        thresholds.material_contradiction) {
      return nullptr;
    }
    return support_leader;
  }
  return belief_leader;
}

}  // namespace

std::vector<TargetEpistemicTrace> EpistemicController::inspect_targets(
    const FiberBundle& bundle) const {
  std::vector<TargetEpistemicTrace> output;
  for (const TargetCandidate& target : target_candidates(bundle)) {
    TargetEpistemicTrace trace;
    trace.target_node = target.fiber->target_node;
    trace.support_strength = target.support_strength;
    trace.opposition_strength = target.opposition_strength;
    trace.belief_strength = target.belief_strength;
    trace.semantic_verification = target.semantic_verification;
    trace.independent_support_family_count =
        target.independent_support_count;
    trace.best_support_score = target.best_support_score;
    output.push_back(trace);
  }
  return output;
}

EpistemicAdmissibility EpistemicController::assess(
    const FiberBundle& bundle,
    const StabilityAssessment& stability,
    QueryMode mode) const {
  return assess(bundle, stability, mode, StabilityThresholds{});
}

EpistemicAdmissibility EpistemicController::assess(
    const FiberBundle& bundle,
    const StabilityAssessment& stability,
    QueryMode mode,
    const StabilityThresholds& thresholds) const {
  EpistemicAdmissibility output;
  const auto candidates = support_candidates(bundle);
  const auto targets = target_candidates(bundle);
  const TargetCandidate* selected =
      select_primary_target(targets, thresholds);
  output.semantic_tie =
      !selected && targets.size() > 1 &&
      semantic_target_equal(targets[0], targets[1]);
  const uint32_t selected_target =
      selected ? selected->fiber->target_node : 0;
  const auto selected_candidates =
      candidates_for_target(candidates, selected_target);
  const TargetFiber* selected_fiber =
      selected ? find_target_fiber(bundle, selected_target) : nullptr;

  // Contradiction is normally scoped to the selected hypothesis. However, if
  // material opposition displaces a previously stronger supported target, the
  // replacement must itself earn independent corroboration before that
  // contestation can be cleared. This prevents "H1 was falsified, therefore
  // weak H2 wins" while still allowing a well-corroborated H2 to replace H1.
  output.unresolved_contradiction =
      selected_fiber ? selected_fiber->contradiction_mass : 0.0;
  // When V3 intentionally clears selection because a materially refuted
  // incumbent has not yet earned a corroborated replacement, retain the
  // contradiction signal so governed projection can emit Contested rather
  // than collapsing the state into generic abstention.
  if (!selected) {
    for (const TargetCandidate& candidate : targets) {
      if (candidate.opposition_strength >= thresholds.material_contradiction) {
        output.unresolved_contradiction =
            std::max(output.unresolved_contradiction,
                     candidate.opposition_strength);
      }
    }
  }
  double displaced_material_contradiction = 0.0;
  if (selected_fiber &&
      selected_fiber->independent_evidence_family_count < 2 &&
      selected) {
    for (const TargetCandidate& candidate : targets) {
      if (candidate.fiber->target_node == selected->fiber->target_node)
        continue;
      if (candidate.support_strength <= selected->support_strength)
        continue;
      if (candidate.opposition_strength < thresholds.material_contradiction)
        continue;
      displaced_material_contradiction =
          std::max(displaced_material_contradiction,
                   candidate.opposition_strength);
    }
  }
  output.unresolved_contradiction =
      std::max(output.unresolved_contradiction,
               displaced_material_contradiction);
  output.contradiction_blocks_resolution =
      output.unresolved_contradiction >= thresholds.material_contradiction;

  if (!selected_candidates.empty()) {
    const double count = static_cast<double>(selected_candidates.size());
    double relevance = 0.0;
    double target_consistency = 0.0;
    double completeness = 0.0;
    double provenance = 0.0;
    for (const Candidate& candidate : selected_candidates) {
      relevance += candidate.path->query_relevance;
      target_consistency += candidate.path->target_consistency;
      completeness += candidate.path->completeness;
      provenance += candidate.path->provenance_quality;
    }
    output.relevance = relevance / count;
    output.target_consistency = target_consistency / count;
    output.completeness = completeness / count;
    output.provenance = provenance / count;
  }
  output.retrieval_noise =
      selected_fiber ? selected_fiber->retrieval_noise_ratio : 0.0;

  // Verification and corroboration are properties of the selected hypothesis,
  // not of the retrieval set as a whole. A verified secondary target must never
  // promote an unverified primary target.
  output.semantic_verification =
      strongest_semantic_status(selected_candidates);
  output.independent_support = selected_fiber
      ? selected_fiber->independent_support_score
      : 0.0;
  output.sufficient_independent_support =
      selected_fiber &&
      selected_fiber->independent_evidence_family_count >= 2;

  output.evidence_admissible =
      !selected_candidates.empty() &&
      output.relevance >= thresholds.minimum_relevance &&
      output.target_consistency >= thresholds.minimum_target_consistency &&
      output.completeness >= thresholds.minimum_completeness &&
      output.provenance >= 0.25 &&
      output.retrieval_noise <= thresholds.maximum_noise &&
      !output.contradiction_blocks_resolution;
  output.requires_external_verification =
      output.semantic_verification ==
          SemanticVerificationStatus::Unverified ||
      !output.sufficient_independent_support;

  if (selected_candidates.empty())
    output.reasons.push_back("no independent support candidate is available");
  if (!output.sufficient_independent_support)
    output.reasons.push_back(
        "the selected target has fewer than two independent evidence families");
  if (output.contradiction_blocks_resolution)
    output.reasons.push_back("material contradiction blocks resolution");
  if (output.retrieval_noise > 0.0)
    output.reasons.push_back("retrieval noise was quarantined from positive support");
  if (output.completeness < 0.75)
    output.reasons.push_back("critical-path completeness remains weak");
  if (output.semantic_verification ==
      SemanticVerificationStatus::Unverified)
    output.reasons.push_back(
        "semantic correctness of the selected target has not been externally verified");
  if (output.semantic_verification ==
      SemanticVerificationStatus::Contradicted)
    output.reasons.push_back(
        "semantic verification contradicted the selected target");
  if (mode == QueryMode::Empirical && output.provenance < 0.85)
    output.reasons.push_back("empirical provenance is below the preferred threshold");
  return output;
}

ConvergedAnswer EpistemicController::converge(
    const FiberBundle& bundle,
    const EpistemicAdmissibility& admissibility,
    const StabilityAssessment& stability,
    const DialecticOptions& options,
    const StabilityThresholds& thresholds) const {
  ConvergedAnswer output;
  const auto candidates = support_candidates(bundle);
  const auto targets = target_candidates(bundle);
  if (candidates.empty() || targets.empty()) {
    output.residual_uncertainty.push_back(
        "no support-eligible FiberBundle path exists");
    return output;
  }

  const TargetCandidate* primary_target =
      select_primary_target(targets, thresholds);
  if (!primary_target) {
    output.residual_uncertainty.push_back(
        "no target earned an operative selection");
    return output;
  }
  const uint32_t primary_node = primary_target->fiber->target_node;
  const auto primary_candidates =
      candidates_for_target(candidates, primary_node);
  const Candidate& primary = primary_candidates.front();
  // has_answer means an operative candidate remains inspectable; it is not
  // equivalent to governed resolution. Opposition attenuates confidence and
  // admissibility/status can still block promotion, but a materially
  // challenged candidate must remain visible for audit and dialectic repair.
  output.has_answer =
      primary_target->support_strength >=
      std::max(0.05, options.minimum_confidence * 0.50);
  output.primary_node = primary_node;
  output.confidence = primary_target->belief_strength;
  output.false_promotion_risk =
      primary.path->contains_hypothetical ? 1.0 : 0.0;
  if (primary_target->opposition_strength > 0.0) {
    output.residual_uncertainty.push_back(
        "target-level belief strength is attenuated by independent opposition");
  }

  size_t selected = 0;
  std::set<uint32_t> evidence_edges;
  for (const auto& candidate : candidates) {
    if (candidate.fiber->target_node == output.primary_node &&
        selected < options.max_selected_paths) {
      output.selected_paths.push_back(
          {candidate.fiber->target_node, candidate.path_index,
           "selected independent FiberBundle support representative"});
      evidence_edges.insert(candidate.path->edges.begin(),
                            candidate.path->edges.end());
      ++selected;
    } else {
      output.discarded_paths.push_back(
          {candidate.fiber->target_node, candidate.path_index,
           candidate.fiber->target_node == output.primary_node
               ? "bounded convergence path limit"
               : "alternative target retained outside the selected view"});
    }
  }

  for (const auto& fiber : bundle.fibers) {
    for (size_t index = 0; index < fiber.paths.size(); ++index) {
      const FiberPath& path = fiber.paths[index];
      if (path.role == FiberPathRole::Noise) {
        output.discarded_paths.push_back(
            {fiber.target_node, index,
             "irrelevant path quarantined from convergence"});
      } else if (path.role == FiberPathRole::Opposition) {
        output.discarded_paths.push_back(
            {fiber.target_node, index,
             "opposition path retained for contestation"});
      }
    }
  }

  output.evidence_edges.assign(evidence_edges.begin(), evidence_edges.end());
  output.residual_uncertainty.insert(output.residual_uncertainty.end(),
                                     admissibility.reasons.begin(),
                                     admissibility.reasons.end());
  if (!stability.stable)
    output.residual_uncertainty.push_back(
        "reasoning dynamics have not reached the practical stability set");
  if (!admissibility.evidence_admissible)
    output.residual_uncertainty.push_back(
        "candidate evidence is not admissible for final resolution");
  return output;
}

OppositionReport EpistemicController::oppose(
    const FiberBundle& bundle,
    const ConvergedAnswer& answer,
    const EpistemicAdmissibility& admissibility,
    const StabilityAssessment& stability,
    const DialecticOptions& options) const {
  OppositionReport output;
  std::set<uint32_t> reopen;
  double strongest = 0.0;
  for (const auto& fiber : bundle.fibers) {
    for (const auto& group : fiber.correlation_groups) {
      if (group.role != FiberPathRole::Opposition) continue;
      const auto path_it = std::find_if(
          fiber.paths.begin(), fiber.paths.end(),
          [&](const FiberPath& path) {
            return path.id == group.representative_path_id;
          });
      if (path_it == fiber.paths.end() ||
          !path_it->eligible_for_opposition) {
        continue;
      }
      const double score = candidate_score(*path_it);
      strongest = std::max(strongest, score);
      output.challenged_claims.push_back(
          "independent opposition challenges target " +
          std::to_string(fiber.target_node));
      output.falsification_questions.push_back(
          "What independently sourced observation discriminates target " +
          std::to_string(fiber.target_node) + " from its opposition?");
      reopen.insert(fiber.target_node);
    }
  }

  output.opposition_score = std::max(
      strongest, admissibility.unresolved_contradiction);

  // V3 semantic split:
  // - Challenge is reserved for actual material opposition.
  // - Missing corroboration is an evidence-acquisition request, not a
  //   dialectical challenge.
  output.dialectical_challenge =
      !output.challenged_claims.empty() &&
      output.opposition_score >= options.reexpansion_threshold;

  if (!admissibility.sufficient_independent_support && answer.has_answer) {
    output.corroboration_search_required = true;
    output.corroboration_questions.push_back(
        "Which new evidence family could independently corroborate the selected answer?");
  }

  output.reopen_nodes.assign(reopen.begin(), reopen.end());
  output.requests_reexpansion =
      output.dialectical_challenge &&
      !output.reopen_nodes.empty();

  // Retrieval noise remains a recovery concern handled by the generic escape
  // planner. It is not promoted into a DWM challenge.
  (void)stability;
  return output;
}

}  // namespace graphene
