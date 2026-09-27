#include "graphene/epistemic_control.hpp"

#include <algorithm>
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

}  // namespace

EpistemicAdmissibility EpistemicController::assess(
    const FiberBundle& bundle,
    const StabilityAssessment& stability,
    QueryMode mode,
    const StabilityThresholds& thresholds) const {
  EpistemicAdmissibility output;
  const auto candidates = support_candidates(bundle);
  const auto targets = target_candidates(bundle);
  const uint32_t selected_target =
      targets.empty() ? 0 : targets.front().fiber->target_node;
  const auto selected_candidates =
      candidates_for_target(candidates, selected_target);
  const TargetFiber* selected_fiber =
      targets.empty() ? nullptr : find_target_fiber(bundle, selected_target);

  // Contradiction is a property of the selected hypothesis, not the retrieval
  // set as a whole. Opposition against a discarded alternative must not block
  // an independently supported primary target.
  output.unresolved_contradiction =
      selected_fiber ? selected_fiber->contradiction_mass : 0.0;
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
    const DialecticOptions& options) const {
  ConvergedAnswer output;
  const auto candidates = support_candidates(bundle);
  const auto targets = target_candidates(bundle);
  if (candidates.empty() || targets.empty()) {
    output.residual_uncertainty.push_back(
        "no support-eligible FiberBundle path exists");
    return output;
  }

  const TargetCandidate& primary_target = targets.front();
  const uint32_t primary_node = primary_target.fiber->target_node;
  const auto primary_candidates =
      candidates_for_target(candidates, primary_node);
  const Candidate& primary = primary_candidates.front();
  output.has_answer =
      primary_target.belief_strength >=
      std::max(0.05, options.minimum_confidence * 0.50);
  output.primary_node = primary_node;
  output.confidence = primary_target.belief_strength;
  output.false_promotion_risk =
      primary.path->contains_hypothetical ? 1.0 : 0.0;
  if (primary_target.opposition_strength > 0.0) {
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

  // A separately supported target is not discarded noise. It is a competing
  // hypothesis and must remain visible to opposition and falsification. Use
  // the same target-level aggregate used by convergence rather than the best
  // individual support path.
  if (answer.has_answer) {
    const auto ranked_targets = target_candidates(bundle);
    for (const TargetCandidate& candidate : ranked_targets) {
      if (candidate.fiber->target_node == answer.primary_node) continue;
      strongest = std::max(strongest, candidate.belief_strength);
      output.challenged_claims.push_back(
          "independently supported alternative target " +
          std::to_string(candidate.fiber->target_node) +
          " competes with selected target " +
          std::to_string(answer.primary_node));
      output.falsification_questions.push_back(
          "Which observation discriminates selected target " +
          std::to_string(answer.primary_node) + " from alternative target " +
          std::to_string(candidate.fiber->target_node) + "?");
      reopen.insert(answer.primary_node);
      reopen.insert(candidate.fiber->target_node);
    }
  }

  output.opposition_score = std::max(
      strongest, admissibility.unresolved_contradiction);
  if (stability.retrieval_noise_penalty > 0.0) {
    output.challenged_claims.push_back(
        "retrieval included paths unrelated to the selected target");
  }
  if (!admissibility.sufficient_independent_support && answer.has_answer) {
    output.falsification_questions.push_back(
        "Which new evidence family could independently corroborate the selected answer?");
    reopen.insert(answer.primary_node);
  }
  output.reopen_nodes.assign(reopen.begin(), reopen.end());
  output.requests_reexpansion =
      output.opposition_score >= options.reexpansion_threshold ||
      !admissibility.evidence_admissible ||
      !admissibility.sufficient_independent_support ||
      stability.retrieval_noise_penalty > 0.10;
  return output;
}

}  // namespace graphene
