#pragma once

#include "graphene/types.hpp"

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace graphene {

struct EntityRecord {
  std::string id;
  std::string canonical_name;
  std::string type;
  std::vector<std::string> aliases;
  std::vector<std::string> context_terms;
};

struct EntityMention {
  std::string text;
  std::string type;
  std::vector<std::string> context_terms;
  std::string source_id;
};

struct EntityCandidate {
  std::string entity_id;
  double lexical_score{0.0};
  double type_score{0.0};
  double context_score{0.0};
  double total_score{0.0};
};

struct EntityResolutionResult {
  EntityMention mention;
  std::vector<EntityCandidate> candidates;
  std::optional<std::string> resolved_entity_id;
  bool ambiguous{false};
  bool unresolved{false};
};

struct EntityResolutionOptions {
  double minimum_score{0.65};
  double ambiguity_margin{0.10};
  size_t max_candidates{8};
};

class EntityResolver {
 public:
  Status add_entity(const EntityRecord& entity);
  EntityResolutionResult resolve(
      const EntityMention& mention,
      const EntityResolutionOptions& options = {}) const;
  std::optional<EntityRecord> get(const std::string& entity_id) const;
  size_t size() const { return entities_.size(); }

  static std::string normalize(const std::string& value);

 private:
  std::map<std::string, EntityRecord> entities_;
};

}  // namespace graphene
