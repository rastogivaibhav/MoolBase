#include "graphene/regime_world.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef NDEBUG
#error "Regime-world research tests require native assertions"
#endif

using namespace graphene;
namespace fs = std::filesystem;

bool near(double a, double b, double tol = 1e-10) {
  return std::abs(a - b) < tol;
}

struct OutcomeCase {
  std::string name;
  std::vector<bool> outcomes;
  size_t expected_changes;
  double min_brier_improvement;
};

void run_case(const OutcomeCase& testcase) {
  ModelWorld ledger;
  RegimePredictiveWorld predictor(ledger, "worldA");
  RegimeWorldForecast forecast;
  uint64_t observed_node = 0;
  const size_t n = testcase.outcomes.size();
  for (size_t i = 0; i < n; ++i) {
    const std::string trial = "trial-" + std::to_string(i);
    assert(predictor.forecast("incident", trial, &forecast));
    const auto stored_prediction = ledger.get(forecast.prediction_node_id);
    assert(stored_prediction &&
           stored_prediction->origin == EdgeOrigin::Hypothetical &&
           stored_prediction->status == ModelWorldStatus::Proposed);
    assert(!predictor.forecast("incident", "second-pending", &forecast));

    const WorldVerifiedOutcome observation{
        "incident", trial, testcase.outcomes[i],
        static_cast<uint32_t>(1000 + i),
        "unique-family-" + std::to_string(i),
        "independent-auditor"};
    assert(predictor.observe(observation, &observed_node));
    assert(ledger.get(observed_node)->origin == EdgeOrigin::Observed);
    assert(ledger.get(observed_node)->status == ModelWorldStatus::Verified);
    assert(!predictor.report("incident").has_pending_forecast);
    assert(ledger.get(stored_prediction->id)->status == ModelWorldStatus::Proposed);
    if (testcase.expected_changes == 1 && i == 9) {
      assert(predictor.report("incident").regime_changes == 1);
    }
  }

  const auto result = predictor.report("incident");
  assert(result.verified_observations == n);
  assert(result.regime_changes == testcase.expected_changes);
  assert(result.lifetime_brier + 1e-12 >= result.adaptive_brier +
         testcase.min_brier_improvement);
  if (testcase.expected_changes == 0)
    assert(near(result.adaptive_brier, result.lifetime_brier));

  size_t marker_count = 0;
  for (const auto& node : ledger.nodes()) {
    if (node.type != ModelWorldNodeType::Hypothesis ||
        node.metadata.find("pw_v2_type") == node.metadata.end() ||
        node.metadata.at("pw_v2_type") != "regime_shift_hypothesis") continue;
    ++marker_count;
    assert(node.status == ModelWorldStatus::Proposed);
    assert(node.origin == EdgeOrigin::Inferred);
    assert(node.evidence_edges.size() == 2);
    assert(node.related_nodes.size() == 2);
    assert(!ledger.update_status(node.id, ModelWorldStatus::Verified,
                                 "model-only agreement must not verify"));
  }
  assert(marker_count == testcase.expected_changes);
  assert(ledger.audit().empty());

  // Cross-world state must not be borrowed, despite using the same ledger.
  RegimePredictiveWorld other(ledger, "worldB");
  assert(other.forecast("incident", "other-1", &forecast));
  assert(near(forecast.adaptive_probability, 0.5));
  assert(other.report("incident").verified_observations == 0);

  // Snapshot restart must preserve only observed updates and the exact score.
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path saved = fs::temp_directory_path() /
      ("moolbase-p2-" + testcase.name + "-" + std::to_string(now) + ".mwv");
  assert(ledger.save(saved));
  ModelWorld restored;
  assert(restored.load(saved));
  RegimePredictiveWorld after_restart(restored, "worldA");
  const auto replay = after_restart.report("incident");
  assert(replay.verified_observations == result.verified_observations);
  assert(replay.regime_changes == result.regime_changes);
  assert(near(replay.adaptive_brier, result.adaptive_brier));
  assert(near(replay.lifetime_brier, result.lifetime_brier));
  assert(near(replay.next_adaptive_probability,
              result.next_adaptive_probability));
  assert(near(replay.next_lifetime_probability,
              result.next_lifetime_probability));
  std::error_code ec;
  fs::remove(saved, ec);
  assert(!ec);
  std::cout << testcase.name
            << " observations=" << result.verified_observations
            << " changes=" << result.regime_changes
            << " brier_p2=" << result.adaptive_brier
            << " brier_p1=" << result.lifetime_brier << "\n";
}

void attack_cases() {
  ModelWorld ledger;
  RegimePredictiveWorld world(ledger, "guard");
  RegimeWorldForecast f;
  uint64_t outcome_id = 0;
  WorldVerifiedOutcome first{"signal", "t1", false, 101, "source-1", "v1"};
  assert(!world.observe(first, &outcome_id)); // no pre-outcome forecast
  assert(world.forecast("signal", "t1", &f));
  assert(world.report("signal").has_pending_forecast);
  assert(!world.forecast("signal", "t2", &f));

  WorldVerifiedOutcome missing = first;
  missing.evidence_edge_id = 0;
  assert(!world.observe(missing, &outcome_id));
  missing = first;
  missing.verifier_id = "";
  assert(!world.observe(missing, &outcome_id));
  missing = first;
  missing.evidence_family_id = "";
  assert(!world.observe(missing, &outcome_id));
  assert(world.report("signal").verified_observations == 0);

  assert(world.observe(first, &outcome_id));
  assert(!world.observe(first, &outcome_id));
  assert(world.forecast("signal", "t2", &f));
  assert(!world.forecast("signal", "t2", &f));

  // Correlated direct-write outcome with the same provenance family is
  // structurally admissible to the generic ledger but must not train P2.
  ModelWorldNode copied;
  copied.type = ModelWorldNodeType::Outcome;
  copied.status = ModelWorldStatus::Verified;
  copied.origin = EdgeOrigin::Observed;
  copied.evidence_edges = {202};
  copied.related_nodes = {f.prediction_node_id};
  copied.metadata = {
      {"pw_v2_type", "outcome"}, {"pw_v2_world", "guard"},
      {"pw_v2_event", "signal"}, {"pw_v2_trial", "t2"},
      {"pw_v2_occurred", "true"}, {"pw_v2_family", "source-1"},
      {"pw_v2_verifier", "v-forged"}};
  assert(ledger.add(copied, "adversarial correlated record") != 0);
  assert(world.report("signal").verified_observations == 1);
  assert(world.report("signal").has_pending_forecast);
  WorldVerifiedOutcome duplicate = first;
  duplicate.trial_id = "t2";
  assert(!world.observe(duplicate, &outcome_id));
  WorldVerifiedOutcome second{"signal", "t2", true, 203, "source-2", "v2"};
  assert(world.observe(second, &outcome_id));
  assert(world.report("signal").verified_observations == 2);

  // Insertion-order defends against outcomes linked to a future prediction.
  ModelWorldNode retro = copied;
  retro.related_nodes = {static_cast<uint64_t>(ledger.nodes().size() + 2)};
  retro.metadata["pw_v2_trial"] = "retro";
  retro.metadata["pw_v2_family"] = "source-retro";
  const auto retro_id = ledger.add(retro, "backdated observation");
  assert(retro_id != 0);
  assert(world.forecast("signal", "retro", &f));
  assert(f.prediction_node_id == retro_id + 1);
  assert(world.report("signal").verified_observations == 2);

  // Deliberately inconsistent forged probability must be ignored by replay.
  ModelWorldNode invalid;
  invalid.type = ModelWorldNodeType::Hypothesis;
  invalid.status = ModelWorldStatus::Proposed;
  invalid.origin = EdgeOrigin::Hypothetical;
  invalid.metadata = {
      {"pw_v2_type", "forecast"}, {"pw_v2_world", "guard"},
      {"pw_v2_event", "second-event"}, {"pw_v2_trial", "fake"},
      {"pw_v2_adaptive_probability", "0.999"},
      {"pw_v2_lifetime_probability", "0.5"},
      {"pw_v2_regime", "0"}};
  assert(ledger.add(invalid, "retroactive model calibration") != 0);
  assert(world.report("second-event").verified_observations == 0);
  assert(!world.report("second-event").has_pending_forecast);
  assert(world.forecast("second-event", "real", &f));
  assert(near(f.adaptive_probability, 0.5));
  std::cout << "regime_world_adversarial_replay=PASS\n";
}

int main() {
  run_case({"S0_stable", std::vector<bool>(20, false), 0, 0.0});

  std::vector<bool> shift_up(8, false);
  shift_up.insert(shift_up.end(), 12, true);
  run_case({"S1_up", shift_up, 1, 0.10});

  std::vector<bool> shift_down(8, true);
  shift_down.insert(shift_down.end(), 12, false);
  run_case({"S2_down", shift_down, 1, 0.10});

  std::vector<bool> outlier(8, false);
  outlier.push_back(true);
  outlier.insert(outlier.end(), 8, false);
  run_case({"S3_outlier", outlier, 0, 0.0});

  std::vector<bool> alternating;
  for (int i = 0; i < 10; ++i) {
    alternating.push_back(false);
    alternating.push_back(true);
  }
  run_case({"S4_alternating", alternating, 0, 0.0});

  attack_cases();
  std::cout << "regime_world_contract=PASS\n";
}
