#pragma once

#include "graphene/stability_critic.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

enum class EscapeAction : uint8_t {
  ExpandMinorityPath = 0,
  SearchContradiction = 1,
  SearchTemporalNeighbour = 2,
  SeekIndependentEvidence = 3,
  GenerateFalsificationQuestion = 4,
  GenerateMissingEvidenceQuery = 5,
  ExploreAnalogy = 6,
  RequestHumanEvidence = 7
};

struct EscapeTask {
  EscapeAction action{EscapeAction::SeekIndependentEvidence};
  uint32_t target_node{0};
  std::string rationale;
  std::string query;
};

struct EscapeBudget {
  size_t max_new_nodes{200};
  size_t max_new_edges{400};
  uint32_t max_depth_increment{2};
  size_t max_path_multiplier{4};
  size_t max_candidate_increment{8};
};

struct EscapePlan {
  std::vector<EscapeTask> tasks;
  EscapeBudget budget;
  bool requires_reexpansion{false};
  bool requests_human_evidence{false};
};

class CorrectiveEscape {
 public:
  EscapePlan plan(const FiberBundle& bundle,
                  const StabilityAssessment& assessment,
                  QueryMode mode = QueryMode::Balanced,
                  const EscapeBudget& budget = {}) const;
};

}  // namespace graphene
