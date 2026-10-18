#include "graphene/regime_world.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>

namespace graphene {
namespace {

constexpr const char* kType = "pw_v2_type";
constexpr const char* kWorld = "pw_v2_world";
constexpr const char* kEvent = "pw_v2_event";
constexpr const char* kTrial = "pw_v2_trial";
constexpr const char* kProb = "pw_v2_adaptive_probability";
constexpr const char* kLifetime = "pw_v2_lifetime_probability";
constexpr const char* kRegime = "pw_v2_regime";
constexpr const char* kOccurred = "pw_v2_occurred";
constexpr const char* kFamily = "pw_v2_family";
constexpr const char* kVerifier = "pw_v2_verifier";

bool key_valid(const std::string& s) {
  if (s.empty() || s.size() > 128) return false;
  for (char ch : s) {
    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
        (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
        ch == '.' || ch == ':') continue;
    return false;
  }
  return true;
}

std::string meta(const ModelWorldNode& n, const char* key) {
  const auto it = n.metadata.find(key);
  return it == n.metadata.end() ? "" : it->second;
}

std::string decimal(double x) {
  std::ostringstream out;
  out << std::setprecision(17) << x;
  return out.str();
}

bool probability(const std::string& s, double* out) {
  if (!out || s.empty()) return false;
  try {
    size_t used = 0;
    const double x = std::stod(s, &used);
    if (used != s.size() || !std::isfinite(x) || x < 0.0 || x > 1.0)
      return false;
    *out = x;
    return true;
  } catch (...) {
    return false;
  }
}

bool integer(const std::string& s, size_t* out) {
  if (!out || s.empty()) return false;
  try {
    size_t used = 0;
    auto x = std::stoull(s, &used);
    if (used != s.size()) return false;
    *out = static_cast<size_t>(x);
    return true;
  } catch (...) {
    return false;
  }
}

bool in_scope(const ModelWorldNode& n, const std::string& world,
              const std::string& event) {
  return meta(n, kWorld) == world && meta(n, kEvent) == event;
}

struct Pending {
  uint64_t node_id{0};
  std::string trial;
  double adaptive{0.5};
  double lifetime{0.5};
};

struct Replay {
  double alpha{1.0};
  double beta{1.0};
  double life_alpha{1.0};
  double life_beta{1.0};
  size_t count{0};
  size_t changes{0};
  size_t surprise_streak{0};
  int last_surprise{-1};
  uint64_t last_surprise_node{0};
  uint32_t last_surprise_edge{0};
  double adaptive_loss_sum{0.0};
  double lifetime_loss_sum{0.0};
  std::optional<Pending> pending;
  std::set<std::string> seen_trials;
  std::set<std::string> seen_families;
};

double adaptive_probability(const Replay& state) {
  return state.alpha / (state.alpha + state.beta);
}
double lifetime_probability(const Replay& state) {
  return state.life_alpha / (state.life_alpha + state.life_beta);
}

bool surprising(bool y, double p) {
  return y ? p <= 0.25 : p >= 0.75;
}

// Reconstruct from append-only World nodes; never look at observations from
// future trials, and never count model-generated regime-change hypotheses.
// Mismatched, duplicated, source-correlated and backdated entries are ignored.
Replay replay(const ModelWorld& ledger, const std::string& world,
              const std::string& event) {
  Replay s;
  for (const auto& n : ledger.nodes()) {
    if (!in_scope(n, world, event)) continue;
    if (meta(n, kType) == "forecast" &&
        n.type == ModelWorldNodeType::Hypothesis &&
        n.status == ModelWorldStatus::Proposed &&
        n.origin == EdgeOrigin::Hypothetical) {
      if (s.pending) continue;  // one unfinished trial per event
      const std::string trial = meta(n, kTrial);
      double p = 0.0, q = 0.0;
      size_t declared_regime = 0;
      if (!key_valid(trial) || s.seen_trials.count(trial) ||
          !probability(meta(n, kProb), &p) ||
          !probability(meta(n, kLifetime), &q) ||
          !integer(meta(n, kRegime), &declared_regime) ||
          declared_regime != s.changes ||
          std::abs(p - adaptive_probability(s)) > 1e-12 ||
          std::abs(q - lifetime_probability(s)) > 1e-12) continue;
      s.pending = Pending{n.id, trial, p, q};
      continue;
    }
    if (meta(n, kType) != "outcome" ||
        n.type != ModelWorldNodeType::Outcome ||
        n.status != ModelWorldStatus::Verified ||
        n.origin != EdgeOrigin::Observed ||
        n.evidence_edges.empty() || !s.pending) continue;

    const std::string trial = meta(n, kTrial);
    const std::string family = meta(n, kFamily);
    const std::string outcome = meta(n, kOccurred);
    const bool valid_outcome = outcome == "true" || outcome == "false";
    if (n.id <= s.pending->node_id ||
        n.related_nodes.size() != 1 ||
        n.related_nodes.front() != s.pending->node_id ||
        trial != s.pending->trial || !valid_outcome ||
        !key_valid(trial) || !key_valid(family) ||
        !key_valid(meta(n, kVerifier)) ||
        s.seen_trials.count(trial) || s.seen_families.count(family))
      continue;

    const bool y = outcome == "true";
    const double p = s.pending->adaptive;
    const double q = s.pending->lifetime;
    s.adaptive_loss_sum += (p - (y ? 1.0 : 0.0)) *
                           (p - (y ? 1.0 : 0.0));
    s.lifetime_loss_sum += (q - (y ? 1.0 : 0.0)) *
                           (q - (y ? 1.0 : 0.0));
    ++s.count;
    s.seen_trials.insert(trial);
    s.seen_families.insert(family);
    s.pending.reset();

    if (surprising(y, p)) {
      if (s.last_surprise == static_cast<int>(y))
        ++s.surprise_streak;
      else
        s.surprise_streak = 1;
      s.last_surprise = static_cast<int>(y);
      s.last_surprise_node = n.id;
      s.last_surprise_edge = n.evidence_edges.front();
    } else {
      s.surprise_streak = 0;
      s.last_surprise = -1;
      s.last_surprise_node = 0;
      s.last_surprise_edge = 0;
    }
    s.alpha += y ? 1.0 : 0.0;
    s.beta += y ? 0.0 : 1.0;
    s.life_alpha += y ? 1.0 : 0.0;
    s.life_beta += y ? 0.0 : 1.0;
    if (s.surprise_streak >= 2) {
      ++s.changes;
      // The two independent triggering observations seed the new regime.
      s.alpha = 1.0 + (y ? 2.0 : 0.0);
      s.beta = 1.0 + (y ? 0.0 : 2.0);
      s.surprise_streak = 0;
      s.last_surprise = -1;
      s.last_surprise_node = 0;
      s.last_surprise_edge = 0;
    }
  }
  return s;
}

}  // namespace

RegimeWorldReport RegimePredictiveWorld::report(
    const std::string& event_key) const {
  const Replay s = replay(ledger_, world_id_, event_key);
  RegimeWorldReport r;
  r.verified_observations = s.count;
  r.regime_changes = s.changes;
  r.adaptive_brier =
      s.count ? s.adaptive_loss_sum / static_cast<double>(s.count) : 0.0;
  r.lifetime_brier =
      s.count ? s.lifetime_loss_sum / static_cast<double>(s.count) : 0.0;
  r.next_adaptive_probability = adaptive_probability(s);
  r.next_lifetime_probability = lifetime_probability(s);
  r.has_pending_forecast = s.pending.has_value();
  return r;
}

Status RegimePredictiveWorld::forecast(const std::string& event_key,
                                       const std::string& trial_id,
                                       RegimeWorldForecast* out) {
  if (!out || !key_valid(world_id_) || !key_valid(event_key) ||
      !key_valid(trial_id))
    return Status::error(ErrorCode::InvalidInput,
                         "invalid regime forecast identity");
  // Never allow a caller to overwrite, replay, or backfill a prior trial.
  for (const auto& node : ledger_.nodes()) {
    if (in_scope(node, world_id_, event_key) &&
        meta(node, kType) == "forecast" &&
        meta(node, kTrial) == trial_id)
      return Status::error(ErrorCode::InvalidInput,
                           "regime trial forecast already exists");
  }
  const Replay s = replay(ledger_, world_id_, event_key);
  if (s.pending)
    return Status::error(ErrorCode::InvalidInput,
                         "previous regime trial has no verified outcome");
  const double p = adaptive_probability(s);
  const double q = lifetime_probability(s);
  ModelWorldNode node;
  node.type = ModelWorldNodeType::Hypothesis;
  node.status = ModelWorldStatus::Proposed;
  node.origin = EdgeOrigin::Hypothetical;
  node.statement = "Regime-aware binary event forecast: " + event_key;
  node.metadata = {{kType, "forecast"},
                   {kWorld, world_id_},
                   {kEvent, event_key},
                   {kTrial, trial_id},
                   {kProb, decimal(p)},
                   {kLifetime, decimal(q)},
                   {kRegime, std::to_string(s.changes)},
                   {"pw_v2_policy", "p2-fixed-surprise-0.25x2"}};
  const uint64_t id = ledger_.add(std::move(node),
                                   "pre-outcome regime forecast");
  if (id == 0)
    return Status::error(ErrorCode::InvalidInput,
                         "regime forecast insertion rejected");
  *out = {id, p, q, s.count, s.changes};
  return Status::ok();
}

Status RegimePredictiveWorld::observe(const WorldVerifiedOutcome& input,
                                      uint64_t* outcome_node_id) {
  if (!outcome_node_id || !key_valid(world_id_) ||
      !key_valid(input.event_key) || !key_valid(input.trial_id) ||
      !key_valid(input.evidence_family_id) || !key_valid(input.verifier_id) ||
      input.evidence_edge_id == 0)
    return Status::error(ErrorCode::InvalidInput,
                         "regime outcome needs valid identity, family, evidence and verifier");
  const Replay before = replay(ledger_, world_id_, input.event_key);
  if (!before.pending || before.pending->trial != input.trial_id ||
      before.seen_trials.count(input.trial_id) ||
      before.seen_families.count(input.evidence_family_id))
    return Status::error(ErrorCode::InvalidInput,
                         "no eligible prior prediction or duplicate evidence");
  const bool fires =
      surprising(input.occurred, before.pending->adaptive) &&
      before.surprise_streak == 1 &&
      before.last_surprise == static_cast<int>(input.occurred);
  ModelWorldNode node;
  node.type = ModelWorldNodeType::Outcome;
  node.status = ModelWorldStatus::Verified;
  node.origin = EdgeOrigin::Observed;
  node.statement = "Caller-attested regime outcome: " + input.event_key;
  node.related_nodes = {before.pending->node_id};
  node.evidence_edges = {input.evidence_edge_id};
  node.metadata = {{kType, "outcome"},
                   {kWorld, world_id_},
                   {kEvent, input.event_key},
                   {kTrial, input.trial_id},
                   {kOccurred, input.occurred ? "true" : "false"},
                   {kFamily, input.evidence_family_id},
                   {kVerifier, input.verifier_id}};
  const uint64_t id = ledger_.add(std::move(node),
                                   "externally asserted observed outcome");
  if (id == 0)
    return Status::error(ErrorCode::InvalidInput,
                         "regime outcome rejected by truth boundary");
  *outcome_node_id = id;

  if (fires) {
    // A detected change remains INFERRED / PROPOSED, never Verified.
    ModelWorldNode hypothesis;
    hypothesis.type = ModelWorldNodeType::Hypothesis;
    hypothesis.status = ModelWorldStatus::Proposed;
    hypothesis.origin = EdgeOrigin::Inferred;
    hypothesis.statement = "Provisional binary-event regime shift: " +
                           input.event_key;
    hypothesis.evidence_edges = {before.last_surprise_edge,
                                 input.evidence_edge_id};
    hypothesis.related_nodes = {before.last_surprise_node, id};
    hypothesis.metadata = {{kType, "regime_shift_hypothesis"},
                           {kWorld, world_id_},
                           {kEvent, input.event_key},
                           {kRegime, std::to_string(before.changes + 1)},
                           {"epistemic_warning", "NOT_VERIFIED_CAUSAL_CHANGE"}};
    // Regime bookkeeping uses observed outcomes; this extra hypothesis is
    // audit-only, so a failed marker insert cannot fabricate a learned fact.
    ledger_.add(std::move(hypothesis),
                "two independent unexpected observed outcomes");
  }
  return Status::ok();
}

}  // namespace graphene
