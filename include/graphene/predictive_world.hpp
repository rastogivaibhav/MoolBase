#pragma once

#include "graphene/model_world.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

// Experimental, deliberately single-world predictive baseline.
// This is not a neural model, causal identification, or an external verifier.
// A predicted probability is a frequency estimate, NOT a verified fact.
namespace graphene {

struct WorldForecast {
  uint64_t prediction_node_id{0};
  double probability{0.5};
  size_t verified_observations{0};
};

struct WorldVerifiedOutcome {
  std::string event_key;
  std::string trial_id;
  bool occurred{false};
  uint32_t evidence_edge_id{0};
  std::string evidence_family_id;
  std::string verifier_id;
};

struct WorldPredictionMetrics {
  size_t scored_predictions{0};
  size_t positive_outcomes{0};
  double mean_brier{0.0};
  double constant_half_brier{0.25};
  double next_probability{0.5};
};

// Persistent learn-from-outcome baseline attached to an existing ModelWorld.
// All probabilities are written BEFORE outcomes. The posterior is rebuilt
// from verified outcome nodes on every read, including after save/load.
// Trials sharing an evidence family cannot independently update one event.
class SinglePredictiveWorld {
 public:
  SinglePredictiveWorld(ModelWorld& ledger, std::string world_id)
      : ledger_(ledger), world_id_(std::move(world_id)) {}

  Status forecast(const std::string& event_key, const std::string& trial_id,
                  WorldForecast* out);

  // Verification is an assertion from the caller: caller MUST establish
  // authenticity of evidence_edge_id/verifier_id at the integration boundary.
  // This component prevents silent promotion but does not authenticate IDs.
  Status observe(const WorldVerifiedOutcome& observation,
                 uint64_t* outcome_node_id);

  WorldPredictionMetrics metrics(const std::string& event_key) const;

 private:
  ModelWorld& ledger_;
  std::string world_id_;
};

}  // namespace graphene
