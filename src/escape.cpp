#include "graphene/escape.hpp"

#include <algorithm>

namespace graphene {
namespace {

void add_task(std::vector<EscapeTask>* tasks,
              EscapeAction action,
              uint32_t target,
              std::string rationale,
              std::string query) {
  const auto duplicate = std::find_if(
      tasks->begin(), tasks->end(), [&](const EscapeTask& task) {
        return task.action == action && task.target_node == target &&
               task.query == query;
      });
  if (duplicate == tasks->end()) {
    tasks->push_back({action, target, std::move(rationale), std::move(query)});
  }
}

}  // namespace

EscapePlan CorrectiveEscape::plan(const FiberBundle& bundle,
                                  const StabilityAssessment& assessment,
                                  QueryMode mode,
                                  const EscapeBudget& requested_budget) const {
  EscapePlan output;
  output.budget = requested_budget;
  output.budget.max_new_nodes =
      std::clamp<size_t>(output.budget.max_new_nodes, 1, 5000);
  output.budget.max_new_edges =
      std::clamp<size_t>(output.budget.max_new_edges, 1, 10000);
  output.budget.max_depth_increment =
      std::clamp<uint32_t>(output.budget.max_depth_increment, 1, 4);
  output.budget.max_path_multiplier =
      std::clamp<size_t>(output.budget.max_path_multiplier, 1, 8);
  output.budget.max_candidate_increment =
      std::clamp<size_t>(output.budget.max_candidate_increment, 1, 32);

  for (const auto& fiber : bundle.fibers) {
    if (fiber.independent_path_count < 2) {
      add_task(&output.tasks, EscapeAction::SeekIndependentEvidence,
               fiber.target_node,
               "the target lacks two independent source lineages",
               "find an independent evidence path for target " +
                   std::to_string(fiber.target_node));
    }
    if (fiber.contradiction_ratio > 0.0) {
      add_task(&output.tasks, EscapeAction::SearchContradiction,
               fiber.target_node,
               "contradictory paths remain unresolved",
               "find evidence that discriminates contradictory claims for target " +
                   std::to_string(fiber.target_node));
      add_task(&output.tasks, EscapeAction::GenerateFalsificationQuestion,
               fiber.target_node,
               "the current explanation must survive deliberate attack",
               "what observation would falsify target " +
                   std::to_string(fiber.target_node) + "?");
    }
    if (fiber.path_diversity < 0.25) {
      add_task(&output.tasks, EscapeAction::ExpandMinorityPath,
               fiber.target_node,
               "retrieved paths share too much structure",
               "expand a structurally distinct path for target " +
                   std::to_string(fiber.target_node));
    }
  }

  for (const auto& missing : bundle.missing_evidence) {
    add_task(&output.tasks, EscapeAction::GenerateMissingEvidenceQuery,
             missing.target_node, missing.requirement,
             "obtain source evidence: " + missing.requirement);
  }

  if (assessment.temporal_consistency < 1.0) {
    for (const auto& fiber : bundle.fibers) {
      add_task(&output.tasks, EscapeAction::SearchTemporalNeighbour,
               fiber.target_node,
               "a path is not valid at the requested time",
               "search temporally valid predecessor or successor evidence for target " +
                   std::to_string(fiber.target_node));
    }
  }

  if (assessment.pattern_lock_score >= 0.70) {
    for (const auto& fiber : bundle.fibers) {
      add_task(&output.tasks, EscapeAction::ExpandMinorityPath,
               fiber.target_node,
               "the current path distribution is dominated by one explanation",
               "reopen discarded and low-score paths for target " +
                   std::to_string(fiber.target_node));
    }
  }

  if (mode == QueryMode::Theoretical && assessment.requires_escape) {
    for (const auto& fiber : bundle.fibers) {
      add_task(&output.tasks, EscapeAction::ExploreAnalogy,
               fiber.target_node,
               "theoretical mode permits labelled analogy exploration",
               "find a structurally similar but explicitly hypothetical path for target " +
                   std::to_string(fiber.target_node));
    }
  }

  if (assessment.requires_abstention && output.tasks.empty()) {
    output.requests_human_evidence = true;
    add_task(&output.tasks, EscapeAction::RequestHumanEvidence, 0,
             "the system cannot establish a defensible path",
             "request a human-supplied source or constraint");
  }

  output.requires_reexpansion = assessment.requires_escape &&
                                !output.tasks.empty() &&
                                !output.requests_human_evidence;
  return output;
}

}  // namespace graphene
