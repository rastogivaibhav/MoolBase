#include "graphene/db.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

using namespace graphene;
namespace fs = std::filesystem;

static void require(Status st, const char* what) {
  if (!st) {
    std::cerr << what << ": " << st.message << "\n";
    std::abort();
  }
}

int main() {
  fs::path dir = fs::temp_directory_path() / "graphenedb_extraction_ingest_test";
  fs::remove_all(dir);

  DBOptions opt;
  opt.dimension = 3;
  opt.require_lattice = true;
  opt.tuning.enable_lattice_retrieval = true;

  GrapheneDB db;
  require(db.open(dir, opt), "open");

  ExtractionInput input;
  input.schema_version = kExtractionSchemaVersion;
  input.source_id = "research-pack-001";
  input.source_uri = "file:///research-pack-001.md";
  input.extraction_run_id = "extract-run-001";
  input.incident = 77;
  input.signature = signature_for(2, 5);

  ExtractionNode root;
  root.external_id = "claim/root";
  root.content = "graphene lattice coordinates are part of the core model";
  root.vector = {0.1f, 0.2f, 0.3f};
  root.role = ExtractionRole::Root;
  input.nodes.push_back(root);

  ExtractionNode symptom;
  symptom.external_id = "claim/symptom";
  symptom.content = "retrieval should propagate through validated neighbor bonds";
  symptom.vector = {0.11f, 0.19f, 0.31f};
  symptom.role = ExtractionRole::Symptom;
  input.nodes.push_back(symptom);

  ExtractionRelation rel;
  rel.from_external_id = root.external_id;
  rel.to_external_id = symptom.external_id;
  rel.role = EdgeRole::Supports;
  rel.bond_type = BondType::Sigma;
  rel.confidence = 0.92;
  rel.evidence_id = "quote-17";
  rel.evidence_uri = "file:///research-pack-001.md#quote-17";
  rel.evidence_text = "retrieval should propagate through validated neighbor bonds";
  rel.metadata["extractor"] = "unit-test-extractor";
  rel.bond_strength = 0.92;
  input.relations.push_back(rel);
  input.relations.push_back(rel);

  ExtractionResult first;
  require(db.put_extraction(input, &first), "first extraction");
  assert(first.inserted_node_ids.size() == 2);
  assert(first.existing_node_ids.empty());
  assert(first.inserted_edge_ids.size() == 1);
  assert(db.node_count() == 2);
  assert(db.edge_count() == 1);
  assert(first.external_to_node_id.at("claim/root") == 0);
  assert(first.external_to_node_id.at("claim/symptom") == 1);

  auto root_node = db.get_node(0);
  assert(root_node);
  assert(root_node->lattice);
  assert(root_node->metadata.at("graphene_source_id") == input.source_id);
  assert(root_node->metadata.at("graphene_external_id") == "claim/root");
  assert(root_node->metadata.at("graphene_extraction_schema") == "1");
  assert(root_node->metadata.at("graphene_source_uri") == input.source_uri);
  assert(root_node->metadata.at("graphene_extraction_run_id") == input.extraction_run_id);

  auto edge = db.get_edge(first.inserted_edge_ids.at(0));
  assert(edge);
  assert(edge->confidence == rel.confidence);
  assert(edge->metadata.at("graphene_extraction_schema") == "1");
  assert(edge->metadata.at("graphene_source_uri") == input.source_uri);
  assert(edge->metadata.at("graphene_extraction_run_id") == input.extraction_run_id);
  assert(edge->metadata.at("graphene_evidence_id") == rel.evidence_id);
  assert(edge->metadata.at("graphene_evidence_uri") == rel.evidence_uri);
  assert(edge->metadata.at("graphene_evidence_text") == rel.evidence_text);
  assert(edge->metadata.at("extractor") == "unit-test-extractor");

  ExtractionResult second;
  require(db.put_extraction(input, &second), "idempotent extraction");
  assert(second.inserted_node_ids.empty());
  assert(second.inserted_edge_ids.empty());
  assert(second.existing_node_ids.size() == 2);
  assert(db.node_count() == 2);
  assert(db.edge_count() == 1);

  ExtractionInput changed_node = input;
  changed_node.nodes[0].content = "conflicting replay content";
  auto changed_node_status = db.put_extraction(changed_node);
  assert(!changed_node_status);
  assert(changed_node_status.code == ErrorCode::InvalidInput);
  assert(changed_node_status.message.find("idempotency conflict") !=
         std::string::npos);
  assert(db.node_count() == 2);
  assert(db.edge_count() == 1);

  ExtractionInput changed_relation = input;
  changed_relation.relations.resize(1);
  changed_relation.relations[0].confidence = 0.5;
  auto changed_relation_status = db.put_extraction(changed_relation);
  assert(!changed_relation_status);
  assert(changed_relation_status.code == ErrorCode::InvalidInput);
  assert(changed_relation_status.message.find("idempotency conflict") !=
         std::string::npos);
  assert(db.node_count() == 2);
  assert(db.edge_count() == 1);

  ExtractionInput invalid_atomic = input;
  invalid_atomic.source_id = "invalid-atomic-source";
  invalid_atomic.nodes.resize(1);
  invalid_atomic.relations.resize(1);
  invalid_atomic.relations[0].to_external_id = "missing-endpoint";
  auto invalid_atomic_status = db.put_extraction(invalid_atomic);
  assert(!invalid_atomic_status);
  assert(invalid_atomic_status.code == ErrorCode::EdgeInvalid);
  assert(db.node_count() == 2);
  assert(db.edge_count() == 1);

  std::string report;
  require(db.validate(&report), "validate");
  require(db.close(), "close");

  GrapheneDB reopened;
  require(reopened.open(dir, opt), "reopen");
  ExtractionResult third;
  require(reopened.put_extraction(input, &third), "idempotent extraction after reopen");
  assert(third.inserted_node_ids.empty());
  assert(third.inserted_edge_ids.empty());
  assert(reopened.node_count() == 2);
  assert(reopened.edge_count() == 1);
  auto reopened_edge = reopened.get_edge(first.inserted_edge_ids.at(0));
  assert(reopened_edge);
  assert(reopened_edge->metadata.at("graphene_evidence_id") == rel.evidence_id);
  require(reopened.validate(&report), "validate reopened");
  require(reopened.close(), "close reopened");

  fs::path bad_dir = fs::temp_directory_path() / "graphenedb_extraction_ingest_bad_version_test";
  fs::remove_all(bad_dir);
  GrapheneDB bad;
  require(bad.open(bad_dir, opt), "open bad version db");
  input.schema_version = kExtractionSchemaVersion + 1;
  auto bad_status = bad.put_extraction(input);
  assert(!bad_status);
  assert(bad_status.code == ErrorCode::UnsupportedMode);
  require(bad.close(), "close bad version db");

  std::cout << "graphenedb_extraction_ingest_tests_passed=true\n";
  return 0;
}
