#include "graphene/hypokosh_runtime.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace graphene;
namespace fs = std::filesystem;

namespace {

// Domain verification comes solely from the runtime-visible certificate on
// ingested evidence. No expected answer or evaluator eligibility is consulted.
class EvidenceCertificateVerifier final : public PathVerifier {
 public:
  explicit EvidenceCertificateVerifier(const GrapheneDB& db) : db_(db) {}
  PathVerificationResult verify(
      const DialecticPath& path,
      const PathVerificationContext& context) const override {
    PathVerificationResult out;
    out.verifier_version = "v3-runtime-evidence-certificate-v1";
    bool saw_evidence = false;
    bool all_verified = true;
    for (uint32_t id : path.edges) {
      const auto edge = db_.get_edge(id, context.snapshot_version);
      if (!edge) { all_verified = false; continue; }
      if (edge->metadata.count("topology_bridge")) continue;
      saw_evidence = true;
      const auto certificate = edge->metadata.find("semantic_verification");
      if (certificate == edge->metadata.end() || certificate->second != "verified")
        all_verified = false;
    }
    if (saw_evidence && all_verified && !path.contains_contradiction)
      out.semantic_verification = SemanticVerificationStatus::Verified;
    return out;
  }
 private:
  const GrapheneDB& db_;
};

struct Observation {
  int step{0};
  std::string id;
  std::string family;
  std::string kind;
  std::string bears_on;
  std::string depends_on;
  std::string revokes;
  std::string supersedes;
  bool semantic_verified{true};
  bool material{false};
  bool independent{true};
};
struct EvidenceRecord {
  uint32_t node_id{0};
  uint32_t target_node{0};
  std::string id;
  std::string family;
  std::string bears_on;
  std::string depends_on;
};
struct HarnessContract {
  std::string target_order{"H1,H2"};
  size_t padding_nodes{0};
  size_t semantic_candidates{32};
  size_t max_paths{128};
  size_t max_visited_states{4096};
};
struct TopologyEdge {
  std::string from_ref;
  std::string to_ref;
  std::string opportunity_class;
};

struct TargetTelemetry {
  uint32_t target_node{0};
  double support_strength{0.0};
  double opposition_strength{0.0};
  double belief_strength{0.0};
  SemanticVerificationStatus verification{
      SemanticVerificationStatus::Unverified};
  size_t independent_support_count{0};
  double best_support_score{0.0};
};

struct StepResult {
  int step{0};
  std::string ingested_evidence_id;
  HypoKoshRuntimeResult result;
  std::vector<std::string> active_evidence_refs;
  bool previous_available{false};
  bool previous_has_answer{false};
  uint32_t previous_operative_node{0};
  uint32_t previous_committed_node{0};
  std::string previous_status{"none"};
};

std::vector<std::string> split(const std::string& value, char delimiter) {
  std::vector<std::string> out;
  std::stringstream stream(value);
  std::string part;
  while (std::getline(stream, part, delimiter)) out.push_back(part);
  return out;
}

std::string json_escape(const std::string& value) {
  std::string out;
  for (char ch : value) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += ch; break;
    }
  }
  return out;
}

template <typename T>
void sort_unique(std::vector<T>* values) {
  std::sort(values->begin(), values->end());
  values->erase(std::unique(values->begin(), values->end()), values->end());
}

void print_u32_array(const std::vector<uint32_t>& values) {
  std::cout << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}

void print_string_array(const std::vector<std::string>& values) {
  std::cout << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << '"' << json_escape(values[i]) << '"';
  }
  std::cout << ']';
}

NodeInput make_node(std::string content,
                    std::vector<float> vector,
                    uint64_t signature,
                    bool root) {
  NodeInput input;
  input.content = std::move(content);
  input.vector = std::move(vector);
  input.signature = signature;
  input.root = root;
  input.incident = 7001;
  input.metadata["source"] = "epistemic-process-v3-cycle5-runner";
  return input;
}

std::vector<Observation> load_observations(const fs::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open sanitized observation file");
  std::vector<Observation> out;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    auto fields = split(line, '\t');
    while (fields.size() < 11 && !line.empty() && line.back() == '\t') {
      fields.emplace_back();
      line.pop_back();
    }
    if (fields.size() != 11) {
      throw std::runtime_error("V3 sanitized observation row must contain 11 fields");
    }
    Observation observation;
    observation.step = std::stoi(fields[0]);
    observation.id = fields[1];
    observation.family = fields[2];
    observation.kind = fields[3];
    observation.bears_on = fields[4];
    observation.depends_on = fields[5];
    observation.revokes = fields[6];
    observation.supersedes = fields[7];
    observation.semantic_verified = fields[8] == "1";
    observation.material = fields[9] == "1";
    observation.independent = fields[10] == "1";
    out.push_back(std::move(observation));
  }
  return out;
}

HarnessContract load_contract(const fs::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open V3 harness contract");
  std::string line;
  if (!std::getline(input, line)) {
    throw std::runtime_error("empty V3 harness contract");
  }
  const auto fields = split(line, '\t');
  if (fields.size() != 5) {
    throw std::runtime_error("V3 harness contract must contain 5 fields");
  }
  HarnessContract contract;
  contract.target_order = fields[0];
  contract.padding_nodes = static_cast<size_t>(std::stoull(fields[1]));
  contract.semantic_candidates = static_cast<size_t>(std::stoull(fields[2]));
  contract.max_paths = static_cast<size_t>(std::stoull(fields[3]));
  contract.max_visited_states = static_cast<size_t>(std::stoull(fields[4]));
  return contract;
}

std::vector<TopologyEdge> load_topology(const fs::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open V3 topology file");
  std::vector<TopologyEdge> out;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    const auto fields = split(line, '\t');
    if (fields.size() != 3) {
      throw std::runtime_error("V3 topology row must contain 3 fields");
    }
    out.push_back({fields[0], fields[1], fields[2]});
  }
  return out;
}

void require(Status status, const std::string& operation) {
  if (!status) throw std::runtime_error(operation + ": " + status.message);
}

double candidate_score(const FiberPath& path) {
  const double raw =
      path.confidence * path.query_relevance * path.target_consistency *
      path.completeness * path.provenance_quality;
  return std::max(0.0, std::min(1.0, raw));
}

double independent_union_strength(const std::vector<double>& scores) {
  double residual = 1.0;
  for (double score : scores) {
    residual *= 1.0 - std::max(0.0, std::min(1.0, score));
  }
  return std::max(0.0, std::min(1.0, 1.0 - residual));
}

SemanticVerificationStatus strongest_verification(
    const std::vector<const FiberPath*>& paths) {
  bool verified = false;
  bool contradicted = false;
  bool not_applicable = false;
  for (const FiberPath* path : paths) {
    switch (path->semantic_verification) {
      case SemanticVerificationStatus::Verified: verified = true; break;
      case SemanticVerificationStatus::Contradicted: contradicted = true; break;
      case SemanticVerificationStatus::NotApplicable: not_applicable = true; break;
      case SemanticVerificationStatus::Unverified: break;
    }
  }
  if (contradicted) return SemanticVerificationStatus::Contradicted;
  if (verified) return SemanticVerificationStatus::Verified;
  if (not_applicable) return SemanticVerificationStatus::NotApplicable;
  return SemanticVerificationStatus::Unverified;
}

int verification_rank(SemanticVerificationStatus status) {
  switch (status) {
    case SemanticVerificationStatus::Verified: return 3;
    case SemanticVerificationStatus::NotApplicable: return 2;
    case SemanticVerificationStatus::Unverified: return 1;
    case SemanticVerificationStatus::Contradicted: return 0;
  }
  return 0;
}

const char* verification_name(SemanticVerificationStatus status) {
  switch (status) {
    case SemanticVerificationStatus::Verified: return "verified";
    case SemanticVerificationStatus::NotApplicable: return "not_applicable";
    case SemanticVerificationStatus::Unverified: return "unverified";
    case SemanticVerificationStatus::Contradicted: return "contradicted";
  }
  return "unverified";
}

std::vector<TargetTelemetry> target_ranking(const FiberBundle& bundle) {
  std::vector<TargetTelemetry> output;
  for (const auto& fiber : bundle.fibers) {
    std::set<uint64_t> support_representatives;
    std::set<uint64_t> opposition_representatives;
    for (const auto& group : fiber.correlation_groups) {
      if (group.role == FiberPathRole::Support && group.independent_support) {
        support_representatives.insert(group.representative_path_id);
      } else if (group.role == FiberPathRole::Opposition) {
        opposition_representatives.insert(group.representative_path_id);
      }
    }

    std::vector<double> support_scores;
    std::vector<double> opposition_scores;
    std::vector<const FiberPath*> support_paths;
    double best_support = 0.0;
    for (const auto& path : fiber.paths) {
      if (support_representatives.count(path.id) && path.eligible_for_support) {
        const double score = candidate_score(path);
        support_scores.push_back(score);
        support_paths.push_back(&path);
        best_support = std::max(best_support, score);
      }
      if (opposition_representatives.count(path.id) &&
          path.eligible_for_opposition) {
        opposition_scores.push_back(candidate_score(path));
      }
    }
    if (support_scores.empty()) continue;

    TargetTelemetry target;
    target.target_node = fiber.target_node;
    target.support_strength = independent_union_strength(support_scores);
    target.opposition_strength = independent_union_strength(opposition_scores);
    target.belief_strength = std::max(
        0.0,
        std::min(
            1.0,
            target.support_strength * (1.0 - target.opposition_strength)));
    target.verification = strongest_verification(support_paths);
    target.independent_support_count =
        fiber.independent_evidence_family_count;
    target.best_support_score = best_support;
    output.push_back(target);
  }

  std::sort(
      output.begin(),
      output.end(),
      [](const TargetTelemetry& left, const TargetTelemetry& right) {
        if (left.belief_strength != right.belief_strength)
          return left.belief_strength > right.belief_strength;
        const int left_verification = verification_rank(left.verification);
        const int right_verification = verification_rank(right.verification);
        if (left_verification != right_verification)
          return left_verification > right_verification;
        if (left.independent_support_count != right.independent_support_count)
          return left.independent_support_count >
                 right.independent_support_count;
        if (left.best_support_score != right.best_support_score)
          return left.best_support_score > right.best_support_score;
        return left.target_node < right.target_node;
      });
  return output;
}

void print_target_ranking(const FiberBundle& bundle) {
  const auto ranking = target_ranking(bundle);
  std::cout << '[';
  for (size_t i = 0; i < ranking.size(); ++i) {
    if (i) std::cout << ',';
    const auto& item = ranking[i];
    std::cout
        << "{\"target_id\":" << item.target_node
        << ",\"rank\":" << (i + 1)
        << ",\"support_strength\":" << item.support_strength
        << ",\"opposition_strength\":" << item.opposition_strength
        << ",\"belief_strength\":" << item.belief_strength
        << ",\"semantic_verification\":\""
        << verification_name(item.verification)
        << "\",\"independent_support_family_count\":"
        << item.independent_support_count << '}';
  }
  std::cout << ']';
}

void print_target_trace_ranking(
    const std::vector<TargetEpistemicTrace>& ranking) {
  std::cout << '[';
  for (size_t i = 0; i < ranking.size(); ++i) {
    if (i) std::cout << ',';
    const auto& item = ranking[i];
    std::cout
        << "{\"target_id\":" << item.target_node
        << ",\"rank\":" << (i + 1)
        << ",\"support_strength\":" << item.support_strength
        << ",\"opposition_strength\":" << item.opposition_strength
        << ",\"belief_strength\":" << item.belief_strength
        << ",\"semantic_verification\":\""
        << verification_name(item.semantic_verification)
        << "\",\"independent_support_family_count\":"
        << item.independent_support_family_count
        << ",\"best_support_score\":" << item.best_support_score
        << '}';
  }
  std::cout << ']';
}

bool target_trace_ranking_changed(
    const std::vector<TargetEpistemicTrace>& left,
    const std::vector<TargetEpistemicTrace>& right) {
  if (left.size() != right.size()) return true;
  for (size_t i = 0; i < left.size(); ++i) {
    if (left[i].target_node != right[i].target_node ||
        left[i].support_strength != right[i].support_strength ||
        left[i].opposition_strength != right[i].opposition_strength ||
        left[i].belief_strength != right[i].belief_strength ||
        left[i].semantic_verification != right[i].semantic_verification ||
        left[i].independent_support_family_count !=
            right[i].independent_support_family_count ||
        left[i].best_support_score != right[i].best_support_score) {
      return true;
    }
  }
  return false;
}

std::vector<uint32_t> bundle_edges(const FiberBundle& bundle) {
  std::vector<uint32_t> edges;
  for (const auto& fiber : bundle.fibers) {
    for (const auto& path : fiber.paths) {
      edges.insert(edges.end(), path.edges.begin(), path.edges.end());
    }
  }
  sort_unique(&edges);
  return edges;
}

std::vector<std::string> bundle_families(const FiberBundle& bundle) {
  std::vector<std::string> families;
  for (const auto& fiber : bundle.fibers) {
    for (const auto& path : fiber.paths) {
      families.insert(
          families.end(),
          path.evidence_family_lineage.begin(),
          path.evidence_family_lineage.end());
    }
  }
  sort_unique(&families);
  return families;
}

std::vector<std::string> bundle_derivations(const FiberBundle& bundle) {
  std::vector<std::string> derivations;
  for (const auto& fiber : bundle.fibers) {
    for (const auto& path : fiber.paths) {
      derivations.insert(
          derivations.end(),
          path.derivation_lineage.begin(),
          path.derivation_lineage.end());
    }
  }
  sort_unique(&derivations);
  return derivations;
}

std::vector<std::string> refs_for_edges(
    const std::vector<uint32_t>& edges,
    const std::map<uint32_t, std::string>& edge_to_ref) {
  std::vector<std::string> refs;
  for (uint32_t edge : edges) {
    const auto it = edge_to_ref.find(edge);
    if (it != edge_to_ref.end()) refs.push_back(it->second);
  }
  sort_unique(&refs);
  return refs;
}

bool status_commits(const std::string& status) {
  return status == "resolved" || status == "provisionally_resolved";
}

void print_state(
    const FiberBundle& bundle,
    const ConvergedAnswer* convergence,
    const std::string& status,
    bool final_state,
    bool evidence_only) {
  const bool has_answer =
      !evidence_only && convergence != nullptr && convergence->has_answer;
  const uint32_t operative =
      has_answer ? convergence->primary_node : 0;
  const bool commits = final_state && has_answer && status_commits(status);
  const uint32_t committed = commits ? operative : 0;
  const double confidence =
      has_answer && convergence != nullptr ? convergence->confidence : 0.0;
  const std::vector<uint32_t> edges =
      has_answer && convergence != nullptr
          ? convergence->evidence_edges
          : bundle_edges(bundle);
  const std::vector<std::string> families = bundle_families(bundle);
  const std::vector<std::string> derivations =
      bundle_derivations(bundle);

  std::cout
      << "{\"has_answer\":" << (has_answer ? "true" : "false")
      << ",\"operative_hypothesis_node\":" << operative
      << ",\"committed_answer_node\":" << committed
      << ",\"epistemic_status\":\"" << json_escape(status)
      << "\",\"confidence\":" << confidence
      << ",\"bundle_hash\":" << bundle.immutable_hash
      << ",\"visited_states\":" << bundle.visited_states
      << ",\"truncated\":" << (bundle.truncated ? "true" : "false")
      << ",\"evidence_edge_ids\":";
  print_u32_array(edges);
  std::cout << ",\"evidence_family_ids\":";
  print_string_array(families);
  std::cout << ",\"dependency_lineage_ids\":";
  print_string_array(derivations);
  std::cout << ",\"target_ranking\":";
  print_target_ranking(bundle);
  std::cout << '}';
}

void print_native_event(const NativeEpistemicEvent& event) {
  std::cout << "{\"source\":\""
            << epistemic_event_source_name(event.source)
            << "\",\"type\":\""
            << epistemic_event_type_name(event.type)
            << "\",\"previous_hypothesis_node\":"
            << event.previous_hypothesis_node
            << ",\"hypothesis_node\":" << event.hypothesis_node
            << ",\"competing_hypotheses\":";
  print_u32_array(event.competing_hypotheses);
  std::cout << ",\"reopen_nodes\":";
  print_u32_array(event.reopen_nodes);
  std::cout << ",\"evidence_edges\":";
  print_u32_array(event.evidence_edges);
  std::cout << ",\"evidence_family_ids\":";
  print_string_array(event.evidence_family_ids);
  std::cout << ",\"epistemic_state\":\""
            << json_escape(event.epistemic_state)
            << "\",\"reason\":\"" << json_escape(event.reason) << "\"}";
}

void print_options(
    uint32_t max_hops,
    size_t semantic_candidates,
    size_t max_paths,
    size_t max_paths_per_root,
    size_t max_visited_states,
    double minimum_confidence,
    size_t reopen_nodes) {
  std::cout
      << "{\"max_hops\":" << max_hops
      << ",\"semantic_candidates\":" << semantic_candidates
      << ",\"max_paths\":" << max_paths
      << ",\"max_paths_per_root\":" << max_paths_per_root
      << ",\"max_visited_states\":" << max_visited_states
      << ",\"minimum_confidence\":" << minimum_confidence
      << ",\"reopen_node_count\":" << reopen_nodes << '}';
}

void print_recovery_rounds(const HypoKoshRuntimeResult& result) {
  const auto& traces = result.receipt.recovery_trace;
  std::cout << '[';
  for (size_t i = 0; i < traces.size(); ++i) {
    if (i) std::cout << ',';
    const auto& trace = traces[i];
    const bool operative_changed =
        trace.previous_has_answer != trace.next_has_answer ||
        (trace.previous_has_answer && trace.next_has_answer &&
         trace.previous_primary_node != trace.next_primary_node);
    const bool rank_changed = target_trace_ranking_changed(
        trace.previous_target_ranking, trace.next_target_ranking);

    std::cout
        << "{\"round_index\":" << trace.round_index
        << ",\"trigger\":\""
        << (trace.opposition_search_requested
                ? "dwm_opposition"
                : (trace.recovery_search_requested ? "recovery" : "none"))
        << "\",\"dialectical_challenge_present\":"
        << (trace.dialectical_challenge_present ? "true" : "false")
        << ",\"corroboration_search_requested\":"
        << (trace.corroboration_search_requested ? "true" : "false")
        << ",\"expansion_opportunity_available\":"
        << (trace.expansion_opportunity_available ? "true" : "false")
        << ",\"expansion_opportunity_class\":\""
        << json_escape(trace.expansion_opportunity_class)
        << "\",\"reopen_node_count_before\":"
        << trace.previous_reopen_nodes
        << ",\"reopen_node_count_after\":" << trace.next_reopen_nodes
        << ",\"options_before\":";
    print_options(
        trace.previous_max_hops,
        trace.previous_semantic_candidates,
        trace.previous_max_paths,
        trace.previous_max_paths_per_root,
        trace.previous_max_visited_states,
        trace.previous_minimum_confidence,
        trace.previous_reopen_nodes);
    std::cout << ",\"options_after\":";
    print_options(
        trace.next_max_hops,
        trace.next_semantic_candidates,
        trace.next_max_paths,
        trace.next_max_paths_per_root,
        trace.next_max_visited_states,
        trace.next_minimum_confidence,
        trace.next_reopen_nodes);
    std::cout
        << ",\"bundle_hash_before\":" << trace.previous_bundle_hash
        << ",\"bundle_hash_after\":" << trace.next_bundle_hash
        << ",\"visited_states_before\":" << trace.previous_visited_states
        << ",\"visited_states_after\":" << trace.next_visited_states
        << ",\"frontier_changed\":"
        << (trace.bundle_changed || trace.frontier_progress ? "true" : "false")
        << ",\"has_answer_before\":"
        << (trace.previous_has_answer ? "true" : "false")
        << ",\"has_answer_after\":"
        << (trace.next_has_answer ? "true" : "false")
        << ",\"operative_hypothesis_node_before\":"
        << trace.previous_primary_node
        << ",\"operative_hypothesis_node_after\":"
        << trace.next_primary_node
        << ",\"committed_answer_node_before\":"
        << trace.previous_committed_node
        << ",\"committed_answer_node_after\":"
        << trace.next_committed_node
        << ",\"status_before\":\""
        << governed_status_name(trace.previous_status)
        << "\",\"status_after\":\""
        << governed_status_name(trace.next_status)
        << "\",\"target_ranking_before\":";
    print_target_trace_ranking(trace.previous_target_ranking);
    std::cout << ",\"target_ranking_after\":";
    print_target_trace_ranking(trace.next_target_ranking);
    std::cout
        << ",\"rank_changed\":"
        << (rank_changed ? "true" : "false")
        << ",\"operative_hypothesis_changed\":"
        << (operative_changed ? "true" : "false")
        << ",\"status_changed\":"
        << (trace.previous_status != trace.next_status ? "true" : "false")
        << ",\"committed_answer_changed\":"
        << (trace.previous_committed_node != trace.next_committed_node
                ? "true" : "false")
        << ",\"stop_reason\":\""
        << json_escape(trace.stop_reason) << "\"}";
  }
  std::cout << ']';
}

void print_previous_state(const StepResult& step) {
  std::cout
      << "{\"has_answer\":"
      << (step.previous_has_answer ? "true" : "false")
      << ",\"operative_hypothesis_node\":"
      << step.previous_operative_node
      << ",\"committed_answer_node\":"
      << step.previous_committed_node
      << ",\"epistemic_status\":\""
      << json_escape(step.previous_status)
      << "\",\"confidence\":null"
      << ",\"bundle_hash\":null"
      << ",\"visited_states\":null"
      << ",\"truncated\":null"
      << ",\"evidence_edge_ids\":[]"
      << ",\"evidence_family_ids\":[]"
      << ",\"dependency_lineage_ids\":[]"
      << ",\"target_ranking\":[]}";
}

uint32_t final_operative(const HypoKoshRuntimeResult& result, bool evidence_only) {
  if (evidence_only || !result.final_convergence.has_answer) return 0;
  return result.final_convergence.primary_node;
}

uint32_t final_committed(const HypoKoshRuntimeResult& result, bool evidence_only) {
  if (evidence_only || !result.final_convergence.has_answer) return 0;
  const std::string status = governed_status_name(result.status);
  return status_commits(status) ? result.final_convergence.primary_node : 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 9) {
    std::cerr
        << "usage: epistemic_process_runtime_runner_v3_cycle5 "
        << "<G0E|G1|G2> <episode-id> <visible-tsv> <latent-tsv> "
        << "<topology-tsv> <contract-tsv> <db-dir> <seed>\n";
    return 2;
  }

  try {
    const std::string configuration = argv[1];
    const std::string episode_id = argv[2];
    const fs::path observations_path = argv[3];
    const fs::path latent_path = argv[4];
    const fs::path topology_path = argv[5];
    const fs::path contract_path = argv[6];
    const fs::path db_dir = argv[7];
    const uint64_t seed = std::stoull(argv[8]);
    (void)seed;

    if (configuration != "G0E" &&
        configuration != "G1" &&
        configuration != "G2") {
      throw std::runtime_error("configuration must be G0E, G1 or G2");
    }

    const bool evidence_only = configuration == "G0E";
    const auto observations = load_observations(observations_path);
    const auto latent_observations = load_observations(latent_path);
    const auto topology_edges = load_topology(topology_path);
    const HarnessContract contract = load_contract(contract_path);
    fs::remove_all(db_dir);

    GrapheneDB db;
    DBOptions db_options;
    db_options.dimension = 3;
    db_options.fsync_on_commit = false;
    require(db.open(db_dir, db_options), "open database");

    const uint64_t signature = signature_for(21, 34);
    // Runtime transition receipts reserve zero as null. The DB allocator may
    // produce node zero, so reserve it before mapping logical task targets.
    uint32_t sentinel = 0;
    require(db.put_node(make_node("reserved-null-sentinel",
                                 {0.0f, -1.0f, 0.0f}, signature, false),
                        &sentinel), "reserve null sentinel");
    for (size_t i = 0; i < contract.padding_nodes; ++i) {
      uint32_t ignored = 0;
      require(
          db.put_node(
              make_node("padding-" + std::to_string(i),
                        {0.0f, -1.0f, 0.0f}, signature, false),
              &ignored),
          "put padding node");
    }
    uint32_t h1 = 0;
    uint32_t h2 = 0;
    const auto order = split(contract.target_order, ',');
    if (order.size() != 2 || order[0] == order[1]) {
      throw std::runtime_error("invalid V3 target insertion order");
    }
    for (const std::string& target_name : order) {
      uint32_t* target_id = target_name == "H1" ? &h1 : &h2;
      const std::vector<float> vector =
          target_name == "H1"
              ? std::vector<float>{1.0f, 0.0f, 0.0f}
              : std::vector<float>{-1.0f, 0.0f, 0.0f};
      require(
          db.put_node(make_node(target_name, vector, signature, true), target_id),
          "put " + target_name);
    }

    RuntimeOptions options;
    options.dialectic.mode = QueryMode::Empirical;
    options.dialectic.semantic_candidates = contract.semantic_candidates;
    options.dialectic.max_hops = 2;
    options.dialectic.max_paths = contract.max_paths;
    options.dialectic.max_paths_per_root =
        std::min<size_t>(64, std::max<size_t>(1, contract.max_paths));
    options.dialectic.max_visited_states = contract.max_visited_states;
    options.dialectic.minimum_confidence = 0.10;
    options.dialectic.reexpansion_threshold = 0.25;
    options.max_recursive_cycles = 2;
    options.update_model_world = false;
    options.enable_hypokosh = !evidence_only;
    options.enable_dwm = configuration == "G2";
    options.enable_opposition_research = configuration == "G2";
    EvidenceCertificateVerifier certificate_verifier(db);
    options.path_verifier = &certificate_verifier;

    CompleteHypoKoshRuntime runtime(db);
    std::map<uint32_t, std::string> edge_to_ref;
    std::map<std::string, EvidenceRecord> evidence_by_ref;
    std::vector<StepResult> steps;

    auto insert_observation =
        [&](const Observation& observation,
            const std::vector<float>& vector,
            const std::string& lifecycle) {
          uint32_t evidence_node = 0;
          NodeInput evidence = make_node(
              observation.id + " " + observation.kind + " " +
                  observation.bears_on,
              vector, signature, false);
          evidence.metadata["event_id"] = observation.id;
          evidence.metadata["family"] = observation.family;
          require(db.put_node(evidence, &evidence_node),
                  "put evidence " + observation.id);

          const uint32_t target =
              observation.bears_on == "H2" ? h2 : h1;
          EdgeInput edge;
          edge.from = target;
          edge.to = evidence_node;
          edge.origin = EdgeOrigin::Observed;
          edge.role =
              observation.kind == "refute"
                  ? EdgeRole::Contradicts
                  : EdgeRole::Supports;
          edge.confidence =
              observation.kind == "refute" ? 0.95 : 0.90;
          edge.metadata["source_id"] = observation.id;
          edge.metadata["evidence_family_id"] = observation.family;
          edge.metadata["event_kind"] = observation.kind;
          edge.metadata["evidence_state"] = lifecycle;
          if (!observation.depends_on.empty()) {
            edge.metadata["derivation_id"] = observation.depends_on;
          }
          if (observation.semantic_verified) {
            edge.metadata["semantic_verification"] = "verified";
          }
          if (observation.material) {
            edge.metadata["material"] = "true";
          }
          edge.metadata["independent"] =
              observation.independent ? "true" : "false";
          uint32_t edge_id = 0;
          require(db.put_edge(edge, &edge_id), "put edge " + observation.id);
          edge_to_ref[edge_id] = observation.id;
          evidence_by_ref[observation.id] = {
              evidence_node, target, observation.id, observation.family,
              observation.bears_on, observation.depends_on};
        };

    auto retire_evidence =
        [&](const std::string& ref, const std::string& lifecycle) {
          const auto it = evidence_by_ref.find(ref);
          if (it == evidence_by_ref.end()) {
            throw std::runtime_error(
                lifecycle + " references unknown evidence " + ref);
          }
          const EvidenceRecord prior = it->second;
          require(db.delete_node(prior.node_id),
                  lifecycle + " evidence " + ref);

          Observation audit;
          audit.step = 0;
          audit.id = prior.id + "__audit_" + lifecycle;
          audit.family = prior.family;
          audit.kind = "support";
          audit.bears_on = prior.bears_on;
          audit.depends_on = prior.depends_on;
          audit.semantic_verified = true;
          audit.independent = false;
          insert_observation(
              audit, {0.0f, 0.95f, 0.0f}, lifecycle);
        };

    // Harness-only latent evidence exists in the graph from the beginning but
    // has deliberately poor query similarity. It becomes a first-class
    // candidate only when a DWM reopen seeds the challenged target's
    // downstream observations.
    for (const Observation& latent : latent_observations) {
      insert_observation(latent, {0.0f, -1.0f, 0.0f}, "active");
    }

    auto materialize_topology_from =
        [&](const std::string& from_ref) {
          const auto from_it = evidence_by_ref.find(from_ref);
          if (from_it == evidence_by_ref.end()) return;
          for (const TopologyEdge& topology : topology_edges) {
            if (topology.from_ref != from_ref) continue;
            const auto to_it = evidence_by_ref.find(topology.to_ref);
            if (to_it == evidence_by_ref.end()) {
              throw std::runtime_error(
                  "topology references unknown latent evidence " +
                  topology.to_ref);
            }
            EdgeInput bridge;
            bridge.from = from_it->second.node_id;
            bridge.to = to_it->second.node_id;
            bridge.origin = EdgeOrigin::Observed;
            bridge.role = EdgeRole::Mechanistic;
            bridge.confidence = 0.90;
            bridge.metadata["topology_bridge"] = "true";
            bridge.metadata["expansion_opportunity_class"] =
                topology.opportunity_class;
            uint32_t ignored_edge = 0;
            require(db.put_edge(bridge, &ignored_edge),
                    "put V3 topology bridge");
          }
        };

    bool previous_available = false;
    bool previous_has_answer = false;
    uint32_t previous_operative = 0;
    uint32_t previous_committed = 0;
    std::string previous_status = "none";
    uint32_t last_committed = 0;

    for (const Observation& observation : observations) {
      if (observation.kind == "revoke" && !observation.revokes.empty()) {
        for (const std::string& ref : split(observation.revokes, ',')) {
          if (!ref.empty()) retire_evidence(ref, "revoked");
        }
      }
      if (observation.kind == "supersede" &&
          !observation.supersedes.empty()) {
        for (const std::string& ref : split(observation.supersedes, ',')) {
          if (!ref.empty()) retire_evidence(ref, "superseded");
        }
      }

      const std::string lifecycle =
          observation.kind == "revoke" ? "audit_only" : "active";
      insert_observation(
          observation, {0.0f, 1.0f, 0.0f}, lifecycle);
      materialize_topology_from(observation.id);

      StepResult step;
      step.step = observation.step;
      step.ingested_evidence_id = observation.id;
      step.previous_available = previous_available;
      step.previous_has_answer = previous_has_answer;
      step.previous_operative_node = previous_operative;
      step.previous_committed_node = previous_committed;
      step.previous_status = previous_status;
      step.result = runtime.reason(
          {0.0f, 1.0f, 0.0f}, signature, options);

      step.active_evidence_refs =
          refs_for_edges(bundle_edges(step.result.final_bundle), edge_to_ref);
      steps.push_back(std::move(step));

      const auto& current = steps.back().result;
      previous_available = true;
      previous_has_answer =
          !evidence_only && current.final_convergence.has_answer;
      previous_operative = final_operative(current, evidence_only);
      previous_committed = final_committed(current, evidence_only);
      previous_status =
          evidence_only ? "evidence_only" : governed_status_name(current.status);
      if (!evidence_only) {
        if (previous_committed != 0) last_committed = previous_committed;
        auto& prior = options.prior_epistemic_state;
        prior.available = true;
        prior.has_answer = previous_has_answer;
        prior.operative_node = previous_operative;
        prior.committed_node = previous_committed;
        prior.last_committed_node = last_committed;
        prior.status = current.status;
        prior.bundle_hash = current.final_bundle.immutable_hash;
        prior.stability = current.final_stability;
      }
    }

    std::cout
        << "{\"schema\":\"epistemic-process-v3-cycle5-runtime-telemetry-v1\""
        << ",\"episode_id\":\"" << json_escape(episode_id)
        << "\",\"configuration\":\"" << json_escape(configuration)
        << "\",\"score_bearing\":false"
        << ",\"hypothesis_nodes\":{\"H1\":" << h1
        << ",\"H2\":" << h2 << "}"
        << ",\"harness\":{\"latent_observations\":"
        << latent_observations.size()
        << ",\"topology_edges\":" << topology_edges.size()
        << ",\"semantic_candidates\":"
        << contract.semantic_candidates
        << ",\"max_paths\":" << contract.max_paths
        << ",\"max_visited_states\":"
        << contract.max_visited_states
        << "}"
        << ",\"steps\":[";

    for (size_t i = 0; i < steps.size(); ++i) {
      if (i) std::cout << ',';
      const StepResult& step = steps[i];
      const auto& result = step.result;
      const ConvergedAnswer* initial =
          evidence_only ? nullptr : &result.initial_convergence;
      const ConvergedAnswer* final =
          evidence_only ? nullptr : &result.final_convergence;
      const std::string initial_status =
          evidence_only
              ? "evidence_only"
              : (result.initial_convergence.has_answer
                     ? "operative_selected"
                     : "abstain");
      const std::string final_status =
          evidence_only ? "evidence_only"
                        : governed_status_name(result.status);

      const uint32_t current_initial =
          initial && initial->has_answer ? initial->primary_node : 0;
      const bool cross_step_changed =
          step.previous_available &&
          (step.previous_operative_node != current_initial);

      std::cout
          << "{\"step\":" << step.step
          << ",\"ingested_evidence_ids\":[\""
          << json_escape(step.ingested_evidence_id) << "\"]"
          << ",\"previous_step_state\":";
      print_previous_state(step);
      std::cout << ",\"initial_state\":";
      print_state(
          result.initial_bundle,
          initial,
          initial_status,
          false,
          evidence_only);
      std::cout << ",\"recovery_rounds\":";
      print_recovery_rounds(result);
      std::cout << ",\"final_state\":";
      print_state(
          result.final_bundle,
          final,
          final_status,
          true,
          evidence_only);
      std::cout << ",\"native_events\":[";

      bool emitted = false;
      if (cross_step_changed) {
        std::cout
            << "{\"source\":\"cycle4_runner\""
            << ",\"type\":\"cross_step_belief_change\""
            << ",\"from_node\":" << step.previous_operative_node
            << ",\"to_node\":" << current_initial
            << ",\"triggering_evidence_ids\":[\""
            << json_escape(step.ingested_evidence_id)
            << "\"],\"step\":" << step.step << '}';
        emitted = true;
      }
      for (const auto& event : result.receipt.epistemic_events) {
        if (emitted) std::cout << ',';
        print_native_event(event);
        emitted = true;
      }
      std::cout << "]"
                << ",\"active_evidence_refs\":";
      print_string_array(step.active_evidence_refs);

      const std::vector<std::string> gaps;
      std::cout << ",\"telemetry_gaps\":";
      print_string_array(gaps);

      std::cout
          << ",\"execution\":{"
          << "\"execution_seconds\":null"
          << ",\"expansion_rounds\":" << result.receipt.expansion_rounds
          << ",\"visited_states_total\":" << result.final_bundle.visited_states
          << ",\"evidence_edges_considered\":"
          << bundle_edges(result.final_bundle).size()
          << ",\"runtime_failure\":false}"
          << '}';
    }

    std::cout << "],\"failures\":[]}"
              << std::endl;

    require(db.close(), "close database");
    fs::remove_all(db_dir);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "cycle5_runtime_runner_error=" << error.what() << "\n";
    return 1;
  }
}
