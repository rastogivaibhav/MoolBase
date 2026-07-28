#pragma once

#include "graphene/db.hpp"
#include "graphene/entity_resolution.hpp"
#include "graphene/relation_ontology.hpp"

#include <map>
#include <string>
#include <vector>

namespace graphene {

struct GenericRelationRecord {
  std::string subject;
  std::string predicate;
  std::string object;
  std::string subject_type;
  std::string object_type;
  std::string source_id;
  double confidence{0.85};
  std::string valid_from;
  std::string valid_until;
};

struct GenericRelationParseResult {
  std::vector<GenericRelationRecord> records;
  std::vector<std::string> errors;
};

struct GenericIngestOptions {
  uint32_t dimension{16};
  std::vector<std::string> root_entities;
  std::vector<std::string> symptom_entities;
  bool reject_unknown_relations{false};
};

struct GenericIngestResult {
  std::map<std::string, uint32_t> entity_nodes;
  std::vector<uint32_t> inserted_nodes;
  std::vector<uint32_t> inserted_edges;
  std::vector<std::string> warnings;
};

GenericRelationParseResult parse_generic_relations(const std::string& text,
                                                   const std::string& format,
                                                   const std::string& source_id = "generic-input");

class GenericRelationIngestor {
 public:
  Status ingest(GrapheneDB& db,
                const std::vector<GenericRelationRecord>& records,
                const RelationOntology& ontology,
                const GenericIngestOptions& options,
                GenericIngestResult* out) const;
};

std::vector<float> deterministic_text_vector(const std::string& text,
                                             uint32_t dimension);
uint64_t deterministic_text_signature(const std::string& text);

}  // namespace graphene
