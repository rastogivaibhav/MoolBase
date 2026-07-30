#include "graphene/hypokosh_runtime.hpp"

#include <filesystem>
#include <iostream>

using namespace graphene;

int main() {
  const auto directory = std::filesystem::temp_directory_path() / "graphenedb_hypokosh_runtime_demo";
  std::filesystem::remove_all(directory);
  GrapheneDB db;
  DBOptions database_options;
  database_options.dimension = 3;
  database_options.fsync_on_commit = false;
  if (!db.open(directory, database_options)) return 1;

  const uint64_t signature = signature_for(3, 8);
  NodeInput root{"release changed pool timeout", {1, 0, 0}, signature, 1, true, false};
  root.metadata["source"] = "release-log";
  NodeInput middle{"connection pool exhausted", {0, 1, 0}, signature, 1, false, false};
  middle.metadata["source"] = "heap-profile";
  NodeInput symptom{"checkout failures", {0, 1, 0}, signature, 1, false, true};
  symptom.metadata["source"] = "monitoring";
  uint32_t a = 0, b = 0, c = 0;
  db.put_node(root, &a);
  db.put_node(middle, &b);
  db.put_node(symptom, &c);
  db.put_edge({a, b, EdgeOrigin::Observed, EdgeRole::Mechanistic, 0.96,
               {{"source_id", "release-log"}}});
  db.put_edge({b, c, EdgeOrigin::Discovered, EdgeRole::Causal, 0.95,
               {{"source_id", "heap-profile"}}});

  RuntimeOptions options;
  options.dialectic.mode = QueryMode::Empirical;
  options.dialectic.minimum_confidence = 0.30;
  ModelWorld world;
  CompleteHypoKoshRuntime runtime(db, &world);
  const auto result = runtime.reason({0, 1, 0}, signature, options);
  std::cout << "status=" << governed_status_name(result.status) << "\n";
  std::cout << "primary_node=" << result.primary_node << "\n";
  std::cout << "bundle_hash=" << result.receipt.final_bundle_hash << "\n";
  std::cout << "stability=" << result.final_stability.total_score << "\n";
  std::cout << "opposition=" << result.final_opposition.opposition_score << "\n";
  db.close();
  std::filesystem::remove_all(directory);
  return 0;
}
