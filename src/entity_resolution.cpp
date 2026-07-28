#include "graphene/entity_resolution.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace graphene {
namespace {

std::set<std::string> tokens(const std::vector<std::string>& values) {
  std::set<std::string> output;
  for (const auto& value : values) {
    std::string current;
    for (unsigned char ch : value) {
      if (std::isalnum(ch)) current.push_back(static_cast<char>(std::tolower(ch)));
      else if (!current.empty()) { output.insert(current); current.clear(); }
    }
    if (!current.empty()) output.insert(current);
  }
  return output;
}

double overlap(const std::vector<std::string>& left,
               const std::vector<std::string>& right) {
  const auto a = tokens(left);
  const auto b = tokens(right);
  if (a.empty() || b.empty()) return 0.0;
  size_t intersection = 0;
  for (const auto& value : a) if (b.count(value)) ++intersection;
  const size_t union_size = a.size() + b.size() - intersection;
  return union_size == 0 ? 0.0 : static_cast<double>(intersection) /
                                      static_cast<double>(union_size);
}

}  // namespace

std::string normalise_entity_name(const std::string& value) {
  std::string output;
  bool space = false;
  for (unsigned char ch : value) {
    if (std::isalnum(ch)) {
      output.push_back(static_cast<char>(std::tolower(ch)));
      space = false;
    } else if (!output.empty() && !space) {
      output.push_back(' ');
      space = true;
    }
  }
  while (!output.empty() && output.back() == ' ') output.pop_back();
  return output;
}

bool EntityResolver::add(EntityRecord record) {
  if (record.id == 0 || normalise_entity_name(record.canonical_name).empty()) return false;
  if (std::any_of(records_.begin(), records_.end(),
                  [&](const EntityRecord& existing) { return existing.id == record.id; })) return false;
  records_.push_back(std::move(record));
  return true;
}

EntityResolution EntityResolver::resolve(const EntityMention& mention,
                                         double minimum_score,
                                         double ambiguity_margin) const {
  EntityResolution output;
  const std::string needle = normalise_entity_name(mention.text);
  for (const auto& record : records_) {
    double lexical = normalise_entity_name(record.canonical_name) == needle ? 1.0 : 0.0;
    for (const auto& alias : record.aliases) {
      if (normalise_entity_name(alias) == needle) lexical = std::max(lexical, 0.98);
    }
    if (lexical == 0.0) continue;
    const double type = mention.expected_type.empty() ? 0.5 :
                        (mention.expected_type == record.type ? 1.0 : 0.0);
    const double context = overlap(mention.context_terms, record.context_terms);
    const double total = 0.72 * lexical + 0.18 * type + 0.10 * context;
    output.candidates.push_back({record.id, lexical, type, context, total});
  }
  std::sort(output.candidates.begin(), output.candidates.end(),
            [](const EntityCandidate& left, const EntityCandidate& right) {
              if (std::abs(left.total_score - right.total_score) > 1e-12)
                return left.total_score > right.total_score;
              return left.entity_id < right.entity_id;
            });
  if (output.candidates.empty() || output.candidates.front().total_score < minimum_score) return output;
  if (output.candidates.size() > 1 &&
      output.candidates.front().total_score - output.candidates[1].total_score <= ambiguity_margin) {
    output.ambiguous = true;
    return output;
  }
  output.resolved = output.candidates.front().entity_id;
  return output;
}

std::optional<EntityRecord> EntityResolver::get(uint64_t id) const {
  const auto it = std::find_if(records_.begin(), records_.end(),
                               [&](const EntityRecord& record) { return record.id == id; });
  if (it == records_.end()) return std::nullopt;
  return *it;
}

}  // namespace graphene
