#include "graphene/opposition.hpp"

#include <algorithm>
#include <cmath>
#include <set>

namespace graphene {
namespace {

void add_unique(std::vector<std::string>* values, std::string value) {
  if (std::find(values->begin(), values->end(), value) == values->end()) {
    values->push_back(std::move(value));
  }
}

}  // namespace

RuntimeOppositionReport OppositionEngine::oppose(
    const FiberBundle& bundle,
    const StabilityAssessment& stability,
    const RuntimeConvergedAnswer& converged,
    const RuntimeOppositionOptions& requested_options) const {
  RuntimeOppositionOptions options = requested_options;
  options.reopen_threshold =
      std::clamp(options.reopen_threshold, 0.0, 1.0);
  options.path_loss_alert_threshold =
      std::clamp(options.path_loss_alert_threshold, 0.0, 1.0);

  RuntimeOppositionReport output;
  if (!converged.has_answer) {
    add_unique(&output.challenged_claims,
               "no answer survived governed convergence");
    add_unique(&output.falsification_questions,
               "What source evidence would establish a defensible target?");
  }

  for (const auto& fiber : bundle.fibers) {
    if (fiber.contradiction_ratio > 0.0) {
      add_unique(&output.challenged_claims,
                 "the selected explanation coexists with contradiction paths");
      add_unique(&output.hidden_contradictions,
                 "target " + std::to_string(fiber.target_node) +
                     " has contradiction ratio " +
                     std::to_string(fiber.contradiction_ratio));
      add_unique(&output.falsification_questions,
                 "Which independent observation discriminates the contradictory paths?");
      output.reopen_queries.push_back(
          {fiber.target_node,
           "retrieve temporally overlapping evidence for both contradictory claims",
           "contradiction must be resolved rather than compressed away"});
    }
    if (fiber.independent_path_count < 2) {
      add_unique(&output.challenged_claims,
                 "the selected target lacks independent support");
      add_unique(&output.falsification_questions,
                 "Can an independent source reproduce the same conclusion?");
      output.reopen_queries.push_back(
          {fiber.target_node, "seek an independent source lineage",
           "single-source coherence is vulnerable to pattern lock"});
    }
  }

  if (stability.pattern_lock_score >= 0.70) {
    add_unique(&output.challenged_claims,
               "one path dominates before alternatives are adequately explored");
    add_unique(&output.falsification_questions,
               "Which discarded path would most strongly change the conclusion?");
  }
  if (stability.provenance_score < 0.75) {
    add_unique(&output.challenged_claims,
               "the answer relies on incomplete or unsafe provenance");
    add_unique(&output.falsification_questions,
               "Can every selected edge be tied to source evidence or a derivation chain?");
  }
  if (stability.temporal_consistency < 1.0) {
    add_unique(&output.challenged_claims,
               "the answer combines facts that are not jointly valid in time");
  }
  if (converged.false_promotion_risk > 0.0) {
    add_unique(&output.challenged_claims,
               "the selected path risks promoting inference or speculation as fact");
  }

  const size_t total_paths = converged.selected_paths.size() +
                             converged.discarded_paths.size();
  const double discarded_ratio =
      total_paths == 0
          ? 0.0
          : static_cast<double>(converged.discarded_paths.size()) /
                static_cast<double>(total_paths);
  if (discarded_ratio >= options.path_loss_alert_threshold) {
    add_unique(&output.challenged_claims,
               "convergence compressed a material share of available paths");
    output.discarded_path_alerts = converged.discarded_paths;
  }

  // Any compressed edge without an explicit derivation is a shortcut risk.
  for (uint32_t edge_id : converged.evidence_edges) {
    for (const auto& fiber : bundle.fibers) {
      for (const auto& path : fiber.paths) {
        if (std::find(path.edges.begin(), path.edges.end(), edge_id) ==
            path.edges.end()) {
          continue;
        }
        if (path.contains_inferred && path.provenance_quality < 1.0) {
          output.shortcut_risks.push_back(edge_id);
        }
      }
    }
  }
  std::sort(output.shortcut_risks.begin(), output.shortcut_risks.end());
  output.shortcut_risks.erase(
      std::unique(output.shortcut_risks.begin(), output.shortcut_risks.end()),
      output.shortcut_risks.end());

  output.opposition_score = std::clamp(
      0.12 * static_cast<double>(output.challenged_claims.size()) +
          0.08 * static_cast<double>(output.hidden_contradictions.size()) +
          0.04 * static_cast<double>(output.discarded_path_alerts.size()) +
          0.05 * static_cast<double>(output.shortcut_risks.size()) +
          0.10 * converged.false_promotion_risk,
      0.0, 1.0);
  output.reopen_required =
      output.opposition_score >= options.reopen_threshold &&
      (!output.reopen_queries.empty() ||
       !output.discarded_path_alerts.empty());
  return output;
}

}  // namespace graphene
