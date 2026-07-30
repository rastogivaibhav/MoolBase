#include "graphene/relation_ontology.hpp"

#include <algorithm>
#include <cctype>

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
  std::vector<std::string> all_aliases = definition.aliases;
  all_aliases.push_back(definition.id);
  for (const auto& alias : all_aliases) {
    const std::string normalised = normalise_relation_name(alias);
    const auto existing = alias_to_id_.find(normalised);
    if (normalised.empty() || (existing != alias_to_id_.end() && existing->second != definition.id)) {
      return false;
    }
  }
  definitions_[definition.id] = definition;
  for (const auto& alias : all_aliases) {
    alias_to_id_[normalise_relation_name(alias)] = definition.id;
  }
  return true;
}

std::optional<std::string> RelationOntology::canonicalise(const std::string& predicate) const {
  const auto it = alias_to_id_.find(normalise_relation_name(predicate));
  if (it == alias_to_id_.end()) return std::nullopt;
  return it->second;
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
  ontology.register_relation({"caused", {"causes", "caused_by", "resulted_in"}, {}, {}, std::nullopt, false, false, true, true});
  ontology.register_relation({"supports", {"supported_by", "evidence_for"}, {}, {}, std::nullopt, false, false, true, false});
  ontology.register_relation({"contradicts", {"conflicts_with", "opposes"}, {}, {}, std::nullopt, true, false, true, false});
  ontology.register_relation({"supersedes", {"replaces", "updates"}, {}, {}, std::nullopt, false, false, true, false});
  ontology.register_relation({"located_in", {"hosted_in", "deployed_to", "runs_in", "current_depot"}, {}, {}, std::nullopt, false, false, true, false});
  ontology.register_relation({"contains", {"includes", "holds"}, {}, {}, std::nullopt, false, false, false, false});
  ontology.register_relation({"depends_on", {"calls", "requires", "uses"}, {}, {}, std::nullopt, false, false, true, false});
  ontology.register_relation({"assigned_to", {"assigned_vehicle", "owned_by"}, {}, {}, std::nullopt, false, false, true, false});
  ontology.register_relation({"directed_by", {"director", "was_directed_by"}, {"work"}, {"person"}, std::nullopt, false, false, false, false});
  ontology.register_relation({"authored_by", {"author", "written_by"}, {"work"}, {"person"}, std::nullopt, false, false, false, false});
  ontology.register_relation({"mother", {"mother_of", "has_mother"}, {"person"}, {"person"}, std::nullopt, false, false, false, false});
  ontology.register_relation({"father", {"father_of", "has_father"}, {"person"}, {"person"}, std::nullopt, false, false, false, false});
  ontology.register_relation({"spouse", {"married_to", "wife", "husband"}, {"person"}, {"person"}, std::nullopt, true, false, true, false});
  return ontology;
}

}  // namespace graphene
