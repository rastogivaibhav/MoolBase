#pragma once

#include "graphene/convergence.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace graphene {

struct ReopenQuery {
  uint32_t target_node{0};
  std::string query;
  std::string reason;
};

struct RuntimeOppositionReport {
  std::vector<std::string> challenged_claims;
  std::vector<RuntimePathReference> discarded_path_alerts;
  std::vector<std::string> hidden_contradictions;
  std::vector<uint32_t> shortcut_risks;
  std::vector<std::string> falsification_questions;
  std::vector<ReopenQuery> reopen_queries;
  double opposition_score{0.0};
  bool reopen_required{false};
};

struct RuntimeOppositionOptions {
  double reopen_threshold{0.25};
  double path_loss_alert_threshold{0.20};
};

class OppositionEngine {
 public:
  RuntimeOppositionReport oppose(
      const FiberBundle& bundle,
      const StabilityAssessment& stability,
      const RuntimeConvergedAnswer& converged,
      const RuntimeOppositionOptions& options = {}) const;
};

}  // namespace graphene
