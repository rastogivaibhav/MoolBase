#pragma once

#include "graphene/types.hpp"

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
};

struct RelationResolution {
  std::string input;
  std::string canonical_id;
  bool known{false};
  bool alias_match{false};
};

class RelationOntology {
 public:
  explicit RelationOntology(std::string version = "v1");

  Status register_relation(const RelationDefinition& definition);
  RelationResolution resolve(const std::string& predicate) const;
  std::optional<RelationDefinition> get(const std::string& canonical_id) const;
  Status validate_types(const std::string& predicate,
                        const std::string& subject_type,
                        const std::string& object_type) const;
  const std::string& version() const { return version_; }
  size_t size() const { return definitions_.size(); }

  static std::string normalize(const std::string& value);
  static RelationOntology defaults();

 private:
  std::string version_;
  std::map<std::string, RelationDefinition> definitions_;
  std::map<std::string, std::string> alias_to_id_;
};

}  // namespace graphene
