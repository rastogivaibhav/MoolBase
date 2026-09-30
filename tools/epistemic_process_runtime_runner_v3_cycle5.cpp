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
  std::vector<std::string> depends_on;
  std::vector<std::string> revokes;
  std::vector<std::string> supersedes;
  bool independent{true};
  bool semantic_verification{true};
  bool material{false};
};

std::vector<std::string> split(const std::string& value, char delimiter) {
  std::vector<std::string> out;
  if (value.empty()) return out;
  std::stringstream stream(value);
  std::string part;
  while (std::getline(stream, part, delimiter)) {
    if (!part.empty()) out.push_back(part);
  }
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

void require(Status status, const std::string& operation) {
  if (!status) throw std::runtime_error(operation + ": " + status.message);
}

template <typename T>
void sort_unique(std::vector<T>* values) {
  std::sort(values->begin(), values->end());
  values->erase(std::unique(values->begin(), values->end()), values->end());
}

void print_string_array(const std::vector<std::string>& values) {
  std::cout << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << '"' << json_escape(values[i]) << '"';
  }
  std::cout << ']';
}

void print_u32_array(const std::vector<uint32_t>& values) {
  std::cout << '[';
  for (size_t i = 0; i < values.size(); ++i) {
    if (i) std::cout << ',';
    std::cout << values[i];
  }
  std::cout << ']';
}

std::vector<Observation> load_observations(const fs::path& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open observation file " + path.string());
  std::vector<Observation> out;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) continue;
    std::vector<std::string> fields;
    std::stringstream row(line);
    std::string part;
    while (std::getline(row, part, '\t')) fields.push_back(part);
    while (fields.size() < 11) fields.emplace_back();
    if (fields.size() != 11) {
      throw std::runtime_error("Cycle-5 TSV row must contain 11 fields");
    }
    Observation o;
    o.step = std::stoi(fields[0]);
    o.id = fields[1];
    o.family = fields[2];
    o.kind = fields[3];
    o.bears_on = fields[4];
    o.depends_on = split(fields[5], ',');
    o.revokes = split(fields[6], ',');
    o.supersedes = split(fields[7], ',');
    o.independent = fields[8] != "0";
    o.semantic_verification = fields[9] != "0";
    o.material = fields[10] == "1";
    out.push_back(std::move(o));
  }
  return out;
}

NodeInput make_node(std::string content,
                    std::vector<float> vector,
                    uint64_t signature,
                    bool root,
                    const std::string& source) {
  NodeInput node;
  node.content = std::move(content);
  node.vector = std::move(vector);
  node.signature = signature;
  node.root = root;
  node.incident = 7505;
  node.metadata["source"] = source;
  return node;
}

bool creates_evidence_edge(const Observation& observation) {
  return observation.kind == "support" ||
         observation.kind == "refute" ||
         observation.kind == "supersede" ||
         observation.kind == "noop";
}

EdgeRole edge_role(const Observation& observation) {
  return observation.kind == "refute"
      ? EdgeRole::Contradicts
      : EdgeRole::Supports;
}

double edge_confidence(const Observation& observation) {
  if (observation.kind == "refute") return observation.material ? 0.95 : 0.80;
  return 0.90;
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
    const FiberBundle& bundle,
    const std::map<uint32_t, std::string>& edge_to_ref) {
  std::vector<std::string> refs;
  for (uint32_t edge : bundle_edges(bundle)) {
    const auto it = edge_to_ref.find(edge);
    if (it != edge_to_ref.end()) refs.push_back(it->second);
  }
  sort_unique(&refs);
  return refs;
}

bool status_commits(GovernedEpistemicStatus status) {
  return status == GovernedEpistemicStatus::Resolved ||
         status == GovernedEpistemicStatus::ProvisionallyResolved;
}

void print_event(const NativeEpistemicEvent& event) {
  std::cout
      << "{\"source\":\"" << epistemic_event_source_name(event.source)
      << "\",\"type\":\"" << epistemic_event_type_name(event.type)
      << "\",\"previous_hypothesis_node\":" << event.previous_hypothesis_node
      << ",\"hypothesis_node\":" << event.hypothesis_node
      << ",\"competing_hypotheses\":";
  print_u32_array(event.competing_hypotheses);
  std::cout << ",\"reopen_nodes\":";
  print_u32_array(event.reopen_nodes);
  std::cout << ",\"evidence_edges\":";
  print_u32_array(event.evidence_edges);
  std::cout << ",\"evidence_family_ids\":";
  print_string_array(event.evidence_family_ids);
  std::cout
      << ",\"epistemic_state\":\"" << json_escape(event.epistemic_state)
      << "\",\"reason\":\"" << json_escape(event.reason) << "\"}";
}

void print_state(const HypoKoshRuntimeResult& result,
                 bool evidence_only) {
  const bool has_answer =
      !evidence_only && result.final_convergence.has_answer;
  const uint32_t operative =
      has_answer ? result.final_convergence.primary_node : 0;
  const uint32_t committed =
      has_answer && status_commits(result.status) ? operative : 0;
  std::cout
      << "{\"has_answer\":" << (has_answer ? "true" : "false")
      << ",\"operative_hypothesis_node\":" << operative
      << ",\"committed_answer_node\":" << committed
      << ",\"epistemic_status\":\""
      << (evidence_only ? "evidence_only" : governed_status_name(result.status))
      << "\",\"bundle_hash\":" << result.final_bundle.immutable_hash
      << ",\"visited_states\":" << result.final_bundle.visited_states
      << ",\"truncated\":" << (result.final_bundle.truncated ? "true" : "false")
      << ",\"evidence_family_ids\":";
  print_string_array(bundle_families(result.final_bundle));
  std::cout << '}';
}

void print_recovery_rounds(const HypoKoshRuntimeResult& result) {
  std::cout << '[';
  for (size_t i = 0; i < result.receipt.recovery_trace.size(); ++i) {
    if (i) std::cout << ',';
    const auto& row = result.receipt.recovery_trace[i];
    const bool operative_changed =
        row.previous_has_answer != row.next_has_answer ||
        (row.previous_has_answer && row.next_has_answer &&
         row.previous_primary_node != row.next_primary_node);
    std::cout
        << "{\"round_index\":" << row.round_index
        << ",\"dialectical_challenge_present\":"
        << (row.dialectical_challenge_present ? "true" : "false")
        << ",\"corroboration_search_requested\":"
        << (row.corroboration_search_requested ? "true" : "false")
        << ",\"recovery_search_requested\":"
        << (row.recovery_search_requested ? "true" : "false")
        << ",\"opposition_search_requested\":"
        << (row.opposition_search_requested ? "true" : "false")
        << ",\"expansion_opportunity_available\":"
        << (row.expansion_opportunity_available ? "true" : "false")
        << ",\"expansion_opportunity_class\":\""
        << json_escape(row.expansion_opportunity_class) << "\""
        << ",\"bundle_hash_before\":" << row.previous_bundle_hash
        << ",\"bundle_hash_after\":" << row.next_bundle_hash
        << ",\"visited_states_before\":" << row.previous_visited_states
        << ",\"visited_states_after\":" << row.next_visited_states
        << ",\"frontier_changed\":"
        << ((row.bundle_changed || row.frontier_progress) ? "true" : "false")
        << ",\"has_answer_before\":"
        << (row.previous_has_answer ? "true" : "false")
        << ",\"has_answer_after\":"
        << (row.next_has_answer ? "true" : "false")
        << ",\"operative_hypothesis_node_before\":" << row.previous_primary_node
        << ",\"operative_hypothesis_node_after\":" << row.next_primary_node
        << ",\"committed_answer_node_before\":" << row.previous_committed_node
        << ",\"committed_answer_node_after\":" << row.next_committed_node
        << ",\"status_before\":\"" << governed_status_name(row.previous_status)
        << "\",\"status_after\":\"" << governed_status_name(row.next_status)
        << "\",\"operative_hypothesis_changed\":"
        << (operative_changed ? "true" : "false")
        << ",\"status_changed\":"
        << (row.previous_status != row.next_status ? "true" : "false")
        << ",\"committed_answer_changed\":"
        << (row.previous_committed_node != row.next_committed_node ? "true" : "false")
        << ",\"stop_reason\":\"" << json_escape(row.stop_reason) << "\"}";
  }
  std::cout << ']';
}

uint32_t add_evidence(
    GrapheneDB& db,
    const Observation& observation,
    uint32_t target,
    uint64_t signature,
    bool latent,
    std::map<std::string, uint32_t>* evidence_node_by_ref,
    std::map<uint32_t, std::string>* edge_to_ref) {
  uint32_t node_id = 0;
  const std::vector<float> vector =
      latent ? std::vector<float>{0.20f, 0.98f, 0.0f}
             : std::vector<float>{1.0f, 0.0f, 0.0f};
  NodeInput node = make_node(
      observation.id + " " + observation.kind + " " + observation.bears_on,
      vector, signature, false,
      latent ? "ep-process-v3-cycle5-latent" : "ep-process-v3-cycle5-visible");
  node.metadata["event_id"] = observation.id;
  require(db.put_node(node, &node_id), "put evidence " + observation.id);

  EdgeInput edge;
  edge.from = target;
  edge.to = node_id;
  edge.origin = EdgeOrigin::Observed;
  edge.role = edge_role(observation);
  edge.confidence = edge_confidence(observation);
  edge.metadata["source_id"] = observation.id;
  edge.metadata["evidence_family_id"] = observation.family;
  edge.metadata["event_kind"] = observation.kind;
  edge.metadata["evidence_state"] = "active";
  if (!observation.depends_on.empty()) {
    std::string joined;
    for (size_t i = 0; i < observation.depends_on.size(); ++i) {
      if (i) joined += ',';
      joined += observation.depends_on[i];
    }
    edge.metadata["derivation_id"] = joined;
  }
  uint32_t edge_id = 0;
  require(db.put_edge(edge, &edge_id), "put evidence edge " + observation.id);
  (*evidence_node_by_ref)[observation.id] = node_id;
  (*edge_to_ref)[edge_id] = observation.id;
  return node_id;
}

void revoke_refs(GrapheneDB& db,
                 const std::vector<std::string>& refs,
                 const std::map<std::string, uint32_t>& evidence_node_by_ref) {
  for (const auto& ref : refs) {
    const auto it = evidence_node_by_ref.find(ref);
    if (it == evidence_node_by_ref.end()) {
      throw std::runtime_error("lifecycle transition references unknown evidence " + ref);
    }
    require(db.delete_node(it->second), "deactivate evidence " + ref);
  }
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 12) {
    std::cerr
        << "usage: ep_v3_cycle5_runner <G0E|G1|G2> <episode> <visible-tsv> "
        << "<latent-tsv> <db-dir> <seed> <target-order> <padding> "
        << "<semantic-candidates> <max-paths> <max-visited>\n";
    return 2;
  }

  try {
    const std::string configuration = argv[1];
    const std::string episode_id = argv[2];
    const fs::path visible_path = argv[3];
    const fs::path latent_path = argv[4];
    const fs::path db_dir = argv[5];
    const uint64_t seed = std::stoull(argv[6]);
    const std::string target_order = argv[7];
    const int padding = std::stoi(argv[8]);
    const size_t semantic_candidates = std::stoull(argv[9]);
    const size_t max_paths = std::stoull(argv[10]);
    const size_t max_visited = std::stoull(argv[11]);
    (void)seed;

    if (configuration != "G0E" &&
        configuration != "G1" &&
        configuration != "G2") {
      throw std::runtime_error("configuration must be G0E, G1 or G2");
    }

    const auto visible = load_observations(visible_path);
    const auto latent = load_observations(latent_path);
    const bool evidence_only = configuration == "G0E";
    fs::remove_all(db_dir);

    GrapheneDB db;
    DBOptions db_options;
    db_options.dimension = 3;
    db_options.fsync_on_commit = false;
    require(db.open(db_dir, db_options), "open database");

    const uint64_t signature = signature_for(21, 34);

    for (int i = 0; i < padding; ++i) {
      uint32_t ignored = 0;
      require(db.put_node(
          make_node("padding-" + std::to_string(i),
                    {-1.0f, 0.0f, 0.0f}, signature, false,
                    "ep-process-v3-cycle5-padding"),
          &ignored), "put padding node");
    }

    std::map<std::string, uint32_t> target_nodes;
    const auto order = split(target_order, ',');
    if (order.size() != 2 ||
        std::set<std::string>(order.begin(), order.end()) !=
            std::set<std::string>{"H1","H2"}) {
      throw std::runtime_error("target order must contain H1,H2 exactly once");
    }
    for (const auto& name : order) {
      uint32_t node = 0;
      require(db.put_node(
          make_node(name, {-1.0f, 0.0f, 0.0f}, signature, true,
                    "ep-process-v3-cycle5-target"),
          &node), "put target " + name);
      target_nodes[name] = node;
    }

    // Harness-only neutral decoys reserve the initial semantic-candidate
    // budget so latent evidence cannot leak into an early step simply because
    // later visible observations have not arrived yet. Visible evidence scores
    // above decoys; latent evidence scores below them and becomes reachable
    // only after production expands the candidate budget.
    const size_t decoys =
        !latent.empty() && semantic_candidates > 0
            ? semantic_candidates - 1
            : 0;
    for (size_t i = 0; i < decoys; ++i) {
      uint32_t ignored = 0;
      require(db.put_node(
          make_node("frontier-decoy-" + std::to_string(i),
                    {0.80f, 0.60f, 0.0f}, signature, false,
                    "ep-process-v3-cycle5-frontier-decoy"),
          &ignored), "put frontier decoy");
    }

    std::map<std::string, uint32_t> evidence_node_by_ref;
    std::map<uint32_t, std::string> edge_to_ref;

    for (const auto& event : latent) {
      if (!creates_evidence_edge(event)) continue;
      add_evidence(
          db, event, target_nodes.at(event.bears_on), signature, true,
          &evidence_node_by_ref, &edge_to_ref);
    }

    RuntimeOptions options;
    options.dialectic.mode = QueryMode::Empirical;
    options.dialectic.semantic_candidates = semantic_candidates;
    options.dialectic.max_hops = 2;
    options.dialectic.max_paths = max_paths;
    options.dialectic.max_paths_per_root =
        std::max<size_t>(1, std::min<size_t>(8, max_paths));
    options.dialectic.max_visited_states = max_visited;
    options.dialectic.minimum_confidence = 0.10;
    options.dialectic.reexpansion_threshold = 0.25;
    options.max_recursive_cycles = 2;
    options.update_model_world = false;
    options.enable_hypokosh = !evidence_only;
    options.enable_dwm = configuration == "G2";
    options.enable_opposition_research = configuration == "G2";

    CompleteHypoKoshRuntime runtime(db);

    std::cout
        << "{\"schema\":\"epistemic-process-v3-cycle5-runtime-telemetry-v1\""
        << ",\"episode_id\":\"" << json_escape(episode_id)
        << "\",\"configuration\":\"" << configuration
        << "\",\"score_bearing\":false"
        << ",\"hypothesis_nodes\":{\"H1\":" << target_nodes.at("H1")
        << ",\"H2\":" << target_nodes.at("H2") << "}"
        << ",\"steps\":[";

    bool first_step = true;
    for (const auto& observation : visible) {
      if (observation.kind == "revoke") {
        revoke_refs(db, observation.revokes, evidence_node_by_ref);
      } else if (observation.kind == "supersede") {
        revoke_refs(db, observation.supersedes, evidence_node_by_ref);
        add_evidence(
            db, observation, target_nodes.at(observation.bears_on), signature,
            false, &evidence_node_by_ref, &edge_to_ref);
      } else if (creates_evidence_edge(observation)) {
        add_evidence(
            db, observation, target_nodes.at(observation.bears_on), signature,
            false, &evidence_node_by_ref, &edge_to_ref);
      }

      const HypoKoshRuntimeResult result =
          runtime.reason({1.0f, 0.0f, 0.0f}, signature, options);

      if (!first_step) std::cout << ',';
      first_step = false;
      std::cout
          << "{\"step\":" << observation.step
          << ",\"ingested_evidence_ids\":[\""
          << json_escape(observation.id) << "\"]"
          << ",\"final_state\":";
      print_state(result, evidence_only);
      std::cout << ",\"active_evidence_refs\":";
      print_string_array(refs_for_edges(result.final_bundle, edge_to_ref));
      std::cout << ",\"native_events\":[";
      for (size_t i = 0; i < result.receipt.epistemic_events.size(); ++i) {
        if (i) std::cout << ',';
        print_event(result.receipt.epistemic_events[i]);
      }
      std::cout << "],\"recovery_rounds\":";
      print_recovery_rounds(result);
      std::cout
          << ",\"telemetry_gaps\":[]"
          << ",\"execution\":{\"expansion_rounds\":"
          << result.receipt.expansion_rounds
          << ",\"visited_states_total\":" << result.final_bundle.visited_states
          << ",\"evidence_edges_considered\":"
          << bundle_edges(result.final_bundle).size()
          << ",\"runtime_failure\":false}}";
    }

    std::cout << "],\"failures\":[]}" << std::endl;
    require(db.close(), "close database");
    fs::remove_all(db_dir);
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "v3_cycle5_runtime_runner_error=" << error.what() << "\n";
    return 1;
  }
}
