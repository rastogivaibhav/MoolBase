#include "graphene/generic_relation.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <sstream>

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

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n\"");
  if (first == std::string::npos) return {};
  const auto last = value.find_last_not_of(" \t\r\n\"");
  return value.substr(first, last - first + 1);
}

std::vector<std::string> split(const std::string& line, char delimiter) {
  std::vector<std::string> values;
  std::stringstream input(line);
  std::string value;
  while (std::getline(input, value, delimiter)) values.push_back(trim(value));
  return values;
}

bool contains_name(const std::vector<std::string>& values, const std::string& name) {
  const std::string normalised = normalise_entity_name(name);
  return std::any_of(values.begin(), values.end(), [&](const std::string& value) {
    return normalise_entity_name(value) == normalised;
  });
}

EdgeRole role_for(const std::string& relation) {
  if (relation == "caused") return EdgeRole::Causal;
  if (relation == "contradicts") return EdgeRole::Contradicts;
  if (relation == "supersedes") return EdgeRole::Supersedes;
  return EdgeRole::Supports;
}

}  // namespace

std::vector<float> deterministic_text_vector(const std::string& text,
                                             uint32_t dimension) {
  std::vector<float> output(dimension, 0.0f);
  if (dimension == 0) return output;
  std::stringstream words(normalise_entity_name(text));
  std::string word;
  while (words >> word) {
    const uint64_t hash = fnv1a(word);
    const size_t index = static_cast<size_t>(hash % dimension);
    output[index] += (hash & 1ULL) ? 1.0f : -1.0f;
  }
  double norm = 0.0;
  for (float value : output) norm += static_cast<double>(value) * value;
  if (norm > 0.0) {
    const float scale = static_cast<float>(1.0 / std::sqrt(norm));
    for (float& value : output) value *= scale;
  }
  return output;
}

uint64_t deterministic_text_signature(const std::string& text) {
  const uint64_t hash = fnv1a(normalise_entity_name(text));
  return (1ULL << (hash % 16)) | (1ULL << (16 + ((hash >> 8) % 16)));
}

GenericRelationParseResult parse_generic_relations(const std::string& text,
                                                   const std::string& format,
                                                   const std::string& source_id) {
  GenericRelationParseResult output;
  std::stringstream input(text);
  std::string line;
  size_t line_number = 0;
  while (std::getline(input, line)) {
    ++line_number;
    line = trim(line);
    if (line.empty() || line[0] == '#') continue;
    GenericRelationRecord record;
    record.source_id = source_id;
    bool parsed = false;
    if (format == "tsv" || format == "pipe") {
      const auto values = split(line, format == "tsv" ? '\t' : '|');
      if (values.size() >= 3) {
        record.subject = values[0];
        record.predicate = values[1];
        record.object = values[2];
        if (values.size() > 3) record.subject_type = values[3];
        if (values.size() > 4) record.object_type = values[4];
        parsed = true;
      }
    } else if (format == "rdf") {
      if (!line.empty() && line.back() == '.') line.pop_back();
      std::stringstream row(line);
      if (row >> record.subject >> record.predicate >> record.object) {
        record.subject = trim(record.subject);
        record.predicate = trim(record.predicate);
        record.object = trim(record.object);
        if (!record.subject.empty() && record.subject.front() == '<' && record.subject.back() == '>')
          record.subject = record.subject.substr(1, record.subject.size() - 2);
        if (!record.predicate.empty() && record.predicate.front() == '<' && record.predicate.back() == '>')
          record.predicate = record.predicate.substr(1, record.predicate.size() - 2);
        if (!record.object.empty() && record.object.front() == '<' && record.object.back() == '>')
          record.object = record.object.substr(1, record.object.size() - 2);
        parsed = true;
      }
    } else if (format == "token") {
      std::stringstream row(line);
      std::string token;
      while (row >> token) {
        const auto position = token.find('=');
        if (position == std::string::npos) continue;
        const std::string key = token.substr(0, position);
        const std::string value = trim(token.substr(position + 1));
        if (key == "SUBJ" || key == "S") record.subject = value;
        else if (key == "REL" || key == "P") record.predicate = value;
        else if (key == "OBJ" || key == "O") record.object = value;
      }
      parsed = !record.subject.empty() && !record.predicate.empty() && !record.object.empty();
    }
    if (!parsed) {
      output.errors.push_back("line " + std::to_string(line_number) + ": cannot parse " + format + " relation");
      continue;
    }
    output.records.push_back(std::move(record));
  }
  return output;
}

Status GenericRelationIngestor::ingest(GrapheneDB& db,
                                       const std::vector<GenericRelationRecord>& records,
                                       const RelationOntology& ontology,
                                       const GenericIngestOptions& options,
                                       GenericIngestResult* out) const {
  if (!out) return Status::error(ErrorCode::InvalidInput, "generic ingest result cannot be null");
  if (options.dimension == 0 || db.dimension() != options.dimension) {
    return Status::error(ErrorCode::InvalidInput, "generic ingest dimension must match the open database");
  }
  GenericIngestResult result;
  auto ensure_entity = [&](const std::string& name, const std::string& type) -> std::optional<uint32_t> {
    const std::string key = normalise_entity_name(name);
    const auto existing_local = result.entity_nodes.find(key);
    if (existing_local != result.entity_nodes.end()) return existing_local->second;
    const auto existing = db.metadata_search("entity_key", key);
    if (existing.size() > 1) {
      result.warnings.push_back("AMBIGUOUS_ENTITY:" + name);
      return std::nullopt;
    }
    if (existing.size() == 1) {
      result.entity_nodes[key] = existing.front();
      return existing.front();
    }
    NodeInput node;
    node.content = name;
    node.vector = deterministic_text_vector(name, options.dimension);
    node.signature = deterministic_text_signature(name);
    node.root = contains_name(options.root_entities, name);
    node.symptom = contains_name(options.symptom_entities, name);
    node.metadata["entity_key"] = key;
    node.metadata["entity_type"] = type;
    node.metadata["source"] = "generic-relation-ingestor";
    uint32_t id = 0;
    const Status status = db.put_node(node, &id);
    if (!status) return std::nullopt;
    result.entity_nodes[key] = id;
    result.inserted_nodes.push_back(id);
    return id;
  };

  for (const auto& record : records) {
    auto canonical = ontology.canonicalise(record.predicate);
    std::string predicate;
    if (!canonical) {
      predicate = normalise_relation_name(record.predicate);
      if (options.reject_unknown_relations) {
        return Status::error(ErrorCode::InvalidInput, "unknown relation: " + record.predicate);
      }
      result.warnings.push_back("UNKNOWN_RELATION_PRESERVED:" + predicate);
    } else {
      predicate = *canonical;
      const auto validation = ontology.validate(predicate, record.subject_type, record.object_type);
      result.warnings.insert(result.warnings.end(), validation.warnings.begin(), validation.warnings.end());
      if (!validation.valid && options.reject_unknown_relations) {
        return Status::error(ErrorCode::InvalidInput, "relation type validation failed: " + predicate);
      }
    }
    const auto subject = ensure_entity(record.subject, record.subject_type);
    const auto object = ensure_entity(record.object, record.object_type);
    if (!subject || !object) {
      result.warnings.push_back("RELATION_SKIPPED_AMBIGUOUS_ENTITY:" + record.subject + "->" + record.object);
      continue;
    }
    EdgeInput edge;
    edge.from = *subject;
    edge.to = *object;
    edge.origin = EdgeOrigin::Observed;
    edge.role = role_for(predicate);
    edge.confidence = std::clamp(record.confidence, 0.0, 1.0);
    edge.metadata["relation_id"] = predicate;
    edge.metadata["source_id"] = record.source_id.empty() ? "generic-input" : record.source_id;
    if (!record.valid_from.empty()) edge.metadata["valid_from"] = record.valid_from;
    if (!record.valid_until.empty()) edge.metadata["valid_until"] = record.valid_until;
    uint32_t edge_id = 0;
    const Status status = db.put_edge(edge, &edge_id);
    if (!status) return status;
    result.inserted_edges.push_back(edge_id);
  }
  *out = std::move(result);
  return Status::ok();
}

}  // namespace graphene
