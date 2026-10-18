#include "graphene/context_world.hpp"

#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#ifdef NDEBUG
#error "Context-world regression assertions must not be disabled"
#endif

using namespace graphene;
namespace fs = std::filesystem;

bool near(double a, double b, double epsilon = 1e-11) {
  return std::abs(a - b) < epsilon;
}

int main() {
  ModelWorld ledger;
  ContextPredictiveWorld world(ledger, "alpha");
  ContextWorldForecast f;
  uint64_t id = 0;
  const std::vector<double> x{1.0, 1.0, 0.25, 0.5};

  // Safety: no outcome before its prediction, no same-stream double pending.
  assert(!world.observe({"evt", "t0", true, 10, "fam0", "auditor"}, &id));
  assert(!world.forecast("evt", "t1", "yesterday", {1.0, std::nan(""), 0, 0}, &f));
  assert(!world.forecast("evt", "t1", "yesterday", {0.9, 0, 0, 0}, &f));
  assert(world.forecast("evt", "t1", "day0", x, &f));
  assert(near(f.probability, .5));
  assert(!world.forecast("evt", "t2", "day1", x, &f));
  assert(world.metrics("evt").has_pending_forecast);

  auto invalid = WorldVerifiedOutcome{"evt", "t1", true, 0, "fam1", "auditor"};
  assert(!world.observe(invalid, &id));
  invalid.evidence_edge_id = 12;
  invalid.verifier_id = "";
  assert(!world.observe(invalid, &id));
  assert(world.metrics("evt").verified_observations == 0);
  assert(world.observe({"evt", "t1", true, 12, "fam1", "auditor"}, &id));
  assert(ledger.get(id)->status == ModelWorldStatus::Verified);
  assert(ledger.get(id)->origin == EdgeOrigin::Observed);
  assert(world.metrics("evt").verified_observations == 1);
  assert(!world.metrics("evt").has_pending_forecast);

  const auto weights = world.metrics("evt").weights;
  for (size_t j = 0; j < x.size(); ++j) {
    assert(near(weights[j], .06 * x[j]));
  }
  const double z = weights[0]*x[0] + weights[1]*x[1] +
                   weights[2]*x[2] + weights[3]*x[3];
  const double p = 1.0 / (1.0 + std::exp(-z));
  assert(world.forecast("evt", "t2", "day1", x, &f));
  assert(near(f.probability, p));
  const uint64_t frozen_id = f.prediction_node_id;

  // Inject direct malformed outcome; generic ModelWorld permits the claimed
  // metadata, while the context replay must refuse correlated family copy.
  ModelWorldNode forged;
  forged.type = ModelWorldNodeType::Outcome;
  forged.status = ModelWorldStatus::Verified;
  forged.origin = EdgeOrigin::Observed;
  forged.related_nodes = {f.prediction_node_id};
  forged.evidence_edges = {999};
  forged.metadata = {
      {"cw_type","outcome"}, {"cw_world","alpha"}, {"cw_event","evt"},
      {"cw_trial","t2"}, {"cw_occurred","true"},
      {"cw_family","fam1"}, {"cw_verifier","fake"}};
  assert(ledger.add(forged, "correlated fake") > 0);
  assert(world.metrics("evt").verified_observations == 1);
  assert(world.metrics("evt").has_pending_forecast);
  assert(!world.observe({"evt","t2",false,99,"fam1","auditor"}, &id));

  assert(world.observe({"evt","t2",false,99,"fam2","auditor"}, &id));
  assert(world.metrics("evt").verified_observations == 2);
  assert(ledger.get(frozen_id)->status == ModelWorldStatus::Proposed);

  // No duplicate trial, no feature-dimension drift.
  assert(!world.forecast("evt","t1","day2",x,&f));
  assert(!world.forecast("evt","t3","day2",{1.,0.},&f));

  // Dynamic scores and all weights survive ledger save/load.
  const auto before = world.metrics("evt");
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path file = fs::temp_directory_path() /
      ("moolbase-context-world-" + std::to_string(stamp) + ".mwv");
  assert(ledger.save(file));
  ModelWorld restored;
  assert(restored.load(file));
  ContextPredictiveWorld recovered(restored, "alpha");
  const auto after = recovered.metrics("evt");
  assert(after.verified_observations == before.verified_observations);
  assert(near(after.mean_brier, before.mean_brier));
  assert(after.weights.size() == before.weights.size());
  for (size_t i = 0; i < before.weights.size(); ++i)
    assert(near(after.weights[i], before.weights[i]));
  std::error_code ec;
  fs::remove(file, ec);
  assert(!ec);

  // Direct injection of miscalibrated forecast must not become a pending
  // hypothesis and must not train the later observed outcome.
  ModelWorldNode fake;
  fake.type = ModelWorldNodeType::Hypothesis;
  fake.status = ModelWorldStatus::Proposed;
  fake.origin = EdgeOrigin::Hypothetical;
  fake.metadata = {
      {"cw_type","forecast"}, {"cw_world","alpha"}, {"cw_event","other"},
      {"cw_trial","bad"}, {"cw_source","day-1"},
      {"cw_features","1,1,1"}, {"cw_probability","0.999999"}};
  assert(restored.add(fake, "forged prediction") > 0);
  assert(!recovered.metrics("other").has_pending_forecast);
  assert(recovered.forecast("other","good","day-1",{1.,0.1,0.2},&f));
  assert(near(f.probability,0.5));

  // World and event are independent in the same ledger.
  ContextPredictiveWorld other(restored, "beta");
  assert(other.forecast("evt","b1","day0",x,&f));
  assert(near(f.probability,0.5));
  assert(other.metrics("evt").verified_observations == 0);

  std::cout << "p4_context_learning_replay_contract=PASS\n";
}
