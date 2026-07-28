#include "graphene/entity_resolution.hpp"

#include <algorithm>
#include <cctype>
#include <set>

namespace graphene {
namespace {

std::set<std::string> tokens(const std::string& value) {
  std::set<std::string> output;
  std::string token;
  for (unsigned char ch : value) {
    if (std::isalnum(ch)) {
      token.push_back(static_cast<char>(std::tolower(ch)));
    } else if (!token.empty()) {
      output.insert(token);
      token.clear();
    }
  }
  if (!token.empty()) output.insert(token);
  return output;
}

double jaccard(const std::set<std::string>& left,
               const std::set<std::string>& right) {
  if (left.empty() || right.empty()) return 0.0;
  size_t intersection = 0;
  for (const auto& value : left) {
    if (right.count(value) != 0) ++intersection;
  }
  const size_t union_size = left.size() + right.size() - intersection;
  return union_size == 0
             ? 0.0
             : static_cast<double>(intersection) /
                   static_cast<double>(union_size);
}

std::set<std::string> normalised_terms(const std::vector<std::string>& values) {
  std::set<std::string> output;
  for (const auto& value : values) {
    const auto value_tokens = tokens(value);
    output.insert(value_tokens.begin(), value_tokens.end());
  }
  return output;
}

}  // namespace

std::string EntityResolver::normalize(const std::string& value) {
  std::string output;
  bool separator = false;
  for (unsigned char ch : value) {
    if (std::isalnum(ch)) {
      output.push_back(static_cast<char>(std::tolower(ch)));
      separator = false;
    } else if (!output.empty() && !separator) {
      output.push_back(' ');
      separator = true;
    }
  }
  while (!output.empty() && output.back() == ' ') output.pop_back();
  return output;
}

Status EntityResolver::add_entity(const EntityRecord& requested) {
  EntityRecord entity = requested;
  if (entity.id.empty() || entity.id.size() > 256 ||
      entity.canonical_name.empty()) {
    return Status::error(ErrorCode::InvalidInput,
                         "entity id and canonical name are required");
  }
  if (entities_.count(entity.id) != 0) {
    return Status::error(ErrorCode::InvalidInput,
                         "entity id is already registered");
  }
  entity.type = normalize(entity.type);
  std::vector<std::string> aliases;
  for (const auto& alias : entity.aliases) {
    const std::string normal = normalize(alias);
    if (!normal.empty() &&
        std::find(aliases.begin(), aliases.end(), normal) == aliases.end()) {
      aliases.push_back(normal);
    }
  }
  entity.aliases = std::move(aliases);
  entities_[entity.id] = std::move(entity);
  return Status::ok();
}

EntityResolutionResult EntityResolver::resolve(
    const EntityMention& mention,
    const EntityResolutionOptions& requested_options) const {
  EntityResolutionOptions options = requested_options;
  options.minimum_score = std::clamp(options.minimum_score, 0.0, 1.0);
  options.ambiguity_margin = std::clamp(options.ambiguity_margin, 0.0, 1.0);
  options.max_candidates = std::clamp<size_t>(options.max_candidates, 1, 64);

  EntityResolutionResult output;
  output.mention = mention;
  const std::string mention_name = normalize(mention.text);
  const std::string mention_type = normalize(mention.type);
  const auto mention_tokens = tokens(mention.text);
  const auto mention_context = normalised_terms(mention.context_terms);

  for (const auto& [id, entity] : entities_) {
    EntityCandidate candidate;
    candidate.entity_id = id;
    const std::string canonical = normalize(entity.canonical_name);
    candidate.lexical_score = canonical == mention_name ? 1.0
                                                        : jaccard(mention_tokens,
                                                                  tokens(canonical));
    for (const auto& alias : entity.aliases) {
      if (alias == mention_name) {
        candidate.lexical_score = 1.0;
        break;
      }
      candidate.lexical_score = std::max(
          candidate.lexical_score, jaccard(mention_tokens, tokens(alias)));
    }
    if (mention_type.empty()) {
      candidate.type_score = 0.5;
    } else {
      candidate.type_score = entity.type == mention_type ? 1.0 : 0.0;
    }
    candidate.context_score =
        jaccard(mention_context, normalised_terms(entity.context_terms));
    candidate.total_score = std::clamp(
        0.70 * candidate.lexical_score + 0.20 * candidate.type_score +
            0.10 * candidate.context_score,
        0.0, 1.0);
    if (candidate.total_score > 0.0) output.candidates.push_back(candidate);
  }

  std::sort(output.candidates.begin(), output.candidates.end(),
            [](const EntityCandidate& left, const EntityCandidate& right) {
              if (left.total_score != right.total_score) {
                return left.total_score > right.total_score;
              }
              return left.entity_id < right.entity_id;
            });
  if (output.candidates.size() > options.max_candidates) {
    output.candidates.resize(options.max_candidates);
  }
  if (output.candidates.empty() ||
      output.candidates.front().total_score < options.minimum_score) {
    output.unresolved = true;
    return output;
  }
  if (output.candidates.size() > 1 &&
      output.candidates.front().total_score -
              output.candidates[1].total_score <
          options.ambiguity_margin) {
    output.ambiguous = true;
    return output;
  }
  output.resolved_entity_id = output.candidates.front().entity_id;
  return output;
}

std::optional<EntityRecord> EntityResolver::get(
    const std::string& entity_id) const {
  const auto it = entities_.find(entity_id);
  if (it == entities_.end()) return std::nullopt;
  return it->second;
}

}  // namespace graphene
