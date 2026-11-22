#include "graphene/relation_ontology.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace graphene {

std::string normalise_relation_name(const std::string& value) {
  std::string output;
  bool underscore = false;
  for (unsigned char ch : value) {
    if (std::isalnum(ch)) {
      output.push_back(static_cast<char>(std::tolower(ch)));
      underscore = false;
    } else if (!output.empty() && !underscore) {
      output.push_back('_');
      underscore = true;
    }
  }
  while (!output.empty() && output.back() == '_') output.pop_back();
  return output;
}

bool RelationOntology::register_relation(RelationDefinition definition) {
  definition.id = normalise_relation_name(definition.id);
  if (definition.id.empty() || definitions_.count(definition.id)) return false;

  // Reject ambiguous aliases *before* mutating the registry, even when two
  // spellings are internal collisions within a single definition.
  std::set<std::string> proposed;
  const auto validate_alias = [&](const std::string& alias) {
    const std::string name = normalise_relation_name(alias);
    return !name.empty() && proposed.insert(name).second &&
           !alias_to_resolution_.count(name);
  };
  if (!validate_alias(definition.id)) return false;
  for (const auto& alias : definition.aliases) {
    if (!validate_alias(alias)) return false;
  }
  for (const auto& alias : definition.inverse_aliases) {
    if (!validate_alias(alias)) return false;
  }

  definitions_[definition.id] = definition;
  alias_to_resolution_[definition.id] = {definition.id, false};
  for (const auto& alias : definition.aliases) {
    alias_to_resolution_[normalise_relation_name(alias)] = {definition.id, false};
  }
  for (const auto& alias : definition.inverse_aliases) {
    alias_to_resolution_[normalise_relation_name(alias)] = {definition.id, true};
  }
  return true;
}

std::optional<RelationResolution> RelationOntology::resolve(
    const std::string& predicate) const {
  const auto it = alias_to_resolution_.find(normalise_relation_name(predicate));
  if (it == alias_to_resolution_.end()) return std::nullopt;
  return it->second;
}

std::optional<std::string> RelationOntology::canonicalise(
    const std::string& predicate) const {
  const auto resolved = resolve(predicate);
  // A string-only return type is insufficient to encode inverse direction.
  if (!resolved || resolved->swap_endpoints) return std::nullopt;
  return resolved->canonical_id;
}

std::optional<RelationDefinition> RelationOntology::definition(
    const std::string& canonical_id) const {
  const auto it = definitions_.find(normalise_relation_name(canonical_id));
  if (it == definitions_.end()) return std::nullopt;
  return it->second;
}

RelationValidation RelationOntology::validate(const std::string& canonical_id,
                                              const std::string& subject_type,
                                              const std::string& object_type) const {
  RelationValidation output;
  const auto found = definition(canonical_id);
  if (!found) {
    output.valid = false;
    output.warnings.push_back("UNKNOWN_RELATION");
    return output;
  }
  if (!found->domain_types.empty() && !subject_type.empty() &&
      !found->domain_types.count(subject_type)) {
    output.valid = false;
    output.warnings.push_back("DOMAIN_TYPE_MISMATCH");
  }
  if (!found->range_types.empty() && !object_type.empty() &&
      !found->range_types.count(object_type)) {
    output.valid = false;
    output.warnings.push_back("RANGE_TYPE_MISMATCH");
  }
  return output;
}

RelationOntology RelationOntology::with_core_relations() {
  RelationOntology ontology;
  // Canonical: "A caused B"; "B caused_by A" reverses endpoints.
  ontology.register_relation({"caused", {"causes", "resulted_in"}, {}, {},
                              std::nullopt, false, false, true, true,
                              {"caused_by", "resulted_from"}});
  ontology.register_relation({"supports", {"evidence_for"}, {}, {},
                              std::nullopt, false, false, true, false,
                              {"supported_by"}});
  ontology.register_relation({"contradicts", {"conflicts_with", "opposes"}, {}, {},
                              std::nullopt, true, false, true, false});
  ontology.register_relation({"supersedes", {"replaces", "updates"}, {}, {},
                              std::nullopt, false, false, true, false});
  ontology.register_relation({"located_in", {"hosted_in", "deployed_to", "runs_in", "current_depot"},
                              {}, {}, std::nullopt, false, false, true, false});
  ontology.register_relation({"contains", {"includes", "holds"}, {}, {},
                              std::nullopt, false, false, false, false});
  // Calling, using and requiring an entity are not logically interchangeable.
  ontology.register_relation({"depends_on", {}, {}, {},
                              std::nullopt, false, false, true, false});
  ontology.register_relation({"calls", {}, {}, {},
                              std::nullopt, false, false, true, false});
  ontology.register_relation({"requires", {}, {}, {},
                              std::nullopt, false, false, true, false});
  ontology.register_relation({"uses", {}, {}, {},
                              std::nullopt, false, false, true, false});
  ontology.register_relation({"assigned_to", {"assigned_vehicle"}, {}, {},
                              std::nullopt, false, false, true, false});
  ontology.register_relation({"owned_by", {}, {}, {},
                              std::nullopt, false, false, true, false});
  ontology.register_relation({"directed_by", {"was_directed_by"}, {"work"}, {"person"},
                              std::nullopt, false, false, false, false});
  ontology.register_relation({"authored_by", {"written_by"}, {"work"}, {"person"},
                              std::nullopt, false, false, false, false});
  // Canonical mother/father: child -> parent. mother_of: parent -> child.
  ontology.register_relation({"mother", {"has_mother"}, {"person"}, {"person"},
                              std::nullopt, false, false, false, false, {"mother_of"}});
  ontology.register_relation({"father", {"has_father"}, {"person"}, {"person"},
                              std::nullopt, false, false, false, false, {"father_of"}});
  ontology.register_relation({"spouse", {"married_to"}, {"person"}, {"person"},
                              std::nullopt, true, false, true, false});
  return ontology;
}

}  // namespace graphene
