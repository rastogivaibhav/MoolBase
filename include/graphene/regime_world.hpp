#pragma once

#include "graphene/predictive_world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace graphene {

// P2 RESEARCH ONLY: bounded binary-frequency regime learner.
// Regime alarms are inferred hypotheses, not externally verified facts.
// All labels and evidence/verifier identifiers are caller assertions.
struct RegimeWorldForecast {
  uint64_t prediction_node_id{0};
  double adaptive_probability{0.5};
  double lifetime_probability{0.5};
  size_t verified_observations{0};
  size_t regime_changes{0};
};

struct RegimeWorldReport {
  size_t verified_observations{0};
  size_t regime_changes{0};
  double adaptive_brier{0.0};
  double lifetime_brier{0.0};
  double next_adaptive_probability{0.5};
  double next_lifetime_probability{0.5};
  bool has_pending_forecast{false};
};

// Fixed P2 preregistration: Beta(1,1) active-regime estimate; after two
// independent consecutive surprises at <=.25/>=.75 in the same direction,
// reset to Beta(1+2*y, 1+2*(1-y)). Exact frozen values, no auto-tuning.
class RegimePredictiveWorld {
 public:
  RegimePredictiveWorld(ModelWorld& ledger, std::string world_id)
      : ledger_(ledger), world_id_(std::move(world_id)) {}

  Status forecast(const std::string& event_key, const std::string& trial_id,
                  RegimeWorldForecast* out);
  Status observe(const WorldVerifiedOutcome& observation,
                 uint64_t* outcome_node_id);
  RegimeWorldReport report(const std::string& event_key) const;

 private:
  ModelWorld& ledger_;
  std::string world_id_;
};

}  // namespace graphene
