#include "graphene/generic_relation.hpp"
#include "graphene/relation_ontology.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

#ifdef NDEBUG
#error "Direction-safety regressions require native C++ assertions"
#endif

using namespace graphene;
namespace fs = std::filesystem;

int main() {
  const auto ontology = RelationOntology::with_core_relations();
  assert(ontology.canonicalise("calls") == std::optional<std::string>("calls"));
  assert(ontology.canonicalise("uses") == std::optional<std::string>("uses"));
  assert(ontology.canonicalise("requires") == std::optional<std::string>("requires"));
  assert(ontology.canonicalise("depends_on") ==
         std::optional<std::string>("depends_on"));

  // Never expose an inverse alias through a directionless API.
  assert(!ontology.canonicalise("caused_by"));
  const auto inverted = ontology.resolve("caused_by");
  assert(inverted && inverted->canonical_id == "caused" &&
         inverted->swap_endpoints);
  const auto forward = ontology.resolve("causes");
  assert(forward && forward->canonical_id == "caused" &&
         !forward->swap_endpoints);
  const auto supported = ontology.resolve("supported_by");
  assert(supported && supported->canonical_id == "supports" &&
         supported->swap_endpoints);
  const auto motherhood = ontology.resolve("mother_of");
  assert(motherhood && motherhood->swap_endpoints);

  RelationOntology copy = ontology;
  RelationDefinition wrong;
  wrong.id = "conflict";
  wrong.aliases = {"dup_alias"};
  wrong.inverse_aliases = {"dup_alias"};
  const size_t original = copy.size();
  assert(!copy.register_relation(wrong));
  assert(copy.size() == original);
  assert(!copy.resolve("dup_alias"));

  const fs::path dir = fs::temp_directory_path() / "moolbase-polymath-direction";
  fs::remove_all(dir);
  GrapheneDB db;
  DBOptions options;
  options.dimension = 16;
  options.fsync_on_commit = false;
  assert(db.open(dir, options));

  GenericIngestOptions policy;
  policy.dimension = 16;
  policy.reject_unknown_relations = true;
  GenericRelationIngestor ingestor;

  // Spoken order "latency caused_by failure" becomes stored
  // edge failure -> latency, with the source wording preserved.
  GenericRelationRecord reversed{"latency", "caused_by", "cache_failure",
                                 "event", "event", "fixture-example",
                                 0.7, "", ""};
  GenericIngestResult inserted;
  assert(ingestor.ingest(db, {reversed}, ontology, policy, &inserted));
  assert(inserted.inserted_edges.size() == 1);
  const auto edge = db.get_edge(inserted.inserted_edges.front());
  assert(edge);
  assert(edge->from == inserted.entity_nodes.at("cache failure"));
  assert(edge->to == inserted.entity_nodes.at("latency"));
  assert(edge->role == EdgeRole::Causal);
  assert(edge->metadata.at("relation_id") == "caused");
  assert(edge->metadata.at("relation_original_predicate") == "caused_by");
  assert(edge->metadata.at("relation_direction_swapped") == "true");

  // Normal forward alias must not reverse endpoints.
  GenericRelationRecord forward_relation{"cache_failure", "causes",
                                         "timeout", "event", "event",
                                         "fixture-example", 0.8, "", ""};
  GenericIngestResult inserted_forward;
  assert(ingestor.ingest(db, {forward_relation}, ontology, policy,
                         &inserted_forward));
  const auto edge_forward = db.get_edge(inserted_forward.inserted_edges.front());
  assert(edge_forward);
  assert(edge_forward->from ==
         inserted_forward.entity_nodes.at("cache failure"));
  assert(edge_forward->to == inserted_forward.entity_nodes.at("timeout"));
  assert(edge_forward->metadata.at("relation_direction_swapped") == "false");

  // Canonical types are validated after inverse direction changes:
  // "person authored_by work" must fail; "person supported_by work"
  // is structurally permitted as a generic evidence link.
  GenericRelationRecord mistyped{"person-a", "authored_by", "work-a",
                                "person", "work", "fixture", .7, "", ""};
  GenericIngestResult rejected;
  assert(!ingestor.ingest(db, {mistyped}, ontology, policy, &rejected));

  assert(db.close());
  fs::remove_all(dir);
  std::cout << "polymath_relation_direction_contract=PASS\n";
}
