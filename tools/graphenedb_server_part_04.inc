    position = end + 1;
  }
  return {};
}

static int http_status_for(const Status& status) {
  switch (status.code) {
    case ErrorCode::Ok: return 200;
    case ErrorCode::InvalidOption:
    case ErrorCode::DimensionMismatch:
    case ErrorCode::EdgeInvalid:
    case ErrorCode::InvalidInput: return 400;
    case ErrorCode::NodeNotFound: return 404;
    case ErrorCode::LockBusy: return 409;
    case ErrorCode::NotOpen: return 503;
    case ErrorCode::UnsupportedMode: return 501;
    case ErrorCode::IoError:
    case ErrorCode::WalCorrupt:
    case ErrorCode::DataCorrupt:
    case ErrorCode::TransactionIncomplete: return 500;
  }
  return 500;
}

static std::vector<float> embed_text(const std::string& text, uint32_t dim) {
  std::vector<float> v(dim, 0.0f);
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : text) {
    h ^= c; h *= 1099511628211ull;
    v[h % dim] += ((h >> 8) & 1) ? 1.0f : -1.0f;
  }
  float norm = 0.0f;
  for (float x : v) norm += x * x;
  norm = std::sqrt(norm);
  if (norm > 0) for (float& x : v) x /= norm;
  return v;
}

static bool parse_query_mode(const std::string& value, QueryMode* out) {
  if (!out) return false;
  if (value.empty() || value == "balanced") {
    *out = QueryMode::Balanced;
  } else if (value == "empirical") {
    *out = QueryMode::Empirical;
  } else if (value == "theoretical") {
    *out = QueryMode::Theoretical;
  } else {
    return false;
  }
  return true;
}

static void append_u32_array(std::ostringstream& output,
                             const std::vector<uint32_t>& values) {
  output << '[';
  for (size_t index = 0; index < values.size(); ++index) {
    if (index != 0) output << ',';
    output << values[index];
  }
  output << ']';
}

static void append_string_array(std::ostringstream& output,
                                const std::vector<std::string>& values) {
  output << '[';
  for (size_t index = 0; index < values.size(); ++index) {
    if (index != 0) output << ',';
    output << '"' << json_escape(values[index]) << '"';
  }
  output << ']';
}

static void append_dialectic_bundle(std::ostringstream& output,
                                    const BundleSet& bundle) {
  output << "{\"snapshot_version\":" << bundle.snapshot_version
         << ",\"semantic_candidates\":";
  append_u32_array(output, bundle.semantic_candidates);
  output << ",\"visited_states\":" << bundle.visited_states
         << ",\"truncated\":" << (bundle.truncated ? "true" : "false")
         << ",\"warnings\":";
  append_string_array(output, bundle.warnings);
  output << ",\"roots\":[";
  for (size_t root_index = 0; root_index < bundle.roots.size(); ++root_index) {
    if (root_index != 0) output << ',';
    const RootBundle& root = bundle.roots[root_index];
    output << "{\"root_node\":" << root.root_node
           << ",\"confidence\":" << root.confidence
           << ",\"degeneracy\":" << root.degeneracy
           << ",\"diversity\":" << root.diversity
           << ",\"contradiction_ratio\":" << root.contradiction_ratio
           << ",\"evidence_coverage\":" << root.evidence_coverage
           << ",\"provenance_risk\":" << root.provenance_risk
           << ",\"paths\":[";
    for (size_t path_index = 0; path_index < root.paths.size(); ++path_index) {
      if (path_index != 0) output << ',';
      const DialecticPath& path = root.paths[path_index];
      output << "{\"anchor_node\":" << path.anchor_node << ",\"nodes\":";
      append_u32_array(output, path.nodes);
      output << ",\"edges\":";
      append_u32_array(output, path.edges);
      output << ",\"score\":" << path.score
             << ",\"contains_contradiction\":"
             << (path.contains_contradiction ? "true" : "false")
             << ",\"contains_hypothetical\":"
             << (path.contains_hypothetical ? "true" : "false")
             << ",\"joint_requirements\":[";
      for (size_t joint_index = 0;
           joint_index < path.joint_requirements.size(); ++joint_index) {
        if (joint_index != 0) output << ',';
        const JointRequirement& joint = path.joint_requirements[joint_index];
        output << "{\"hyperedge_id\":\"" << json_escape(joint.hyperedge_id)
               << "\",\"target_node\":" << joint.target_node
               << ",\"source_nodes\":";
        append_u32_array(output, joint.source_nodes);
        output << ",\"member_edges\":";
        append_u32_array(output, joint.member_edges);
        output << ",\"all_sources_present\":"
               << (joint.all_sources_present ? "true" : "false") << '}';
      }
      output << "],\"provenance_findings\":[";
      for (size_t finding_index = 0;
           finding_index < path.provenance_findings.size(); ++finding_index) {
        if (finding_index != 0) output << ',';
        const ProvenanceFinding& finding =
            path.provenance_findings[finding_index];
        output << "{\"edge_id\":" << finding.edge_id << ",\"code\":\""
               << json_escape(finding.code) << "\",\"detail\":\""
               << json_escape(finding.detail) << "\"}";
      }
      output << "]}";
    }
    output << "]}";
  }
  output << "]}";
}

static void append_dialectic_convergence(std::ostringstream& output,
                                         const ConvergedAnswer& convergence) {
  output << "{\"has_answer\":"
         << (convergence.has_answer ? "true" : "false")
         << ",\"primary_node\":" << convergence.primary_node
         << ",\"confidence\":" << convergence.confidence
         << ",\"false_promotion_risk\":"
         << convergence.false_promotion_risk << ",\"evidence_edges\":";
  append_u32_array(output, convergence.evidence_edges);
  output << ",\"residual_uncertainty\":";
  append_string_array(output, convergence.residual_uncertainty);
  output << ",\"selected_path_count\":" << convergence.selected_paths.size()
         << ",\"discarded_path_count\":" << convergence.discarded_paths.size()
         << '}';
