#include "graphene/relation_ontology.hpp"

#include <algorithm>
#include <cctype>

namespace graphene {

RelationOntology::RelationOntology(std::string version)
    : version_(std::move(version)) {}

std::string RelationOntology::normalize(const std::string& value) {
  std::string output;
  bool previous_separator = false;
  for (unsigned char ch : value) {
    if (std::isalnum(ch)) {
      output.push_back(static_cast<char>(std::tolower(ch)));
      previous_separator = false;
    } else if (!output.empty() && !previous_separator) {
      output.push_back('_');
      previous_separator = true;
    }
  }
  while (!output.empty() && output.back() == '_') output.pop_back();
  return output;
}

Status RelationOntology::register_relation(
    const RelationDefinition& requested) {
  RelationDefinition definition = requested;
  definition.id = normalize(definition.id);
  if (definition.id.empty()) {
    return Status::error(ErrorCode::InvalidInput,
                         "relation canonical id is required");
  }
  if (definitions_.count(definition.id) != 0) {
    return Status::error(ErrorCode::InvalidInput,
                         "relation canonical id already registered");
  }

  std::vector<std::string> aliases;
  aliases.push_back(definition.id);
  for (const auto& alias : definition.aliases) {
    const std::string normalized = normalize(alias);
    if (!normalized.empty() &&
        std::find(aliases.begin(), aliases.end(), normalized) == aliases.end()) {
      aliases.push_back(normalized);
    }
  }
  for (const auto& alias : aliases) {
    const auto existing = alias_to_id_.find(alias);
    if (existing != alias_to_id_.end() && existing->second != definition.id) {
      return Status::error(ErrorCode::InvalidInput,
                           "relation alias conflicts with existing relation: " + alias);
    }
  }

  std::set<std::string> domains;
  for (const auto& type : definition.domain_types) {
    const std::string normalized = normalize(type);
    if (!normalized.empty()) domains.insert(normalized);
  }
  std::set<std::string> ranges;
  for (const auto& type : definition.range_types) {
    const std::string normalized = normalize(type);
    if (!normalized.empty()) ranges.insert(normalized);
  }
  definition.domain_types = std::move(domains);
  definition.range_types = std::move(ranges);
  if (definition.inverse) {
    *definition.inverse = normalize(*definition.inverse);
    if (definition.inverse->empty()) definition.inverse.reset();
  }
  definition.aliases.assign(aliases.begin() + 1, aliases.end());
  definitions_[definition.id] = definition;
  for (const auto& alias : aliases) alias_to_id_[alias] = definition.id;
  return Status::ok();
}

RelationResolution RelationOntology::resolve(
    const std::string& predicate) const {
  RelationResolution output;
  output.input = predicate;
  const std::string normalized = normalize(predicate);
  const auto alias = alias_to_id_.find(normalized);
  if (alias == alias_to_id_.end()) {
    output.canonical_id = normalized;
    return output;
  }
  output.canonical_id = alias->second;
  output.known = true;
  output.alias_match = normalized != alias->second;
  return output;
}

std::optional<RelationDefinition> RelationOntology::get(
    const std::string& canonical_id) const {
  const auto it = definitions_.find(normalize(canonical_id));
  if (it == definitions_.end()) return std::nullopt;
  return it->second;
}

Status RelationOntology::validate_types(const std::string& predicate,
                                        const std::string& subject_type,
                                        const std::string& object_type) const {
  const RelationResolution resolution = resolve(predicate);
  if (!resolution.known) return Status::ok();
  const auto definition = get(resolution.canonical_id);
  if (!definition) return Status::ok();
  const std::string subject = normalize(subject_type);
  const std::string object = normalize(object_type);
  if (!definition->domain_types.empty() &&
      definition->domain_types.count(subject) == 0) {
    return Status::error(ErrorCode::InvalidInput,
                         "subject type violates relation domain");
  }
  if (!definition->range_types.empty() &&
      definition->range_types.count(object) == 0) {
    return Status::error(ErrorCode::InvalidInput,
                         "object type violates relation range");
  }
  return Status::ok();
}

RelationOntology RelationOntology::defaults() {
  RelationOntology ontology("graphene-default-v1");
  ontology.register_relation(
      {"caused_by", {"caused by", "root_cause", "due_to"}, {}, {},
       std::optional<std::string>{"causes"}, false, false, true, true});
  ontology.register_relation(
      {"causes", {"caused", "leads_to", "results_in"}, {}, {},
       std::optional<std::string>{"caused_by"}, false, false, true, true});
  ontology.register_relation(
      {"located_in", {"located at", "hosted_in", "deployed_to"}, {}, {},
       std::nullopt, false, false, true, false});
  ontology.register_relation(
      {"depends_on", {"requires", "calls", "uses"}, {}, {},
       std::nullopt, false, false, false, false});
  ontology.register_relation(
      {"contradicts", {"conflicts_with", "opposes"}, {}, {},
       std::nullopt, true, false, true, false});
  ontology.register_relation(
      {"supersedes", {"replaces", "newer_than"}, {}, {},
       std::nullopt, false, true, true, false});
  return ontology;
}

}  // namespace graphene
