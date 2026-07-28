#include "graphene/canonical_ingestion.hpp"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace graphene {
namespace {

uint64_t fnv1a(const std::string& text) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char ch : text) {
    hash ^= ch;
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string entity_key(const std::string& type, const std::string& name) {
  return RelationOntology::normalize(type) + ":" +
         RelationOntology::normalize(name);
}

struct ResolvedEntity {
  std::string external_id;
  std::string content;
  std::string type;
};

Status resolve_entity(const EntityResolver* resolver,
                      const std::string& text,
                      const std::string& type,
                      const std::string& source_id,
                      bool reject_ambiguous,
                      ResolvedEntity* out,
                      std::vector<std::string>* warnings) {
  if (!out || text.empty()) {
    return Status::error(ErrorCode::InvalidInput,
                         "relation subject/object cannot be empty");
  }
  if (!resolver) {
    out->external_id = entity_key(type, text);
    out->content = text;
    out->type = RelationOntology::normalize(type);
    return Status::ok();
  }

  EntityMention mention;
  mention.text = text;
  mention.type = type;
  mention.source_id = source_id;
  const EntityResolutionResult resolved = resolver->resolve(mention);
  if (resolved.ambiguous) {
    if (reject_ambiguous) {
      return Status::error(ErrorCode::InvalidInput,
                           "ambiguous entity mention: " + text);
    }
    warnings->push_back("AMBIGUOUS_ENTITY_PRESERVED: " + text);
  }
  if (resolved.resolved_entity_id) {
    const auto entity = resolver->get(*resolved.resolved_entity_id);
    if (entity) {
      out->external_id = "entity:" + entity->id;
      out->content = entity->canonical_name;
      out->type = entity->type;
      return Status::ok();
    }
  }
  if (resolved.unresolved) {
    warnings->push_back("UNRESOLVED_ENTITY_PRESERVED: " + text);
  }
  out->external_id = entity_key(type, text);
  out->content = text;
  out->type = RelationOntology::normalize(type);
  return Status::ok();
}

ExtractionRole merge_role(ExtractionRole existing,
                          bool root,
                          bool symptom) {
  if (root) return ExtractionRole::Root;
  if (existing == ExtractionRole::Root) return existing;
  if (symptom) return ExtractionRole::Symptom;
  return existing;
}

}  // namespace

CanonicalIngestor::CanonicalIngestor(GrapheneDB& db,
                                     const RelationOntology& ontology,
                                     const EntityResolver* resolver)
    : db_(db), ontology_(ontology), resolver_(resolver) {}

std::vector<float> CanonicalIngestor::deterministic_embedding(
    const std::string& text,
    uint32_t dimension) {
  std::vector<float> output(dimension, 0.0f);
  if (dimension == 0) return output;
  uint64_t state = fnv1a(text);
  for (uint32_t i = 0; i < dimension; ++i) {
    state ^= state >> 12;
    state ^= state << 25;
    state ^= state >> 27;
    const uint64_t mixed = state * 2685821657736338717ULL;
    const double unit = static_cast<double>(mixed & 0xffffULL) / 65535.0;
    output[i] = static_cast<float>(2.0 * unit - 1.0);
  }
  double norm = 0.0;
  for (float value : output) norm += static_cast<double>(value) * value;
  norm = std::sqrt(norm);
  if (norm > 0.0) {
    for (float& value : output) value = static_cast<float>(value / norm);
  }
  return output;
}

Status CanonicalIngestor::ingest(
    const std::vector<GenericRelationRecord>& records,
    const CanonicalIngestionOptions& options,
    CanonicalIngestionResult* out) {
  if (records.empty()) {
    return Status::error(ErrorCode::InvalidInput,
                         "canonical ingestion requires at least one relation");
  }
  if (db_.dimension() == 0) {
    return Status::error(ErrorCode::InvalidOption,
                         "database dimension must be configured");
  }

  ExtractionInput extraction;
  extraction.source_id = "canonical-ingestion";
  extraction.extraction_run_id = options.extraction_run_id;
  extraction.incident = options.incident;
  extraction.layer = options.layer;
  extraction.place_missing_lattice = options.place_missing_lattice;
  extraction.idempotent = true;

  std::map<std::string, ExtractionNode> nodes;
  std::vector<std::string> warnings;
  std::map<std::string, std::string> relation_mapping;
  std::map<std::string, std::string> entity_mapping;

  for (size_t index = 0; index < records.size(); ++index) {
    const auto& record = records[index];
    if (record.source_id.empty()) {
      return Status::error(ErrorCode::InvalidInput,
                           "observed generic relation requires source_id");
    }
    if (!std::isfinite(record.confidence) || record.confidence < 0.0 ||
        record.confidence > 1.0) {
      return Status::error(ErrorCode::InvalidInput,
                           "relation confidence must be within [0,1]");
    }

    const RelationResolution relation = ontology_.resolve(record.predicate);
    if (!relation.known && options.reject_unknown_relations) {
      return Status::error(ErrorCode::InvalidInput,
                           "unknown relation: " + record.predicate);
    }
    if (relation.canonical_id.empty()) {
      return Status::error(ErrorCode::InvalidInput,
                           "relation predicate cannot be empty");
    }
    const Status type_status = ontology_.validate_types(
        record.predicate, record.subject_type, record.object_type);
    if (!type_status) return type_status;
    if (!relation.known) {
      warnings.push_back("UNKNOWN_RELATION_PRESERVED: " + record.predicate);
    }
    relation_mapping[record.predicate] = relation.canonical_id;

    ResolvedEntity subject;
    ResolvedEntity object;
    Status status = resolve_entity(
        resolver_, record.subject, record.subject_type, record.source_id,
        options.reject_ambiguous_entities, &subject, &warnings);
    if (!status) return status;
    status = resolve_entity(
        resolver_, record.object, record.object_type, record.source_id,
        options.reject_ambiguous_entities, &object, &warnings);
    if (!status) return status;
    entity_mapping[record.subject] = subject.external_id;
    entity_mapping[record.object] = object.external_id;

    auto ensure_node = [&](const ResolvedEntity& entity,
                           bool root,
                           bool symptom) {
      auto it = nodes.find(entity.external_id);
      if (it == nodes.end()) {
        ExtractionNode node;
        node.external_id = entity.external_id;
        node.content = entity.content;
        node.vector = deterministic_embedding(entity.content, db_.dimension());
        node.signature = fnv1a(entity.type + ":" + entity.content);
        node.incident = options.incident;
        node.role = root ? ExtractionRole::Root
                         : (symptom ? ExtractionRole::Symptom
                                    : ExtractionRole::Node);
        node.metadata["canonical_entity_id"] = entity.external_id;
        node.metadata["entity_type"] = entity.type;
        node.metadata["canonical_name"] = entity.content;
        nodes.emplace(entity.external_id, std::move(node));
      } else {
        it->second.role = merge_role(it->second.role, root, symptom);
      }
    };
    ensure_node(subject, record.subject_root, record.subject_symptom);
    ensure_node(object, record.object_root, record.object_symptom);

    ExtractionRelation edge;
    edge.from_external_id = subject.external_id;
    edge.to_external_id = object.external_id;
    edge.origin = EdgeOrigin::Observed;
    const auto definition = ontology_.get(relation.canonical_id);
    if (relation.canonical_id == "contradicts") {
      edge.role = EdgeRole::Contradicts;
    } else if (relation.canonical_id == "supersedes") {
      edge.role = EdgeRole::Supersedes;
    } else if (definition && definition->causal) {
      edge.role = EdgeRole::Causal;
    } else {
      edge.role = EdgeRole::Supports;
    }
    edge.confidence = record.confidence;
    edge.bond_type = BondType::Synthetic;
    edge.layer_coupling = LayerCoupling::Synthetic;
    edge.bond_strength = record.confidence;
    edge.evidence_id = record.source_id + ":" + std::to_string(index);
    edge.evidence_uri = record.source_id;
    edge.evidence_text = record.evidence_text;
    edge.metadata = record.metadata;
    edge.metadata["source_id"] = record.source_id;
    edge.metadata["canonical_relation"] = relation.canonical_id;
    edge.metadata["original_predicate"] = record.predicate;
    edge.metadata["ontology_version"] = ontology_.version();
    edge.metadata["relation_known"] = relation.known ? "true" : "false";
    edge.metadata["subject_type"] = subject.type;
    edge.metadata["object_type"] = object.type;
    if (!record.valid_from.empty()) edge.metadata["valid_from"] = record.valid_from;
    if (!record.valid_until.empty()) edge.metadata["valid_until"] = record.valid_until;
    extraction.relations.push_back(std::move(edge));
  }

  for (auto& [_, node] : nodes) extraction.nodes.push_back(std::move(node));
  std::sort(extraction.nodes.begin(), extraction.nodes.end(),
            [](const ExtractionNode& left, const ExtractionNode& right) {
              return left.external_id < right.external_id;
            });

  CanonicalIngestionResult result;
  const Status status = db_.put_extraction(extraction, &result.extraction);
  if (!status) return status;
  result.warnings = std::move(warnings);
  result.relation_mapping = std::move(relation_mapping);
  result.entity_mapping = std::move(entity_mapping);
  if (out) *out = std::move(result);
  return Status::ok();
}

}  // namespace graphene
