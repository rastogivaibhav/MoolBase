#include "graphene/predictive_world.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <set>
#include <utility>
#include <unordered_map>

namespace graphene {
namespace {

constexpr const char* kType = "pw_v1_type";
constexpr const char* kWorld = "pw_v1_world";
constexpr const char* kEvent = "pw_v1_event";
constexpr const char* kTrial = "pw_v1_trial";
constexpr const char* kProbability = "pw_v1_probability";
constexpr const char* kOccurred = "pw_v1_occurred";
constexpr const char* kFamily = "pw_v1_family";
constexpr const char* kVerifier = "pw_v1_verifier";

bool valid_key(const std::string& s) {
  if (s.empty() || s.size() > 128) return false;
  for (const char ch : s) {
    if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
          (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
          ch == '.' || ch == ':')) return false;
  }
  return true;
}

std::string field(const ModelWorldNode& n, const char* key) {
  const auto it = n.metadata.find(key);
  return it == n.metadata.end() ? "" : it->second;
}

std::string decimal(double value) {
  std::ostringstream output;
  output << std::setprecision(17) << value;
  return output.str();
}

bool prediction_for(const ModelWorldNode& n, const std::string& world,
                    const std::string& event, const std::string& trial) {
  return n.type == ModelWorldNodeType::Hypothesis &&
         field(n, kType) == "prediction" &&
         field(n, kWorld) == world && field(n, kEvent) == event &&
         field(n, kTrial) == trial;
}

bool verified_outcome_for(const ModelWorldNode& n, const std::string& world,
                          const std::string& event) {
  return n.type == ModelWorldNodeType::Outcome &&
         n.status == ModelWorldStatus::Verified &&
         n.origin == EdgeOrigin::Observed &&
         !n.evidence_edges.empty() &&
         field(n, kType) == "outcome" &&
         field(n, kWorld) == world && field(n, kEvent) == event &&
         !field(n, kVerifier).empty();
}

bool parse_probability(const std::string& encoded, double* probability) {
  if (!probability || encoded.empty()) return false;
  try {
    size_t used = 0;
    const double value = std::stod(encoded, &used);
    if (used != encoded.size() || !std::isfinite(value) ||
        value < 0.0 || value > 1.0) return false;
    *probability = value;
    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace

WorldPredictionMetrics SinglePredictiveWorld::metrics(
    const std::string& event_key) const {
  WorldPredictionMetrics result;
  std::set<std::string> seen_trial_ids;
  std::set<std::string> seen_evidence_families;
  // Research P3 performance-only index: previous nested scan was O(n^2)
  // per forecast. Preserve the earliest matching prediction in ledger order.
  // Original integrity predicates and numerical updates are unchanged.
  std::unordered_map<std::string, std::vector<const ModelWorldNode*>>
      forecasts_by_trial;
  for (const auto& candidate : ledger_.nodes()) {
    if (candidate.type == ModelWorldNodeType::Hypothesis &&
        field(candidate, kType) == "prediction" &&
        field(candidate, kWorld) == world_id_ &&
        field(candidate, kEvent) == event_key)
      forecasts_by_trial[field(candidate, kTrial)].push_back(&candidate);
  }
  for (const auto& n : ledger_.nodes()) {
    if (!verified_outcome_for(n, world_id_, event_key)) continue;
    if (field(n, kOccurred) != "true" &&
        field(n, kOccurred) != "false") continue;

    const std::string trial = field(n, kTrial);
    const ModelWorldNode* prediction = nullptr;
    const auto matching = forecasts_by_trial.find(trial);
    if (matching != forecasts_by_trial.end()) {
      for (const ModelWorldNode* candidate : matching->second) {
        if (std::find(n.related_nodes.begin(), n.related_nodes.end(),
                      candidate->id) != n.related_nodes.end()) {
          prediction = candidate;
          break;
        }
      }
    }
    // Defensive replay: a forged/corrupted ledger record must not inflate
    // learning by reusing a trial or evidence family, or by inventing a
    // prediction created only AFTER the claimed observation.
    if (!prediction || prediction->id >= n.id) continue;
    const std::string family = field(n, kFamily);
    if (!valid_key(trial) || !valid_key(family)) continue;
    if (seen_trial_ids.count(trial) > 0 ||
        seen_evidence_families.count(family) > 0) continue;

    double p = 0.0;
    if (!parse_probability(field(*prediction, kProbability), &p)) continue;
    seen_trial_ids.insert(trial);
    seen_evidence_families.insert(family);
    const bool occurred = field(n, kOccurred) == "true";
    const double error = p - (occurred ? 1.0 : 0.0);
    result.mean_brier += error * error;
    ++result.scored_predictions;
    if (occurred) ++result.positive_outcomes;
  }
  if (result.scored_predictions > 0) {
    result.mean_brier /= static_cast<double>(result.scored_predictions);
  }
  // Beta(1,1) prior, updated only using recorded verified outcomes.
  result.next_probability =
      (1.0 + static_cast<double>(result.positive_outcomes)) /
      (2.0 + static_cast<double>(result.scored_predictions));
  return result;
}

Status SinglePredictiveWorld::forecast(const std::string& event_key,
                                       const std::string& trial_id,
                                       WorldForecast* out) {
  if (!out || !valid_key(world_id_) || !valid_key(event_key) ||
      !valid_key(trial_id)) {
    return Status::error(ErrorCode::InvalidInput,
                         "invalid predictive world forecast identity");
  }
  for (const auto& node : ledger_.nodes()) {
    if (prediction_for(node, world_id_, event_key, trial_id)) {
      return Status::error(ErrorCode::InvalidInput,
                           "prediction for this trial already exists");
    }
  }
  const auto snapshot = metrics(event_key);
  ModelWorldNode prediction;
  prediction.type = ModelWorldNodeType::Hypothesis;
  prediction.status = ModelWorldStatus::Proposed;
  prediction.origin = EdgeOrigin::Hypothetical;
  prediction.statement = "Pre-outcome forecast for " + event_key;
  prediction.metadata = {
      {kType, "prediction"},
      {kWorld, world_id_},
      {kEvent, event_key},
      {kTrial, trial_id},
      {kProbability, decimal(snapshot.next_probability)}};
  const uint64_t id = ledger_.add(std::move(prediction), "pre-outcome forecast");
  if (id == 0) {
    return Status::error(ErrorCode::InvalidInput,
                         "predictive world rejected proposed forecast");
  }
  *out = {id, snapshot.next_probability, snapshot.scored_predictions};
  return Status::ok();
}

Status SinglePredictiveWorld::observe(const WorldVerifiedOutcome& input,
                                      uint64_t* outcome_node_id) {
  if (!outcome_node_id || !valid_key(world_id_) ||
      !valid_key(input.event_key) || !valid_key(input.trial_id) ||
      !valid_key(input.evidence_family_id) ||
      !valid_key(input.verifier_id) || input.evidence_edge_id == 0) {
    return Status::error(ErrorCode::InvalidInput,
                         "outcome requires valid identities, verifier and evidence");
  }

  uint64_t prediction_id = 0;
  for (const auto& node : ledger_.nodes()) {
    if (prediction_for(node, world_id_, input.event_key, input.trial_id))
      prediction_id = node.id;
    if (!verified_outcome_for(node, world_id_, input.event_key)) continue;
    if (field(node, kTrial) == input.trial_id) {
      return Status::error(ErrorCode::InvalidInput,
                           "verified outcome for this trial already exists");
    }
    if (field(node, kFamily) == input.evidence_family_id) {
      return Status::error(ErrorCode::InvalidInput,
                           "correlated outcome family cannot be counted twice");
    }
  }
  if (prediction_id == 0) {
    return Status::error(ErrorCode::InvalidInput,
                         "outcome cannot precede its frozen forecast");
  }

  ModelWorldNode observed;
  observed.type = ModelWorldNodeType::Outcome;
  observed.status = ModelWorldStatus::Verified;
  observed.origin = EdgeOrigin::Observed;
  observed.statement = "Attested outcome for " + input.event_key;
  observed.related_nodes = {prediction_id};
  observed.evidence_edges = {input.evidence_edge_id};
  observed.metadata = {
      {kType, "outcome"},
      {kWorld, world_id_},
      {kEvent, input.event_key},
      {kTrial, input.trial_id},
      {kOccurred, input.occurred ? "true" : "false"},
      {kFamily, input.evidence_family_id},
      {kVerifier, input.verifier_id}};
  const uint64_t id = ledger_.add(std::move(observed),
                                  "externally attested observed outcome");
  if (id == 0) {
    return Status::error(ErrorCode::InvalidInput,
                         "verified outcome rejected by model-world truth gate");
  }
  *outcome_node_id = id;
  return Status::ok();
}

}  // namespace graphene
