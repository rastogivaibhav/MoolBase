#include "graphene/predictive_world.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#ifdef NDEBUG
#error "Research predictions must run with native assertions enabled"
#endif

using namespace graphene;
namespace fs = std::filesystem;

static bool near(double a, double b) {
  return std::abs(a - b) < 1e-12;
}

int main() {
  ModelWorld ledger;
  SinglePredictiveWorld predictor(ledger, "supply");
  WorldForecast forecast;
  uint64_t outcome_id = 0;

  // No training from an outcome supplied before a prediction.
  WorldVerifiedOutcome premature{"delay", "trial-0", false, 10, "family-0", "auditor-1"};
  assert(!predictor.observe(premature, &outcome_id));
  assert(ledger.nodes().empty());

  // Six pre-outcome predictions, each followed by independent attested feedback.
  // This controlled stationary toy environment is NOT a scientific holdout.
  const double expected[] = {0.5, 1.0 / 3.0, 0.25, 0.2, 1.0 / 6.0, 1.0 / 7.0};
  for (int i = 0; i < 6; ++i) {
    const std::string trial = "trial-" + std::to_string(i + 1);
    assert(predictor.forecast("delay", trial, &forecast));
    assert(near(forecast.probability, expected[i]));
    assert(forecast.verified_observations == static_cast<size_t>(i));
    const uint64_t prediction_id = forecast.prediction_node_id;
    const auto predicted = ledger.get(prediction_id);
    assert(predicted && predicted->status == ModelWorldStatus::Proposed);
    assert(predicted->origin == EdgeOrigin::Hypothetical);

    const WorldVerifiedOutcome outcome{
        "delay", trial, false, static_cast<uint32_t>(100 + i),
        "independent-" + std::to_string(i + 1), "auditor-1"};
    assert(predictor.observe(outcome, &outcome_id));
    assert(ledger.get(outcome_id)->status == ModelWorldStatus::Verified);
    assert(ledger.get(outcome_id)->origin == EdgeOrigin::Observed);
    // Later outcome updates cannot overwrite the original forecast.
    const auto frozen = ledger.get(prediction_id);
    assert(frozen && frozen->status == ModelWorldStatus::Proposed);
  }

  const auto six = predictor.metrics("delay");
  assert(six.scored_predictions == 6);
  assert(six.positive_outcomes == 0);
  assert(near(six.next_probability, 1.0 / 8.0));
  assert(six.mean_brier < six.constant_half_brier);

  // Duplicate trial and evidence family must NOT amplify learning.
  const size_t node_count = ledger.nodes().size();
  assert(!predictor.forecast("delay", "trial-1", &forecast));
  assert(predictor.forecast("delay", "trial-duplicate-source", &forecast));
  const WorldVerifiedOutcome duplicate{
      "delay", "trial-duplicate-source", false, 500,
      "independent-1", "auditor-1"};
  assert(!predictor.observe(duplicate, &outcome_id));
  assert(predictor.metrics("delay").scored_predictions == 6);
  assert(ledger.nodes().size() == node_count + 1);

  // A real contradictory outcome must update the posterior, not get suppressed.
  assert(predictor.forecast("delay", "trial-7", &forecast));
  assert(near(forecast.probability, 1.0 / 8.0));
  const WorldVerifiedOutcome surprise{"delay", "trial-7", true, 700,
                                      "independent-7", "auditor-2"};
  assert(predictor.observe(surprise, &outcome_id));
  assert(near(predictor.metrics("delay").next_probability, 2.0 / 9.0));
  assert(predictor.metrics("delay").scored_predictions == 7);
  assert(predictor.metrics("delay").mean_brier <
         predictor.metrics("delay").constant_half_brier);

  // No fake evidence ID, verifier or family is admissible.
  assert(predictor.forecast("delay", "trial-8", &forecast));
  WorldVerifiedOutcome invalid{"delay", "trial-8", true, 0, "fam-8", "auditor-1"};
  assert(!predictor.observe(invalid, &outcome_id));
  invalid.evidence_edge_id = 800;
  invalid.verifier_id.clear();
  assert(!predictor.observe(invalid, &outcome_id));
  assert(predictor.metrics("delay").scored_predictions == 7);
  assert(ledger.audit().empty());

  // Existing ModelWorld persistence is the world memory; restore replays
  // prior observed outcomes, not just a mutable cached posterior value.
  const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto file = fs::temp_directory_path() /
      ("moolbase-predictive-world-" + std::to_string(tick) + ".mwv");
  assert(ledger.save(file));
  ModelWorld loaded;
  assert(loaded.load(file));
  SinglePredictiveWorld restored(loaded, "supply");
  const auto after = restored.metrics("delay");
  assert(after.scored_predictions == 7);
  assert(near(after.next_probability, 2.0 / 9.0));
  assert(near(after.mean_brier, predictor.metrics("delay").mean_brier));
  assert(restored.forecast("delay", "trial-9", &forecast));
  assert(near(forecast.probability, 2.0 / 9.0));
  std::error_code ec;
  fs::remove(file, ec);
  assert(!ec);

  // Independent worlds in one ledger do not share observations.
  SinglePredictiveWorld independent(loaded, "other");
  assert(independent.forecast("delay", "other-trial-1", &forecast));
  assert(near(forecast.probability, 0.5));

  std::cout << "single_world_prequential_contract=PASS\n";
  std::cout << "verified_samples=" << after.scored_predictions
            << " next_probability=" << after.next_probability
            << " mean_brier=" << after.mean_brier
            << " fixed_half_brier=" << after.constant_half_brier << "\n";
}
