#pragma once

#include "graphene/db.hpp"
#include "graphene/entity_resolution.hpp"
#include "graphene/relation_ontology.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace graphene {

struct GenericRelationRecord {
  std::string subject;
  std::string subject_type;
  std::string predicate;
  std::string object;
  std::string object_type;
  std::string source_id;
  std::string evidence_text;
  double confidence{0.9};
  std::string valid_from;
  std::string valid_until;
  bool subject_root{false};
  bool object_root{false};
  bool subject_symptom{false};
  bool object_symptom{false};
  std::map<std::string, std::string> metadata;
};

struct CanonicalIngestionOptions {
  std::string extraction_run_id{"canonical-ingestion-v1"};
  bool reject_unknown_relations{false};
  bool reject_ambiguous_entities{true};
  bool place_missing_lattice{true};
  uint32_t incident{0};
  int32_t layer{0};
};

struct CanonicalIngestionResult {
  ExtractionResult extraction;
  std::vector<std::string> warnings;
  std::map<std::string, std::string> relation_mapping;
  std::map<std::string, std::string> entity_mapping;
};

class CanonicalIngestor {
 public:
  CanonicalIngestor(GrapheneDB& db,
                    const RelationOntology& ontology,
                    const EntityResolver* resolver = nullptr);

  Status ingest(const std::vector<GenericRelationRecord>& records,
                const CanonicalIngestionOptions& options = {},
                CanonicalIngestionResult* out = nullptr);

  static std::vector<float> deterministic_embedding(const std::string& text,
                                                    uint32_t dimension);

 private:
  GrapheneDB& db_;
  const RelationOntology& ontology_;
  const EntityResolver* resolver_;
};

}  // namespace graphene
