#include "graphene/generic_relation.hpp"
#include "graphene/hypokosh_runtime.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

int main() {
  RelationOntology ontology = RelationOntology::with_core_relations();
  assert(ontology.size() >= 10);
  assert(ontology.canonicalise("calls") == std::optional<std::string>("depends_on"));
  assert(ontology.canonicalise("current depot") == std::optional<std::string>("located_in"));
  assert(!ontology.canonicalise("unmapped-predicate"));
  const auto invalid_types = ontology.validate("authored_by", "person", "work");
  assert(!invalid_types.valid);

  EntityResolver resolver;
  assert(resolver.add({1, "Alex Kim", "person", {"A. Kim"}, {"research", "physics"}}));
  assert(resolver.add({2, "Alex Kim", "person", {}, {"finance", "bank"}}));
  const auto ambiguous = resolver.resolve({"Alex Kim", "person", {}});
  assert(ambiguous.ambiguous);
  assert(!ambiguous.resolved);
  const auto resolved = resolver.resolve({"Alex Kim", "person", {"physics"}});
  assert(resolved.resolved == std::optional<uint64_t>(1));

  const auto tsv = parse_generic_relations(
      "shipment-22\tcontains\tpackage-9\n"
      "package-9\tassigned_vehicle\ttruck-7\n"
      "truck-7\tcurrent_depot\tdepot-a\n", "tsv", "supply-chain");
  assert(tsv.errors.empty());
  assert(tsv.records.size() == 3);
  const auto pipe = parse_generic_relations("service-a|calls|service-b\n", "pipe", "software");
  assert(pipe.records.size() == 1);
  const auto rdf = parse_generic_relations("<sensor-1> <located_in> <plant-4> .\n", "rdf", "iot");
  assert(rdf.records.size() == 1);
  const auto token = parse_generic_relations("SUBJ=invoice-7 REL=supports OBJ=ledger-2\n", "token", "finance");
  assert(token.records.size() == 1);

  const fs::path directory = fs::temp_directory_path() / "graphenedb_generic_reasoning_pipeline";
  fs::remove_all(directory);
  GrapheneDB db;
  DBOptions database_options;
  database_options.dimension = 16;
  database_options.fsync_on_commit = false;
  assert(db.open(directory, database_options));

  GenericIngestOptions ingest_options;
  ingest_options.dimension = 16;
  ingest_options.root_entities = {"shipment-22"};
  ingest_options.symptom_entities = {"depot-a"};
  GenericIngestResult ingest_result;
  GenericRelationIngestor ingestor;
  assert(ingestor.ingest(db, tsv.records, ontology, ingest_options, &ingest_result));
  assert(ingest_result.inserted_nodes.size() == 4);
  assert(ingest_result.inserted_edges.size() == 3);
  assert(ingest_result.warnings.empty());

  const uint32_t shipment = ingest_result.entity_nodes.at("shipment 22");
  const auto depot_node = ingest_result.entity_nodes.at("depot a");
  const auto depot = db.get_node(depot_node);
  assert(depot && depot->symptom);
  const auto shipment_node = db.get_node(shipment);
  assert(shipment_node && shipment_node->root);

  ModelWorld world;
  CompleteHypoKoshRuntime runtime(db, &world);
  RuntimeOptions runtime_options;
  runtime_options.dialectic.mode = QueryMode::Balanced;
  runtime_options.dialectic.semantic_candidates = 4;
  runtime_options.dialectic.max_hops = 5;
  runtime_options.dialectic.max_paths = 16;
  runtime_options.dialectic.minimum_confidence = 0.25;
  const auto reasoning = runtime.reason(
      deterministic_text_vector("depot-a", 16),
      deterministic_text_signature("depot-a"), runtime_options);
  assert(reasoning.receipt.graphene_executed);
  assert(reasoning.receipt.fiber_bundle_built);
  assert(reasoning.primary_node == shipment);
  assert(reasoning.status != GovernedEpistemicStatus::Abstain);
  assert(reasoning.evidence_edges.size() == 3);
  assert(!world.nodes().empty());

  GenericRelationRecord unknown{"depot-a", "mysterious_link", "region-x", "", "", "unknown-source", 0.85, "", ""};
  GenericIngestResult unknown_result;
  assert(ingestor.ingest(db, {unknown}, ontology, ingest_options, &unknown_result));
  assert(!unknown_result.warnings.empty());
  assert(unknown_result.inserted_edges.size() == 1);

  assert(db.close());
  fs::remove_all(directory);
  std::cout << "generic_reasoning_pipeline_contract_passed=true\n";
  return 0;
}
