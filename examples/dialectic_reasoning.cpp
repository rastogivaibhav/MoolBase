#include "graphene/dialectic.hpp"

#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << operation << " failed: " << status.message << "\n";
    std::exit(1);
  }
}

uint32_t add(GrapheneDB& db,
             std::string content,
             std::vector<float> vector,
             uint64_t signature,
             bool root = false,
             bool symptom = false) {
  NodeInput input;
  input.content = std::move(content);
  input.vector = std::move(vector);
  input.signature = signature;
  input.incident = 8120;
  input.root = root;
  input.symptom = symptom;
  input.metadata["source"] = "dialectic-demo";
  uint32_t id = 0;
  require(db.put_node(input, &id), "put_node");
  return id;
}

} // namespace

int main() {
  const fs::path directory =
      fs::temp_directory_path() / "graphenedb_dialectic_reasoning_demo";
  fs::remove_all(directory);

  GrapheneDB db;
  DBOptions db_options;
  db_options.dimension = 3;
  db_options.fsync_on_commit = false;
  require(db.open(directory, db_options), "open");

  const uint64_t signature = signature_for(2, 7);
  const uint32_t deployment =
      add(db, "Deployment changed the payment connection lifecycle",
          {1.0f, 0.0f, 0.0f}, signature, true);
  const uint32_t leak =
      add(db, "Connections leaked after retry cancellation",
          {0.1f, 0.9f, 0.0f}, signature);
  const uint32_t outage =
      add(db, "Checkout timed out when the pool saturated",
          {0.0f, 1.0f, 0.0f}, signature, false, true);
  const uint32_t traffic =
      add(db, "A traffic spike is a competing explanation",
          {-1.0f, 0.0f, 0.0f}, signature, true);
  const uint32_t stale =
      add(db, "An earlier report denied connection pressure",
          {0.05f, 0.95f, 0.0f}, signature);

  require(db.put_edge(
              {deployment, leak, EdgeOrigin::Observed, EdgeRole::Mechanistic, 0.97,
               {{"source_id", "deployment-log"}}}),
          "deployment -> leak");
  require(db.put_edge(
              {leak, outage, EdgeOrigin::Discovered, EdgeRole::Causal, 0.95,
               {{"source_id", "heap-profile"}}}),
          "leak -> outage");
  require(db.put_edge(
              {deployment, outage, EdgeOrigin::Inferred, EdgeRole::Compressed, 0.91,
               {{"derived_from", "deployment->leak->outage"}}}),
          "deployment -> outage");
  require(db.put_edge(
              {traffic, outage, EdgeOrigin::Observed, EdgeRole::Causal, 0.72,
               {{"source_id", "traffic-dashboard"}}}),
          "traffic -> outage");
  require(db.put_edge(
              {deployment, stale, EdgeOrigin::Observed, EdgeRole::Contradicts, 0.86,
               {{"source_id", "postmortem"}}}),
          "deployment contradicts stale report");

  DialecticOptions options;
  options.mode = QueryMode::Balanced;
  options.semantic_candidates = 3;
  options.max_opposition_rounds = 1;
  options.reexpansion_threshold = 0.20;

  DialecticEngine engine(db);
  const DialecticResult result =
      engine.reason({0.0f, 1.0f, 0.0f}, signature, options);

  std::cout << "initial_roots=" << result.initial_bundle.roots.size() << "\n";
  std::cout << "initial_paths=";
  size_t paths = 0;
  for (const auto& root : result.initial_bundle.roots) paths += root.paths.size();
  std::cout << paths << "\n";
  std::cout << "primary_node=" << result.synthesis.primary_node << "\n";
  std::cout << "confidence=" << result.synthesis.confidence << "\n";
  std::cout << "epistemic_status=" << result.synthesis.epistemic_status << "\n";
  std::cout << "opposition_score="
            << result.initial_opposition.opposition_score << "\n";
  for (const auto& challenge : result.initial_opposition.challenged_claims) {
    std::cout << "challenge=" << challenge << "\n";
  }
  for (const auto& question : result.initial_opposition.falsification_questions) {
    std::cout << "falsification_question=" << question << "\n";
  }
  std::cout << "reexpansion_rounds=" << result.rounds << "\n";
  std::cout << "durable_writes=" << (result.durable_writes ? "true" : "false")
            << "\n";

  require(db.close(), "close");
  fs::remove_all(directory);
  return 0;
}
