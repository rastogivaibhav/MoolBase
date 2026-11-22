#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace graphene {

struct RelationDefinition {
  std::string id;
  std::vector<std::string> aliases;
  std::set<std::string> domain_types;
  std::set<std::string> range_types;
  std::optional<std::string> inverse;
  bool symmetric{false};
  bool transitive{false};
  bool temporal{false};
  bool causal{false};
  // An inverse alias reverses the endpoints; it is never a synonym.
  // Kept last for source compatibility with existing aggregate definitions.
  std::vector<std::string> inverse_aliases;
};

struct RelationResolution {
  std::string canonical_id;
  bool swap_endpoints{false};
};

struct RelationValidation {
  bool valid{true};
  std::vector<std::string> warnings;
};

class RelationOntology {
 public:
  bool register_relation(RelationDefinition definition);
  // Direction-aware lookup is mandatory for relation ingestion.
  std::optional<RelationResolution> resolve(const std::string& predicate) const;
  // Legacy canonicalisation only returns forward relations. Inverse aliases
  // return nullopt rather than silently reversing a scientific assertion.
  std::optional<std::string> canonicalise(const std::string& predicate) const;
  std::optional<RelationDefinition> definition(const std::string& canonical_id) const;
  RelationValidation validate(const std::string& canonical_id,
                              const std::string& subject_type,
                              const std::string& object_type) const;
  size_t size() const { return definitions_.size(); }

  static RelationOntology with_core_relations();

 private:
  std::map<std::string, RelationDefinition> definitions_;
  std::map<std::string, RelationResolution> alias_to_resolution_;
};

std::string normalise_relation_name(const std::string& value);

}  // namespace graphene
