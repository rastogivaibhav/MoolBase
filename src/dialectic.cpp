#include "graphene/dialectic.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <deque>
#include <limits>
#include <map>
#include <set>
#include <unordered_set>

namespace graphene {
namespace {

double clamp01(double value) {
  return std::clamp(value, 0.0, 1.0);
}

bool edge_allowed(const Edge& edge, QueryMode mode) {
  if (mode == QueryMode::Empirical) {
    if (edge.origin != EdgeOrigin::Observed &&
        edge.origin != EdgeOrigin::Discovered) {
      return false;
    }
    if (edge.role == EdgeRole::Analogical) return false;
    if (edge.role == EdgeRole::Compressed) {
      const auto derivation = edge.metadata.find("derived_from");
      if (derivation == edge.metadata.end() || derivation->second.empty()) {
        return false;
      }
    }
    return true;
  }
  if (mode == QueryMode::Balanced) return edge.origin != EdgeOrigin::Hypothetical;
  return true;
}

bool signature_matches(uint64_t node_signature, uint64_t query_signature) {
  if (query_signature == 0 || node_signature == 0) return true;
  const int query_bits = static_cast<int>(std::popcount(query_signature));
  const int required = std::max(1, query_bits - 1);
  return static_cast<int>(std::popcount(node_signature & query_signature)) >= required;
}

std::string metadata_value(const std::map<std::string, std::string>& metadata,
                           const std::string& key) {
  const auto it = metadata.find(key);
  return it == metadata.end() ? std::string{} : it->second;
}

bool parse_u32_list(const std::string& encoded, std::vector<uint32_t>* out) {
  if (!out || encoded.empty()) return false;
  std::vector<uint32_t> values;
  size_t start = 0;
  while (start < encoded.size()) {
    const size_t end = encoded.find(',', start);
    const size_t length =
        end == std::string::npos ? encoded.size() - start : end - start;
    if (length == 0) return false;
    uint32_t value = 0;
    const char* first = encoded.data() + start;
    const char* last = first + length;
    const auto parsed = std::from_chars(first, last, value);
    if (parsed.ec != std::errc{} || parsed.ptr != last) return false;
    values.push_back(value);
    if (end == std::string::npos) break;
    start = end + 1;
  }
  if (values.empty()) return false;
  std::sort(values.begin(), values.end());
  if (std::adjacent_find(values.begin(), values.end()) != values.end()) {
    return false;
  }
  *out = std::move(values);
  return true;
}

void append_provenance(const Edge& edge, DialecticPath* path) {
  const EdgeProvenance provenance = assess_edge_provenance(edge);
  path->evidence.insert(path->evidence.end(), provenance.evidence.begin(),
                        provenance.evidence.end());
  path->provenance_findings.insert(path->provenance_findings.end(),
                                   provenance.findings.begin(),
                                   provenance.findings.end());
}

struct ExpansionState {
  uint32_t node{0};
  uint32_t anchor{0};
  double score{0.0};
  std::vector<uint32_t> reverse_nodes;
  std::vector<uint32_t> reverse_edges;
  bool contradiction{false};
  bool hypothetical{false};
  std::vector<EvidenceRef> evidence;
  std::vector<ProvenanceFinding> findings;
  std::vector<JointRequirement> joint_requirements;
  uint32_t depth{0};
};

const RootBundle* find_root(const BundleSet& bundles, uint32_t root_node) {
  for (const auto& root : bundles.roots) {
    if (root.root_node == root_node) return &root;
  }
  return nullptr;
}

void append_unique(std::vector<std::string>* target, std::string value) {
  if (std::find(target->begin(), target->end(), value) == target->end()) {
    target->push_back(std::move(value));
  }
}

void append_unique(std::vector<uint32_t>* target, uint32_t value) {
  if (std::find(target->begin(), target->end(), value) == target->end()) {
    target->push_back(value);
  }
}

DialecticOptions bounded_options(DialecticOptions options) {
  options.semantic_candidates = std::clamp<size_t>(options.semantic_candidates, 1, 64);
  options.max_hops = std::clamp<uint32_t>(options.max_hops, 1, 16);
  options.max_paths = std::clamp<size_t>(options.max_paths, 1, 256);
  options.max_paths_per_root = std::clamp<size_t>(options.max_paths_per_root, 1, 64);
  options.max_visited_states = std::clamp<size_t>(options.max_visited_states, 1, 200000);
  options.max_selected_paths = std::clamp<size_t>(options.max_selected_paths, 1, 16);
  options.max_opposition_rounds = std::min<uint32_t>(options.max_opposition_rounds, 2);
  options.minimum_confidence = clamp01(options.minimum_confidence);
  options.reexpansion_threshold = clamp01(options.reexpansion_threshold);
  return options;
}

} // namespace

DialecticEngine::DialecticEngine(const GrapheneDB& db) : db_(db) {}

BundleSet DialecticEngine::expand(const std::vector<float>& query,
                                  uint64_t query_signature,
                                  const DialecticOptions& requested_options,
                                  uint64_t snapshot_version) const {
  const DialecticOptions options = bounded_options(requested_options);
  BundleSet output;
  output.snapshot_version =
      snapshot_version == kInfVersion ? db_.snapshot() : snapshot_version;

  std::optional<Rfc3339Instant> as_of;
  if (!options.as_of.empty()) {
    Rfc3339Instant parsed_as_of;
    const Status status = parse_rfc3339(options.as_of, &parsed_as_of);
    if (!status) {
      output.warnings.push_back("INVALID_AS_OF: " + status.message);
      return output;
    }
    as_of = parsed_as_of;
  }
  auto temporal_allowed =
      [&](const std::map<std::string, std::string>& metadata,
          const std::string& object) {
        TemporalValidity validity;
        const Status status = parse_temporal_validity(metadata, &validity);
        if (!status) {
          const std::string warning =
              "INVALID_TEMPORAL_METADATA: " + object + ": " + status.message;
          if (std::find(output.warnings.begin(), output.warnings.end(), warning) ==
              output.warnings.end()) {
            output.warnings.push_back(warning);
          }
          return false;
        }
        return !as_of || valid_at(validity, *as_of);
      };

  const size_t search_count = std::min<size_t>(64, options.semantic_candidates * 4);
  const auto raw_candidates =
      db_.vector_search(query, search_count, output.snapshot_version);
  if (raw_candidates.empty()) {
    output.warnings.push_back("NO_SEMANTIC_CANDIDATES");
    return output;
  }

  std::vector<SearchResult> candidates;
  std::unordered_set<uint32_t> selected_ids;
  for (const auto& candidate : raw_candidates) {
    auto node = db_.get_node(candidate.node_id, output.snapshot_version);
    if (!node ||
        !temporal_allowed(node->metadata,
                          "node " + std::to_string(candidate.node_id))) {
      continue;
    }
    if (!signature_matches(node->signature, query_signature)) continue;
    candidates.push_back(candidate);
    selected_ids.insert(candidate.node_id);
    if (candidates.size() >= options.semantic_candidates) break;
  }
  for (const auto& candidate : raw_candidates) {
    if (candidates.size() >= options.semantic_candidates) break;
    if (selected_ids.count(candidate.node_id) != 0) continue;
    auto node = db_.get_node(candidate.node_id, output.snapshot_version);
    if (!node ||
        !temporal_allowed(node->metadata,
                          "node " + std::to_string(candidate.node_id))) {
      continue;
    }
    candidates.push_back(candidate);
    selected_ids.insert(candidate.node_id);
  }
  for (const auto& candidate : candidates) {
    output.semantic_candidates.push_back(candidate.node_id);
  }

  std::map<uint32_t, RootBundle> grouped;
  std::map<uint32_t, std::set<std::vector<uint32_t>>> path_keys;
  size_t total_paths = 0;

  for (const auto& candidate : candidates) {
    if (total_paths >= options.max_paths || output.truncated) break;
    ExpansionState initial;
    initial.node = candidate.node_id;
    initial.anchor = candidate.node_id;
    initial.score = clamp01(candidate.score);
    initial.reverse_nodes.push_back(candidate.node_id);
    std::deque<ExpansionState> queue;
    queue.push_back(std::move(initial));

    while (!queue.empty() && total_paths < options.max_paths) {
      if (output.visited_states >= options.max_visited_states) {
        output.truncated = true;
        output.warnings.push_back("VISIT_BUDGET_EXHAUSTED");
        break;
      }
      ExpansionState current = std::move(queue.front());
      queue.pop_front();
      ++output.visited_states;

      auto current_node = db_.get_node(current.node, output.snapshot_version);
      if (!current_node ||
          !temporal_allowed(current_node->metadata,
                            "node " + std::to_string(current.node))) {
        continue;
      }

      if (current_node->root) {
        RootBundle& root = grouped[current.node];
        root.root_node = current.node;
        if (root.paths.size() < options.max_paths_per_root &&
            path_keys[current.node].insert(current.reverse_edges).second) {
          DialecticPath path;
          path.root_node = current.node;
          path.anchor_node = current.anchor;
          path.nodes.assign(current.reverse_nodes.rbegin(), current.reverse_nodes.rend());
          path.edges.assign(current.reverse_edges.rbegin(), current.reverse_edges.rend());
          path.score = clamp01(current.score);
          path.contains_contradiction = current.contradiction;
          path.contains_hypothetical = current.hypothetical;
          path.temporal_consistent = true;
          path.evidence = std::move(current.evidence);
          path.provenance_findings = std::move(current.findings);
          path.joint_requirements = std::move(current.joint_requirements);
          root.paths.push_back(std::move(path));
          ++total_paths;
        }
        continue;
      }

      if (current.depth >= options.max_hops) continue;
      auto incoming = db_.incoming_edges(current.node, output.snapshot_version);
      std::vector<Edge> ordinary;
      std::map<std::string, std::vector<Edge>> joint_groups;
      for (const auto& edge : incoming) {
        const std::string hyperedge_id =
            metadata_value(edge.metadata, "hyperedge_id");
        if (!hyperedge_id.empty() &&
            metadata_value(edge.metadata, "hyperedge_semantics") ==
                "all_sources") {
          std::string group_id =
              metadata_value(edge.metadata, "hyperedge_group_id");
          if (group_id.empty()) {
            // Compatibility path for manually written metadata. A single
            // member remains incomplete rather than merging across writes.
            group_id = hyperedge_id + ":" +
                       std::to_string(edge.created_version);
          }
          joint_groups[group_id].push_back(edge);
        } else {
          ordinary.push_back(edge);
        }
      }

      auto enqueue_ordinary = [&](const Edge& edge) {
        if (!edge_allowed(edge, options.mode) ||
            !temporal_allowed(edge.metadata,
                              "edge " + std::to_string(edge.id))) {
          return;
        }
        auto source = db_.get_node(edge.from, output.snapshot_version);
        if (!source ||
            !temporal_allowed(source->metadata,
                              "node " + std::to_string(edge.from))) {
          return;
        }
        if (std::find(current.reverse_nodes.begin(), current.reverse_nodes.end(), edge.from) !=
            current.reverse_nodes.end()) {
          return;
        }

        ExpansionState next = current;
        next.node = edge.from;
        next.reverse_nodes.push_back(edge.from);
        next.reverse_edges.push_back(edge.id);
        ++next.depth;
        next.score *= clamp01(edge.confidence);
        next.contradiction =
            next.contradiction || edge.role == EdgeRole::Contradicts;
        next.hypothetical =
            next.hypothetical || edge.origin == EdgeOrigin::Hypothetical;

        DialecticPath assessment;
        append_provenance(edge, &assessment);
        next.evidence.insert(next.evidence.end(),
                             assessment.evidence.begin(),
                             assessment.evidence.end());
        next.findings.insert(next.findings.end(),
                             assessment.provenance_findings.begin(),
                             assessment.provenance_findings.end());
        queue.push_back(std::move(next));
      };
      for (const auto& edge : ordinary) enqueue_ordinary(edge);

      for (const auto& [group_id, members] : joint_groups) {
        const std::string hyperedge_id =
            metadata_value(members.front().metadata, "hyperedge_id");
        std::vector<uint32_t> required_sources;
        bool valid_group = members.size() >= 2 &&
                           parse_u32_list(metadata_value(
                                              members.front().metadata,
                                              "hyperedge_sources"),
                                          &required_sources);
        std::vector<uint32_t> actual_sources;
        std::vector<uint32_t> member_edges;
        double confidence_product = 1.0;
        bool contradiction = false;
        bool hypothetical = false;

        for (const auto& member : members) {
          actual_sources.push_back(member.from);
          member_edges.push_back(member.id);
          if (metadata_value(member.metadata, "hyperedge_id") != hyperedge_id ||
              metadata_value(member.metadata, "hyperedge_semantics") !=
                  "all_sources" ||
              (!metadata_value(member.metadata, "hyperedge_group_id").empty() &&
               metadata_value(member.metadata, "hyperedge_group_id") !=
                   group_id) ||
              metadata_value(member.metadata, "hyperedge_sources") !=
                  metadata_value(members.front().metadata,
                                 "hyperedge_sources") ||
              !edge_allowed(member, options.mode) ||
              !temporal_allowed(member.metadata,
                                "edge " + std::to_string(member.id))) {
            valid_group = false;
          }
          confidence_product *= clamp01(member.confidence);
          contradiction =
              contradiction || member.role == EdgeRole::Contradicts;
          hypothetical =
              hypothetical || member.origin == EdgeOrigin::Hypothetical;
        }
        std::sort(actual_sources.begin(), actual_sources.end());
        std::sort(member_edges.begin(), member_edges.end());
        if (actual_sources != required_sources) valid_group = false;
        const std::string arity_text =
            metadata_value(members.front().metadata, "hyperedge_arity");
        uint32_t arity = 0;
        const auto arity_parse =
            std::from_chars(arity_text.data(),
                            arity_text.data() + arity_text.size(), arity);
        if (arity_text.empty() || arity_parse.ec != std::errc{} ||
            arity_parse.ptr != arity_text.data() + arity_text.size() ||
            arity != required_sources.size()) {
          valid_group = false;
        }

        for (uint32_t source_id : required_sources) {
          auto source = db_.get_node(source_id, output.snapshot_version);
          if (!source ||
              !temporal_allowed(source->metadata,
                                "node " + std::to_string(source_id)) ||
              std::find(current.reverse_nodes.begin(),
                        current.reverse_nodes.end(),
                        source_id) != current.reverse_nodes.end()) {
            valid_group = false;
          }
        }
        if (!valid_group) {
          const std::string warning = "INCOMPLETE_HYPEREDGE: " + hyperedge_id;
          if (std::find(output.warnings.begin(), output.warnings.end(),
                        warning) == output.warnings.end()) {
            output.warnings.push_back(warning);
          }
          continue;
        }

        JointRequirement requirement;
        requirement.hyperedge_id = hyperedge_id;
        requirement.target_node = current.node;
        requirement.source_nodes = required_sources;
        requirement.member_edges = member_edges;
        requirement.all_sources_present = true;
        const double joint_confidence =
            std::pow(confidence_product,
                     1.0 / static_cast<double>(members.size()));

        for (const auto& member : members) {
          ExpansionState next = current;
          next.node = member.from;
          next.reverse_nodes.push_back(member.from);
          next.reverse_edges.insert(next.reverse_edges.end(),
                                    member_edges.begin(), member_edges.end());
          next.joint_requirements.push_back(requirement);
          ++next.depth;
          next.score *= joint_confidence;
          next.contradiction = next.contradiction || contradiction;
          next.hypothetical = next.hypothetical || hypothetical;
          for (const auto& provenance_member : members) {
            DialecticPath assessment;
            append_provenance(provenance_member, &assessment);
            next.evidence.insert(next.evidence.end(),
                                 assessment.evidence.begin(),
                                 assessment.evidence.end());
            next.findings.insert(
                next.findings.end(),
                assessment.provenance_findings.begin(),
                assessment.provenance_findings.end());
          }
          queue.push_back(std::move(next));
        }
      }
    }
  }

  for (auto& entry : grouped) {
    RootBundle root = std::move(entry.second);
    std::set<uint32_t> unique_edges;
    size_t contradictory_paths = 0;
    size_t edge_instances = 0;
    size_t finding_count = 0;
    size_t evidence_covered_edges = 0;
    double score_sum = 0.0;
    for (const auto& path : root.paths) {
      score_sum += path.score;
      if (path.contains_contradiction) ++contradictory_paths;
      unique_edges.insert(path.edges.begin(), path.edges.end());
      edge_instances += path.edges.size();
      finding_count += path.provenance_findings.size();
      std::set<uint32_t> missing_evidence_edges;
      for (const auto& finding : path.provenance_findings) {
        if (finding.code == "MISSING_EVIDENCE") {
          missing_evidence_edges.insert(finding.edge_id);
        }
      }
      evidence_covered_edges +=
          path.edges.size() >= missing_evidence_edges.size()
              ? path.edges.size() - missing_evidence_edges.size()
              : 0;
    }
    root.degeneracy = static_cast<double>(root.paths.size());
    root.diversity =
        root.paths.empty()
            ? 0.0
            : static_cast<double>(unique_edges.size()) /
                  static_cast<double>(root.paths.size());
    root.contradiction_ratio =
        root.paths.empty()
            ? 0.0
            : static_cast<double>(contradictory_paths) /
                  static_cast<double>(root.paths.size());
    root.evidence_coverage =
        edge_instances == 0
            ? 1.0
            : static_cast<double>(evidence_covered_edges) /
                  static_cast<double>(edge_instances);
    root.provenance_risk =
        edge_instances == 0
            ? 0.0
            : clamp01(static_cast<double>(finding_count) /
                      static_cast<double>(edge_instances));
    const double average_score =
        root.paths.empty() ? 0.0 : score_sum / static_cast<double>(root.paths.size());
    root.confidence = clamp01(
        0.60 * average_score +
        0.15 * std::min(1.0, root.degeneracy / 3.0) +
        0.10 * std::min(1.0, root.diversity / 3.0) +
        0.10 * root.evidence_coverage -
        0.20 * root.contradiction_ratio -
        0.15 * root.provenance_risk);
    output.roots.push_back(std::move(root));
  }

  std::sort(output.roots.begin(), output.roots.end(),
            [](const RootBundle& a, const RootBundle& b) {
              if (std::abs(a.confidence - b.confidence) > 1e-12) {
                return a.confidence > b.confidence;
              }
              return a.root_node < b.root_node;
            });
  if (output.roots.empty()) output.warnings.push_back("NO_CAUSAL_ROOT");
  if (total_paths >= options.max_paths) {
    output.truncated = true;
    output.warnings.push_back("PATH_BUDGET_EXHAUSTED");
  }
  return output;
}

ConvergedAnswer DialecticEngine::converge(
    const BundleSet& bundles,
    const DialecticOptions& requested_options) const {
  const DialecticOptions options = bounded_options(requested_options);
  ConvergedAnswer answer;
  if (bundles.roots.empty()) {
    answer.residual_uncertainty.push_back("no causal root reached");
    return answer;
  }

  const RootBundle& primary = bundles.roots.front();
  answer.primary_node = primary.root_node;
  answer.confidence = primary.confidence;
  answer.false_promotion_risk = primary.provenance_risk;

  std::vector<size_t> path_order(primary.paths.size());
  for (size_t index = 0; index < path_order.size(); ++index) path_order[index] = index;
  std::sort(path_order.begin(), path_order.end(), [&](size_t left, size_t right) {
    if (std::abs(primary.paths[left].score - primary.paths[right].score) > 1e-12) {
      return primary.paths[left].score > primary.paths[right].score;
    }
    return primary.paths[left].edges < primary.paths[right].edges;
  });

  std::set<uint32_t> evidence_edges;
  for (size_t rank = 0; rank < path_order.size(); ++rank) {
    const size_t path_index = path_order[rank];
    if (rank < options.max_selected_paths) {
      answer.selected_paths.push_back(
          {primary.root_node, path_index, "selected by bounded convergence"});
      evidence_edges.insert(primary.paths[path_index].edges.begin(),
                            primary.paths[path_index].edges.end());
    } else {
      answer.discarded_paths.push_back(
          {primary.root_node, path_index, "lower-ranked path retained in immutable bundle"});
    }
  }
  for (size_t root_index = 1; root_index < bundles.roots.size(); ++root_index) {
    const auto& root = bundles.roots[root_index];
    for (size_t path_index = 0; path_index < root.paths.size(); ++path_index) {
      answer.discarded_paths.push_back(
          {root.root_node, path_index, "competing causal root retained for opposition"});
    }
  }
  answer.evidence_edges.assign(evidence_edges.begin(), evidence_edges.end());

  if (primary.contradiction_ratio > 0.0) {
    answer.residual_uncertainty.push_back("selected root contains contradictory paths");
  }
  if (primary.provenance_risk > 0.0) {
    answer.residual_uncertainty.push_back("selected root contains provenance risks");
  }
  if (bundles.roots.size() > 1) {
    answer.residual_uncertainty.push_back("competing causal roots remain");
  }
  if (bundles.truncated) {
    answer.residual_uncertainty.push_back("expansion stopped at a configured budget");
  }
  answer.has_answer = answer.confidence >= options.minimum_confidence;
  if (!answer.has_answer) {
    answer.residual_uncertainty.push_back("confidence is below the convergence threshold");
  }
  return answer;
}

OppositionReport DialecticEngine::oppose(
    const BundleSet& bundles,
    const ConvergedAnswer& converged,
    const DialecticOptions& requested_options) const {
  const DialecticOptions options = bounded_options(requested_options);
  OppositionReport report;

  if (!converged.has_answer) {
    append_unique(&report.challenged_claims,
                  "no answer satisfies the convergence threshold");
    append_unique(&report.falsification_questions,
                  "What additional evidence would establish a stable causal root?");
    for (const auto& root : bundles.roots) append_unique(&report.reopen_nodes, root.root_node);
  }

  const RootBundle* primary = find_root(bundles, converged.primary_node);
  if (primary) {
    if (primary->contradiction_ratio > 0.0) {
      append_unique(&report.challenged_claims,
                    "the converged answer contains a contradiction path");
      append_unique(&report.falsification_questions,
                    "Which observation resolves the contradiction affecting the selected root?");
      append_unique(&report.reopen_nodes, primary->root_node);
    }
    if (primary->provenance_risk > 0.0 || primary->evidence_coverage < 1.0) {
      append_unique(&report.challenged_claims,
                    "the converged answer relies on incomplete or unsafe provenance");
      append_unique(&report.falsification_questions,
                    "Can each selected causal edge be tied to evidence or a derivation chain?");
      append_unique(&report.reopen_nodes, primary->root_node);
    }
    if (primary->degeneracy < 2.0) {
      append_unique(&report.challenged_claims,
                    "the selected root is supported by only one retrieved path");
      append_unique(&report.falsification_questions,
                    "Is there an independent path that supports or contradicts this root?");
      append_unique(&report.reopen_nodes, primary->root_node);
    }
  }

  if (bundles.roots.size() > 1) {
    append_unique(&report.challenged_claims,
                  "convergence selected one root while competing roots remain");
    append_unique(&report.falsification_questions,
                  "What evidence discriminates the selected root from its strongest alternative?");
    for (size_t index = 1; index < bundles.roots.size(); ++index) {
      append_unique(&report.reopen_nodes, bundles.roots[index].root_node);
    }
  }
  if (bundles.truncated) {
    append_unique(&report.challenged_claims,
                  "the expansion budget may have hidden additional paths");
    append_unique(&report.falsification_questions,
                  "Does a bounded re-expansion reveal a materially different explanation?");
    if (primary) append_unique(&report.reopen_nodes, primary->root_node);
  }

  report.opposition_score = clamp01(
      0.18 * static_cast<double>(report.challenged_claims.size()) +
      0.04 * static_cast<double>(report.reopen_nodes.size()));
  report.requests_reexpansion =
      report.opposition_score >= options.reexpansion_threshold &&
      !report.reopen_nodes.empty();
  return report;
}

DialecticResult DialecticEngine::reason(
    const std::vector<float>& query,
    uint64_t query_signature,
    const DialecticOptions& requested_options,
    uint64_t snapshot_version) const {
  const DialecticOptions options = bounded_options(requested_options);
  DialecticResult result;
  const uint64_t resolved_snapshot =
      snapshot_version == kInfVersion ? db_.snapshot() : snapshot_version;
  result.initial_bundle =
      expand(query, query_signature, options, resolved_snapshot);
  result.initial_convergence = converge(result.initial_bundle, options);
  result.initial_opposition =
      oppose(result.initial_bundle, result.initial_convergence, options);

  result.final_convergence = result.initial_convergence;
  result.final_opposition = result.initial_opposition;

  DialecticOptions expanded_options = options;
  for (uint32_t round = 0;
       round < options.max_opposition_rounds &&
       result.final_opposition.requests_reexpansion;
       ++round) {
    expanded_options.max_hops =
        std::min<uint32_t>(16, expanded_options.max_hops + 1);
    expanded_options.max_paths =
        std::min<size_t>(256, expanded_options.max_paths * 2);
    expanded_options.max_paths_per_root =
        std::min<size_t>(64, expanded_options.max_paths_per_root * 2);
    expanded_options.max_visited_states =
        std::min<size_t>(200000, expanded_options.max_visited_states * 2);
    expanded_options.semantic_candidates =
        std::min<size_t>(64, expanded_options.semantic_candidates + 4);

    result.reopened_bundle =
        expand(query, query_signature, expanded_options, resolved_snapshot);
    result.has_reopened_bundle = true;
    result.final_convergence =
        converge(result.reopened_bundle, expanded_options);
    result.final_opposition =
        oppose(result.reopened_bundle, result.final_convergence, expanded_options);
    ++result.rounds;
  }

  result.synthesis.has_answer = result.final_convergence.has_answer;
  result.synthesis.primary_node = result.final_convergence.primary_node;
  result.synthesis.confidence = result.final_convergence.confidence;
  result.synthesis.evidence_edges = result.final_convergence.evidence_edges;
  result.synthesis.residual_uncertainty =
      result.final_convergence.residual_uncertainty;

  if (!result.synthesis.has_answer) {
    result.synthesis.epistemic_status = "abstain";
  } else if (result.final_opposition.opposition_score >= 0.50) {
    result.synthesis.epistemic_status = "contested";
  } else if (!result.synthesis.residual_uncertainty.empty() ||
             result.final_opposition.opposition_score > 0.0) {
    result.synthesis.epistemic_status = "provisional";
  } else {
    result.synthesis.epistemic_status = "supported";
  }
  result.durable_writes = false;
  return result;
}

} // namespace graphene
