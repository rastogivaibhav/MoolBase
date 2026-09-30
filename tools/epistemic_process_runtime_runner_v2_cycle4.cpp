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

struct Observation {
  int step{0};
  std::string id;
  std::string family;
  std::string kind;
  std::string bears_on;
  std::string depends_on;
  std::string revokes;
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
  input.metadata["source"] = "epistemic-process-v2-cycle4-runner";
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
    while (fields.size() < 7 && !line.empty() && line.back() == '\t') {
      fields.emplace_back();
      line.pop_back();
    }
    if (fields.size() != 7) {
      throw std::runtime_error("sanitized observation row must contain 7 fields");
    }
    Observation observation;
    observation.step = std::stoi(fields[0]);
    observation.id = fields[1];
    observation.family = fields[2];
    observation.kind = fields[3];
    observation.bears_on = fields[4];
    observation.depends_on = fields[5];
    observation.revokes = fields[6];
    out.push_back(std::move(observation));
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
    const bool before_ranking_available = i == 0;
    const bool after_ranking_available = i + 1 == traces.size();

    std::cout
        << "{\"round_index\":" << trace.round_index
        << ",\"trigger\":\""
        << (trace.opposition_search_requested
                ? "dwm_opposition"
                : (trace.recovery_search_requested ? "recovery" : "none"))
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
        << ",\"target_ranking_before\":";
    if (before_ranking_available) {
      print_target_ranking(result.initial_bundle);
    } else {
      std::cout << "null";
    }
    std::cout << ",\"target_ranking_after\":";
    if (after_ranking_available) {
      print_target_ranking(result.final_bundle);
    } else {
      std::cout << "null";
    }
    std::cout
        << ",\"rank_changed\":null"
        << ",\"operative_hypothesis_changed\":null"
        << ",\"status_changed\":null"
        << ",\"committed_answer_changed\":null"
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
  if (argc != 6) {
    std::cerr
        << "usage: epistemic_process_runtime_runner_v2_cycle4 "
        << "<G0E|G1|G2> <episode-id> <sanitized-tsv> <db-dir> <seed>\n";
    return 2;
  }

  try {
    const std::string configuration = argv[1];
    const std::string episode_id = argv[2];
    const fs::path observations_path = argv[3];
    const fs::path db_dir = argv[4];
    const uint64_t seed = std::stoull(argv[5]);
    (void)seed;

    if (configuration != "G0E" &&
        configuration != "G1" &&
        configuration != "G2") {
      throw std::runtime_error("configuration must be G0E, G1 or G2");
    }

    const bool evidence_only = configuration == "G0E";
    const auto observations = load_observations(observations_path);
    fs::remove_all(db_dir);

    GrapheneDB db;
    DBOptions db_options;
    db_options.dimension = 3;
    db_options.fsync_on_commit = false;
    require(db.open(db_dir, db_options), "open database");

    const uint64_t signature = signature_for(21, 34);
    uint32_t h1 = 0;
    uint32_t h2 = 0;
    require(
        db.put_node(make_node("H1", {1.0f, 0.0f, 0.0f}, signature, true), &h1),
        "put H1");
    require(
        db.put_node(make_node("H2", {-1.0f, 0.0f, 0.0f}, signature, true), &h2),
        "put H2");

    RuntimeOptions options;
    options.dialectic.mode = QueryMode::Empirical;
    options.dialectic.semantic_candidates = 32;
    options.dialectic.max_hops = 2;
    options.dialectic.max_paths = 128;
    options.dialectic.max_paths_per_root = 32;
    options.dialectic.max_visited_states = 4096;
    options.dialectic.minimum_confidence = 0.10;
    options.dialectic.reexpansion_threshold = 0.25;
    options.max_recursive_cycles = 2;
    options.update_model_world = false;
    options.enable_hypokosh = !evidence_only;
    options.enable_dwm = configuration == "G2";
    options.enable_opposition_research = configuration == "G2";

    CompleteHypoKoshRuntime runtime(db);
    std::map<uint32_t, std::string> edge_to_ref;
    std::map<std::string, uint32_t> evidence_node_by_ref;
    std::vector<StepResult> steps;

    bool previous_available = false;
    bool previous_has_answer = false;
    uint32_t previous_operative = 0;
    uint32_t previous_committed = 0;
    std::string previous_status = "none";

    for (const Observation& observation : observations) {
      if (observation.kind == "revoke" && !observation.revokes.empty()) {
        for (const std::string& ref : split(observation.revokes, ',')) {
          const auto it = evidence_node_by_ref.find(ref);
          if (it == evidence_node_by_ref.end()) {
            throw std::runtime_error("revoke references unknown evidence " + ref);
          }
          require(db.delete_node(it->second), "revoke evidence " + ref);
        }
      }

      uint32_t evidence_node = 0;
      NodeInput evidence = make_node(
          observation.id + " " + observation.kind + " " +
              observation.bears_on,
          {0.0f, 1.0f, 0.0f},
          signature,
          false);
      evidence.metadata["event_id"] = observation.id;
      evidence.metadata["family"] = observation.family;
      require(db.put_node(evidence, &evidence_node),
              "put evidence " + observation.id);
      evidence_node_by_ref[observation.id] = evidence_node;

      const uint32_t target =
          observation.bears_on == "H2" ? h2 : h1;
      EdgeInput edge;
      edge.from = target;
      edge.to = evidence_node;
      edge.origin = EdgeOrigin::Observed;
      edge.role =
          observation.kind == "support"
              ? EdgeRole::Supports
              : EdgeRole::Contradicts;
      edge.confidence =
          observation.kind == "support" ? 0.90 : 0.95;
      edge.metadata["source_id"] = observation.id;
      edge.metadata["evidence_family_id"] = observation.family;
      edge.metadata["event_kind"] = observation.kind;
      if (!observation.depends_on.empty()) {
        edge.metadata["derivation_id"] = observation.depends_on;
      }
      uint32_t edge_id = 0;
      require(db.put_edge(edge, &edge_id), "put edge " + observation.id);
      edge_to_ref[edge_id] = observation.id;

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
    }

    std::cout
        << "{\"schema\":\"epistemic-process-v2-runtime-telemetry-v1\""
        << ",\"episode_id\":\"" << json_escape(episode_id)
        << "\",\"configuration\":\"" << json_escape(configuration)
        << "\",\"score_bearing\":false"
        << ",\"hypothesis_nodes\":{\"H1\":" << h1
        << ",\"H2\":" << h2 << "}"
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

      std::vector<std::string> gaps;
      const auto& traces = result.receipt.recovery_trace;
      if (traces.size() > 1) {
        gaps.push_back(
            "intermediate_round_target_ranking_not_exposed_by_production_receipt");
        gaps.push_back(
            "intermediate_round_operative_and_commitment_state_not_exposed_by_production_receipt");
      }
      if (!traces.empty()) {
        gaps.push_back(
            "per_round_status_change_not_exposed_by_production_receipt");
      }
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
    std::cerr << "cycle4_runtime_runner_error=" << error.what() << "\n";
    return 1;
  }
}
