#pragma once

#include "graphene/predictive_world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace graphene {

// Generic, explicit-feature online logistic learner for prequential experiments.
// Features are caller-supplied, not secretly gathered: upstream runner MUST
// establish their source date precedes the predicted observation.
struct ContextWorldForecast {
  uint64_t prediction_node_id{0};
  double probability{0.5};
  size_t verified_observations{0};
};

struct ContextWorldMetrics {
  size_t verified_observations{0};
  size_t positive_outcomes{0};
  double mean_brier{0.0};
  double next_logit_if_zero_features{0.0};
  bool has_pending_forecast{false};
  std::vector<double> weights;
};

class ContextPredictiveWorld {
 public:
  ContextPredictiveWorld(ModelWorld& ledger, std::string world_id)
      : ledger_(ledger), world_id_(std::move(world_id)) {}

  Status forecast(const std::string& event_key, const std::string& trial_id,
                  const std::string& feature_source, const std::vector<double>& features,
                  ContextWorldForecast* out);
  Status observe(const WorldVerifiedOutcome& observation, uint64_t* outcome_node_id);
  ContextWorldMetrics metrics(const std::string& event_key) const;

 private:
  ModelWorld& ledger_;
  std::string world_id_;
};

}  // namespace graphene
