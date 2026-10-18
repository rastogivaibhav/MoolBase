#include "graphene/context_world.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace graphene {
namespace {

constexpr char kType[] = "cw_type";
constexpr char kWorld[] = "cw_world";
constexpr char kEvent[] = "cw_event";
constexpr char kTrial[] = "cw_trial";
constexpr char kFeatures[] = "cw_features";
constexpr char kSource[] = "cw_source";
constexpr char kProbability[] = "cw_probability";
constexpr char kOccurred[] = "cw_occurred";
constexpr char kFamily[] = "cw_family";
constexpr char kVerifier[] = "cw_verifier";

bool valid_key(const std::string& s) {
  if (s.empty() || s.size() > 128) return false;
  for (const char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == ':') continue;
    return false;
  }
  return true;
}

std::string metadata(const ModelWorldNode& node, const char* key) {
  const auto it = node.metadata.find(key);
  return it == node.metadata.end() ? "" : it->second;
}

bool numeric(const std::string& s, double* out) {
  if (!out || s.empty()) return false;
  try {
    size_t used = 0;
    const double x = std::stod(s, &used);
    if (used != s.size() || !std::isfinite(x)) return false;
    *out = x;
    return true;
  } catch (...) {
    return false;
  }
}

bool features_valid(const std::vector<double>& xs) {
  if (xs.size() < 2 || xs.size() > 32 || xs[0] != 1.0) return false;
  return std::all_of(xs.begin(), xs.end(), [](double x) {
    return std::isfinite(x) && std::abs(x) <= 10.0;
  });
}

std::string encode(const std::vector<double>& xs) {
  std::ostringstream s;
  s << std::setprecision(17);
  for (size_t i = 0; i < xs.size(); ++i) {
    if (i) s << ',';
    s << xs[i];
  }
  return s.str();
}

bool decode(const std::string& repr, std::vector<double>* out) {
  if (!out || repr.empty()) return false;
  std::stringstream input(repr);
  std::string item;
  std::vector<double> parsed;
  while (std::getline(input, item, ',')) {
    double value = 0.0;
    if (!numeric(item, &value)) return false;
    parsed.push_back(value);
  }
  if (!features_valid(parsed)) return false;
  *out = std::move(parsed);
  return true;
}

double sigmoid(double logit) {
  logit = std::max(-15.0, std::min(15.0, logit));
  return 1.0 / (1.0 + std::exp(-logit));
}

double predict(const std::vector<double>& weights, const std::vector<double>& features) {
  double z = 0.0;
  for (size_t j = 0; j < weights.size(); ++j) z += weights[j] * features[j];
  return sigmoid(z);
}

bool scoped(const ModelWorldNode& n, const std::string& world,
            const std::string& event) {
  return metadata(n, kWorld) == world && metadata(n, kEvent) == event;
}

struct Pending {
  uint64_t id{0};
  std::string trial;
  std::vector<double> xs;
  double p{0.5};
};

struct Replay {
  size_t count{0};
  size_t positives{0};
  double brier_sum{0.0};
  std::vector<double> weights;
  std::optional<Pending> pending;
  std::set<std::string> seen_trials;
  std::set<std::string> seen_families;
};

Replay replay(const ModelWorld& ledger, const std::string& world,
              const std::string& event) {
  Replay state;
  for (const auto& n : ledger.nodes()) {
    if (!scoped(n, world, event)) continue;
    const std::string type = metadata(n, kType);
    if (type == "forecast" && n.type == ModelWorldNodeType::Hypothesis &&
        n.status == ModelWorldStatus::Proposed &&
        n.origin == EdgeOrigin::Hypothetical) {
      if (state.pending) continue;
      const std::string trial = metadata(n, kTrial);
      if (!valid_key(trial) || state.seen_trials.count(trial) ||
          !valid_key(metadata(n, kSource))) continue;
      std::vector<double> xs;
      double declared_probability = 0.0;
      if (!decode(metadata(n, kFeatures), &xs) ||
          !numeric(metadata(n, kProbability), &declared_probability) ||
          declared_probability < 0.0 || declared_probability > 1.0)
        continue;
      if (state.weights.empty()) state.weights.assign(xs.size(), 0.0);
      if (state.weights.size() != xs.size()) continue;
      const double expected = predict(state.weights, xs);
      if (std::abs(expected - declared_probability) > 1e-10) continue;
      state.pending = Pending{n.id, trial, std::move(xs), expected};
      continue;
    }
    if (type != "outcome" || n.type != ModelWorldNodeType::Outcome ||
        n.status != ModelWorldStatus::Verified ||
        n.origin != EdgeOrigin::Observed || n.evidence_edges.empty() ||
        !state.pending) continue;
    const std::string trial = metadata(n, kTrial);
    const std::string family = metadata(n, kFamily);
    const std::string value = metadata(n, kOccurred);
    if (n.id <= state.pending->id || n.related_nodes.size() != 1 ||
        n.related_nodes[0] != state.pending->id ||
        trial != state.pending->trial ||
        (value != "true" && value != "false") ||
        !valid_key(trial) || !valid_key(family) ||
        !valid_key(metadata(n, kVerifier)) ||
        state.seen_trials.count(trial) || state.seen_families.count(family))
      continue;
    const bool y = value == "true";
    const double error = state.pending->p - (y ? 1.0 : 0.0);
    state.brier_sum += error * error;
    ++state.count;
    if (y) ++state.positives;
    state.seen_trials.insert(trial);
    state.seen_families.insert(family);
    const auto features = state.pending->xs;
    // P4 frozen SGD (intercept not regularized). Updating after outcome is
    // essential to ensure the precommitted forecast is immutable.
    for (size_t j = 0; j < state.weights.size(); ++j) {
      const double penalty = j ? 0.0005 * state.weights[j] : 0.0;
      state.weights[j] -= 0.12 * (error * features[j] + penalty);
    }
    state.pending.reset();
  }
  return state;
}

}  // namespace

ContextWorldMetrics ContextPredictiveWorld::metrics(
    const std::string& event_key) const {
  const Replay s = replay(ledger_, world_id_, event_key);
  ContextWorldMetrics m;
  m.verified_observations = s.count;
  m.positive_outcomes = s.positives;
  m.mean_brier = s.count ? s.brier_sum / static_cast<double>(s.count) : 0.0;
  m.has_pending_forecast = s.pending.has_value();
  m.weights = s.weights;
  m.next_logit_if_zero_features = s.weights.empty() ? 0.0 : s.weights[0];
  return m;
}

Status ContextPredictiveWorld::forecast(
    const std::string& event_key, const std::string& trial_id,
    const std::string& feature_source, const std::vector<double>& xs,
    ContextWorldForecast* out) {
  if (!out || !valid_key(world_id_) || !valid_key(event_key) ||
      !valid_key(trial_id) || !valid_key(feature_source) ||
      !features_valid(xs))
    return Status::error(ErrorCode::InvalidInput, "context forecast has invalid input");
  for (const auto& n : ledger_.nodes()) {
    if (scoped(n, world_id_, event_key) &&
        metadata(n, kType) == "forecast" &&
        metadata(n, kTrial) == trial_id)
      return Status::error(ErrorCode::InvalidInput, "context trial already forecast");
  }
  const Replay state = replay(ledger_, world_id_, event_key);
  if (state.pending)
    return Status::error(ErrorCode::InvalidInput, "context previous trial still pending");
  const std::vector<double> weights = state.weights.empty() ?
      std::vector<double>(xs.size(), 0.0) : state.weights;
  if (weights.size() != xs.size())
    return Status::error(ErrorCode::InvalidInput, "context feature dimension changed");
  const double p = predict(weights, xs);
  ModelWorldNode n;
  n.type = ModelWorldNodeType::Hypothesis;
  n.status = ModelWorldStatus::Proposed;
  n.origin = EdgeOrigin::Hypothetical;
  n.statement = "Pre-outcome contextual forecast: " + event_key;
  n.metadata = {{kType, "forecast"}, {kWorld, world_id_},
                {kEvent, event_key}, {kTrial, trial_id},
                {kSource, feature_source}, {kFeatures, encode(xs)},
                {kProbability, encode({p}).substr(0)},
                {"cw_policy", "frozen-p4-logistic-sgd-v1"}};
  const uint64_t id = ledger_.add(std::move(n), "pre-outcome contextual forecast");
  if (id == 0)
    return Status::error(ErrorCode::InvalidInput, "context forecast rejected");
  *out = {id, p, state.count};
  return Status::ok();
}

Status ContextPredictiveWorld::observe(
    const WorldVerifiedOutcome& input, uint64_t* outcome_node_id) {
  if (!outcome_node_id || !valid_key(world_id_) ||
      !valid_key(input.event_key) || !valid_key(input.trial_id) ||
      !valid_key(input.evidence_family_id) || !valid_key(input.verifier_id) ||
      input.evidence_edge_id == 0)
    return Status::error(ErrorCode::InvalidInput,
                         "context outcome requires asserted evidence and verifier");
  const Replay before = replay(ledger_, world_id_, input.event_key);
  if (!before.pending || before.pending->trial != input.trial_id ||
      before.seen_trials.count(input.trial_id) ||
      before.seen_families.count(input.evidence_family_id))
    return Status::error(ErrorCode::InvalidInput,
                         "context missing precommitted forecast or duplicate provenance");
  ModelWorldNode n;
  n.type = ModelWorldNodeType::Outcome;
  n.status = ModelWorldStatus::Verified;
  n.origin = EdgeOrigin::Observed;
  n.related_nodes = {before.pending->id};
  n.evidence_edges = {input.evidence_edge_id};
  n.statement = "Caller-attested contextual outcome: " + input.event_key;
  n.metadata = {{kType, "outcome"}, {kWorld, world_id_},
                {kEvent, input.event_key}, {kTrial, input.trial_id},
                {kOccurred, input.occurred ? "true" : "false"},
                {kFamily, input.evidence_family_id},
                {kVerifier, input.verifier_id}};
  const uint64_t id = ledger_.add(std::move(n), "caller-attested contextual observation");
  if (id == 0)
    return Status::error(ErrorCode::InvalidInput, "context outcome rejected");
  *outcome_node_id = id;
  return Status::ok();
}

}  // namespace graphene
