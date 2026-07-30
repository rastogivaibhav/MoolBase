#include "graphene/dialectic.hpp"
#include "graphene/hyperedge.hpp"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

void require(Status status, const char* operation) {
  if (!status) {
    std::cerr << "FAIL " << operation << ": " << status.message << "\n";
    std::abort();
  }
}

NodeInput make_node(const char* content,
                    std::vector<float> vector,
                    bool root = false) {
  NodeInput input;
  input.content = content;
  input.vector = std::move(vector);
  input.signature = signature_for(4, 9);
  input.root = root;
  return input;
}

bool warning_contains(const BundleSet& bundle, const std::string& value) {
  return std::any_of(bundle.warnings.begin(), bundle.warnings.end(),
                     [&](const std::string& warning) {
                       return warning.find(value) != std::string::npos;
                     });
}

} // namespace

int main() {
  const fs::path directory =
      fs::temp_directory_path() / "graphenedb_hyperedge_tests";
  fs::remove_all(directory);

  GrapheneDB db;
  DBOptions options;
  options.dimension = 3;
  options.fsync_on_commit = false;
  require(db.open(directory, options), "open");

  uint32_t source_a = 0;
  uint32_t source_b = 0;
  uint32_t target = 0;
  uint32_t malformed_target = 0;
  uint32_t hypothetical_target = 0;
  require(db.put_node(make_node("source a", {1.0f, 0.0f, 0.0f}, true),
                      &source_a),
          "source a");
  require(db.put_node(make_node("source b", {-1.0f, 0.0f, 0.0f}, true),
                      &source_b),
          "source b");
  require(db.put_node(make_node("joint outcome", {0.0f, 1.0f, 0.0f}),
                      &target),
          "target");
  require(db.put_node(make_node("malformed outcome", {0.0f, 0.0f, 1.0f}),
                      &malformed_target),
          "malformed target");
  require(db.put_node(
              make_node("hypothetical outcome", {0.0f, -1.0f, 0.0f}),
              &hypothetical_target),
          "hypothetical target");

  HyperedgeInput joint;
  joint.external_id = "deployment-and-load";
  joint.sources = {source_b, source_a};
  joint.target = target;
  joint.origin = EdgeOrigin::Observed;
  joint.metadata["source_id"] = "incident-42";
  HyperedgeResult inserted;
  require(put_hyperedge(db, joint, &inserted), "put hyperedge");
  assert(inserted.edge_ids.size() == 2);
  assert(db.incoming_edges(target).size() == 2);
  for (uint32_t edge_id : inserted.edge_ids) {
    const auto edge = db.get_edge(edge_id);
    assert(edge);
    assert(edge->metadata.at("hyperedge_semantics") == "all_sources");
    assert(edge->metadata.at("hyperedge_arity") == "2");
    assert(!edge->metadata.at("hyperedge_group_id").empty());
  }

  const size_t before_invalid = db.edge_count();
  HyperedgeInput invalid = joint;
  invalid.external_id = "invalid";
  invalid.sources = {source_a};
  assert(!put_hyperedge(db, invalid));
  invalid.sources = {source_a, source_a};
  assert(!put_hyperedge(db, invalid));
  invalid.sources = {source_a, 999999};
  assert(!put_hyperedge(db, invalid));
  invalid.sources = {source_a, target};
  invalid.target = target;
  assert(!put_hyperedge(db, invalid));
  invalid = joint;
  invalid.confidence = 2.0;
  assert(!put_hyperedge(db, invalid));
  assert(db.edge_count() == before_invalid);

  DialecticOptions reasoning;
  reasoning.semantic_candidates = 1;
  reasoning.max_hops = 2;
  reasoning.max_paths = 8;
  reasoning.max_opposition_rounds = 0;
  const uint64_t signature = signature_for(4, 9);
  DialecticEngine engine(db);
  BundleSet joint_bundle =
      engine.expand({0.0f, 1.0f, 0.0f}, signature, reasoning);
  assert(joint_bundle.roots.size() == 2);
  for (const auto& root : joint_bundle.roots) {
    assert(root.paths.size() == 1);
    assert(root.paths.front().joint_requirements.size() == 1);
    const JointRequirement& requirement =
        root.paths.front().joint_requirements.front();
    assert(requirement.hyperedge_id == "deployment-and-load");
    assert(requirement.all_sources_present);
    assert(requirement.source_nodes ==
           std::vector<uint32_t>({source_a, source_b}));
    assert(requirement.member_edges.size() == 2);
  }
  HyperedgeResult repeated_insert;
  require(put_hyperedge(db, joint, &repeated_insert),
          "repeat external hyperedge id");
  assert(repeated_insert.edge_ids.size() == 2);
  BundleSet repeated_bundle =
      engine.expand({0.0f, 1.0f, 0.0f}, signature, reasoning);
  assert(repeated_bundle.roots.size() == 2);
  for (const auto& root : repeated_bundle.roots) {
    assert(root.paths.size() == 2);
    for (const auto& path : root.paths) {
      assert(path.joint_requirements.size() == 1);
      assert(path.joint_requirements.front().all_sources_present);
    }
  }

  require(db.put_edge(
              {source_a,
               malformed_target,
               EdgeOrigin::Observed,
               EdgeRole::Causal,
               0.9,
               {{"source_id", "incident-43"},
                {"hyperedge_id", "missing-member"},
                {"hyperedge_semantics", "all_sources"},
                {"hyperedge_sources",
                 std::to_string(source_a) + "," + std::to_string(source_b)},
                {"hyperedge_arity", "2"}}}),
          "put malformed hyperedge member");
  BundleSet malformed =
      engine.expand({0.0f, 0.0f, 1.0f}, signature, reasoning);
  assert(malformed.roots.empty());
  assert(warning_contains(malformed, "INCOMPLETE_HYPEREDGE"));

  HyperedgeInput hypothetical = joint;
  hypothetical.external_id = "speculative-joint";
  hypothetical.target = hypothetical_target;
  hypothetical.origin = EdgeOrigin::Hypothetical;
  require(put_hyperedge(db, hypothetical), "put hypothetical hyperedge");
  BundleSet balanced =
      engine.expand({0.0f, -1.0f, 0.0f}, signature, reasoning);
  assert(balanced.roots.empty());
  DialecticOptions theoretical = reasoning;
  theoretical.mode = QueryMode::Theoretical;
  BundleSet theoretical_bundle =
      engine.expand({0.0f, -1.0f, 0.0f}, signature, theoretical);
  assert(theoretical_bundle.roots.size() == 2);

  require(db.close(), "close");
  fs::remove_all(directory);
  std::cout << "hyperedge_contract_passed=true\n";
  return 0;
}
