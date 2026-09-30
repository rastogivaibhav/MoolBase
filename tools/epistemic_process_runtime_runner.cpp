#include "graphene/hypokosh_runtime.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
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
  input.metadata["source"] = "epistemic-process-production-runner";
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
    // std::getline does not preserve a final empty field. Most observations
    // legitimately have no depends_on value, so restore that
    // empty sixth column rather than rejecting the sanitized runtime input.
    if (fields.size() == 5 && !line.empty() && line.back() == '\t') {
      fields.emplace_back();
    }
    if (fields.size() != 6) {
      throw std::runtime_error("sanitized observation row must contain 6 fields");
    }
    Observation observation;
    observation.step = std::stoi(fields[0]);
    observation.id = fields[1];
    observation.family = fields[2];
    observation.kind = fields[3];
    observation.bears_on = fields[4];
    observation.depends_on = fields[5];
    out.push_back(std::move(observation));
  }
  return out;
}

void require(Status status, const std::string& operation) {
  if (!status) {
    throw std::runtime_error(operation + ": " + status.message);
  }
}

std::string csv_u32(const std::vector<uint32_t>& values) {
  std::ostringstream out;
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) out << ',';
    out << values[i];
  }
  return out.str();
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

void print_event(const NativeEpistemicEvent& event) {
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
            << "\",\"reason\":\""
            << json_escape(event.reason) << "\"}";
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 6) {
    std::cerr
        << "usage: epistemic_process_runtime_runner "
        << "<G0|G1|G2> <episode-id> <sanitized-tsv> <db-dir> <seed>\n";
    return 2;
  }

  try {
    const std::string configuration = argv[1];
    const std::string episode_id = argv[2];
    const fs::path observations_path = argv[3];
    const fs::path db_dir = argv[4];
    const uint64_t seed = std::stoull(argv[5]);
    (void)seed;  // deterministic runner: recorded but no stochastic decisions.

    if (configuration != "G0" &&
        configuration != "G1" &&
        configuration != "G2") {
      throw std::runtime_error("configuration must be G0, G1 or G2");
    }

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
    require(db.put_node(
        make_node("H1", {1.0f, 0.0f, 0.0f}, signature, true), &h1),
        "put H1");
    require(db.put_node(
        make_node("H2", {-1.0f, 0.0f, 0.0f}, signature, true), &h2),
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
    options.enable_hypokosh = configuration != "G0";
    options.enable_dwm = configuration == "G2";
    options.enable_opposition_research = configuration == "G2";

    CompleteHypoKoshRuntime runtime(db);
    std::map<uint32_t, std::string> edge_to_ref;

    std::cout << "{\"episode_id\":\"" << json_escape(episode_id)
              << "\",\"configuration\":\"" << configuration
              << "\",\"hypothesis_nodes\":{\"H1\":" << h1
              << ",\"H2\":" << h2 << "},\"edge_to_evidence_ref\":{";

    // Ingest and execute once first so the mapping can be emitted before steps.
    struct StepResult {
      int step{0};
      HypoKoshRuntimeResult result;
      std::vector<std::string> evidence_refs;
    };
    std::vector<StepResult> steps;

    for (const Observation& observation : observations) {
      uint32_t evidence_node = 0;
      NodeInput evidence = make_node(
          observation.id + " " + observation.kind + " " +
              observation.bears_on,
          {0.0f, 1.0f, 0.0f}, signature, false);
      evidence.metadata["event_id"] = observation.id;
      evidence.metadata["family"] = observation.family;
      require(db.put_node(evidence, &evidence_node),
              "put evidence node " + observation.id);

      const uint32_t target =
          observation.bears_on == "H2" ? h2 : h1;
      const EdgeRole role =
          observation.kind == "support"
              ? EdgeRole::Supports
              : EdgeRole::Contradicts;
      EdgeInput edge;
      edge.from = target;
      edge.to = evidence_node;
      edge.origin = EdgeOrigin::Observed;
      edge.role = role;
      edge.confidence =
          observation.kind == "support" ? 0.90 : 0.95;
      edge.metadata["source_id"] = observation.id;
      edge.metadata["evidence_family_id"] = observation.family;
      edge.metadata["event_kind"] = observation.kind;
      if (!observation.depends_on.empty()) {
        edge.metadata["derivation_id"] = observation.depends_on;
      }
      uint32_t edge_id = 0;
      require(db.put_edge(edge, &edge_id),
              "put evidence edge " + observation.id);
      edge_to_ref[edge_id] = observation.id;

      StepResult step;
      step.step = observation.step;
      step.result = runtime.reason(
          {0.0f, 1.0f, 0.0f}, signature, options);
      for (uint32_t used_edge : step.result.evidence_edges) {
        const auto it = edge_to_ref.find(used_edge);
        if (it != edge_to_ref.end()) step.evidence_refs.push_back(it->second);
      }
      steps.push_back(std::move(step));
    }

    bool first_mapping = true;
    for (const auto& [edge, ref] : edge_to_ref) {
      if (!first_mapping) std::cout << ',';
      first_mapping = false;
      std::cout << '\"' << edge << "\":\"" << json_escape(ref) << '\"';
    }
    std::cout << "},\"steps\":[";

    for (size_t index = 0; index < steps.size(); ++index) {
      if (index) std::cout << ',';
      const StepResult& step = steps[index];
      std::cout << "{\"step\":" << step.step;
      if (configuration == "G0") {
        std::cout << ",\"decision\":{\"status\":\"open\","
                  << "\"hypothesis\":null,\"evidence_refs\":";
        print_string_array(step.evidence_refs);
        std::cout << "}";
      } else {
        std::cout << ",\"native_events\":[";
        for (size_t event_index = 0;
             event_index < step.result.receipt.epistemic_events.size();
             ++event_index) {
          if (event_index) std::cout << ',';
          print_event(step.result.receipt.epistemic_events[event_index]);
        }
        std::cout << ']';
      }
      std::cout << '}';
    }

    std::cout << "],\"failures\":[],\"runtime_receipts\":[";
    for (size_t index = 0; index < steps.size(); ++index) {
      if (index) std::cout << ',';
      const auto& receipt = steps[index].result.receipt;
      std::cout
          << "{\"step\":" << steps[index].step
          << ",\"hypokosh_capability_enabled\":"
          << (receipt.hypokosh_capability_enabled ? "true" : "false")
          << ",\"dwm_capability_enabled\":"
          << (receipt.dwm_capability_enabled ? "true" : "false")
          << ",\"opposition_research_enabled\":"
          << (receipt.opposition_research_enabled ? "true" : "false")
          << ",\"graphene_executed\":"
          << (receipt.graphene_executed ? "true" : "false")
          << ",\"path_verifier_executed\":"
          << (receipt.path_verifier_executed ? "true" : "false")
          << ",\"stability_critic_executed\":"
          << (receipt.stability_critic_executed ? "true" : "false")
          << ",\"epistemic_admissibility_executed\":"
          << (receipt.epistemic_admissibility_executed ? "true" : "false")
          << ",\"convergence_executed\":"
          << (receipt.convergence_executed ? "true" : "false")
          << ",\"opposition_executed\":"
          << (receipt.opposition_executed ? "true" : "false")
          << ",\"bounded_recovery_executed\":"
          << (receipt.bounded_recovery_executed ? "true" : "false")
          << ",\"governed_projection_executed\":"
          << (receipt.governed_projection_executed ? "true" : "false")
          << ",\"model_world_updated\":"
          << (receipt.model_world_updated ? "true" : "false")
          << ",\"expansion_rounds\":" << receipt.expansion_rounds
          << ",\"visited_states\":" << steps[index].result.final_bundle.visited_states
          << ",\"evidence_edge_count\":" << steps[index].result.evidence_edges.size()
          << ",\"terminal_cause\":\""
          << json_escape(receipt.terminal_cause) << "\"}";
    }
    std::cout << "]}\n";

    require(db.close(), "close database");
    fs::remove_all(db_dir);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "production_runtime_runner_error=" << error.what() << "\n";
    return 1;
  }
}
