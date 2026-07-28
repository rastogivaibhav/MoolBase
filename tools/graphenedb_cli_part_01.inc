
static void print_json_object(const std::map<std::string, std::string>& values) {
  std::cout << "{";
  bool first = true;
  for (const auto& kv : values) {
    if (!first) std::cout << ",";
    first = false;
    std::cout << "\"" << json_escape(kv.first) << "\":";
    if (kv.second == "true" || kv.second == "false" || looks_integer(kv.second)) {
      std::cout << kv.second;
    } else {
      std::cout << "\"" << json_escape(kv.second) << "\"";
    }
  }
  std::cout << "}\n";
}

static void usage() {
  std::cout << "graphenedb_cli commands:\n"
            << "  init <path> <dim>\n"
            << "  inspect <path> <dim>\n"
            << "  put-node <path> <dim> <content> <comma-vector> <signature> [root|symptom|impact] [--lattice q,r,layer] [--defect type]\n"
            << "  put-edge <path> <dim> <from> <to> [causal|contradicts|supersedes|supports] [--bond type] [--coupling type] [--strength n] [--defect type]\n"
            << "  neighbors <path> <dim> <node-id> [max-hops]\n"
            << "  extract-tsv <path> <dim> <source-id> <tsv-file>\n"
            << "  import-tsv <path> <dim> <tsv-file> [--base-id N]\n"
            << "  search <path> <dim> <comma-vector> <signature>\n"
            << "  reason <path> <dim> <comma-vector> <signature> [--mode empirical|balanced|theoretical] [--max-rounds N] [--json]\n"
            << "  validate <path> <dim>\n"
            << "  backup <path> <dim> <destination-dir> [--verify]\n"
            << "  compact <path> <dim>\n"
            << "common options:\n"
            << "  --wal-rotate-bytes N   checkpoint and truncate WAL after N bytes\n"
            << "  --vector-index KIND    choose auto, flat, kdtree, or faiss\n"
            << "  --no-recover-stale-lock fail instead of removing a dead-owner LOCK file\n"
            << "  --physical-lattice    materialize graphene.lattice as hex-cell ordered storage\n"
            << "  --json                 emit machine-readable JSON for supported commands\n";
}

int main(int argc, char** argv) {
  if (argc < 2) { usage(); return 1; }
  std::string cmd = argv[1];
  if (cmd == "init") {
    if (argc < 4) { usage(); return 1; }
    GrapheneDB db;
    DBOptions opt; opt.dimension = static_cast<uint32_t>(std::stoul(argv[3]));
    apply_common_options(opt, cmd, argc, argv);
    auto st = db.open(argv[2], opt);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    db.close();
    std::cout << "initialized\n";
    return 0;
  }
  if (argc < 4) { usage(); return 1; }
  fs::path path = argv[2];
  DBOptions opt; opt.dimension = static_cast<uint32_t>(std::stoul(argv[3]));
  apply_common_options(opt, cmd, argc, argv);
  bool json = common_json_enabled(cmd, argc, argv);
  GrapheneDB db;
  auto st = db.open(path, opt);
  if (!st) { std::cerr << st.message << "\n"; return 2; }

  if (cmd == "inspect") {
    std::string out;
    st = db.inspect(&out);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    if (json) print_json_object(parse_key_values(out));
    else std::cout << out;
    return 0;
  }
  if (cmd == "validate") {
    std::string report;
    st = db.validate(&report);
    if (!st) {
      std::cerr << report;
      return 2;
    }
    if (json) {
      print_json_object({{"validation", "ok"}, {"report", report}});
    } else {
      std::cout << report;
      std::cout << "validation=ok\n";
    }
    return 0;
  }
  if (cmd == "put-node") {
    if (argc < 7) { usage(); return 1; }
    NodeInput n;
    n.content = argv[4];
    n.vector = parse_vec(argv[5]);
    n.signature = std::stoull(argv[6]);
    if (argc >= 8) {
      std::string flag = argv[7];
      n.root = flag == "root";
      n.symptom = flag == "symptom";
      n.impact = flag == "impact";
    }
    for (int i = 8; i + 1 < argc; i += 2) {
      std::string flag = argv[i];
      if (skip_common_option(flag, i)) continue;
      if (flag == "--lattice") n.lattice = parse_lattice(argv[i + 1]);
      else if (flag == "--defect") n.defect_type = parse_defect(argv[i + 1]);
    }
    uint32_t id = 0;
    st = db.put_node(n, &id);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << id << "\n";
    return 0;
  }
  if (cmd == "put-edge") {
    if (argc < 6) { usage(); return 1; }
    EdgeInput e;
    e.from = static_cast<uint32_t>(std::stoul(argv[4]));
    e.to = static_cast<uint32_t>(std::stoul(argv[5]));
    if (argc >= 7) {
      std::string role = argv[6];
      if (role == "contradicts") e.role = EdgeRole::Contradicts;
      else if (role == "supersedes") e.role = EdgeRole::Supersedes;
      else if (role == "supports") e.role = EdgeRole::Supports;
      else e.role = EdgeRole::Causal;
    }
    for (int i = 7; i + 1 < argc; i += 2) {
      std::string flag = argv[i];
      if (skip_common_option(flag, i)) continue;
      if (flag == "--bond") e.bond_type = parse_bond(argv[i + 1]);
      else if (flag == "--coupling") e.layer_coupling = parse_coupling(argv[i + 1]);
      else if (flag == "--strength") e.bond_strength = std::stod(argv[i + 1]);
      else if (flag == "--defect") e.defect_type = parse_defect(argv[i + 1]);
    }
    uint32_t id = 0;
    st = db.put_edge(e, &id);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << id << "\n";
    return 0;
  }
  if (cmd == "reason") {
    if (argc < 6) { usage(); return 1; }
    auto q = parse_vec(argv[4]);
    auto sig = std::stoull(argv[5]);
    RuntimeOptions runtime_options;
    runtime_options.dialectic.mode = QueryMode::Balanced;
    for (int i = 6; i < argc; ++i) {
      std::string flag = argv[i];
      if (flag == "--mode") {
        if (i + 1 >= argc) throw std::invalid_argument("--mode requires a value");
        runtime_options.dialectic.mode = parse_query_mode(argv[++i]);
      } else if (flag == "--max-rounds") {
        if (i + 1 >= argc) throw std::invalid_argument("--max-rounds requires a value");
        runtime_options.max_recursive_cycles = static_cast<uint32_t>(std::stoul(argv[++i]));
      } else if (flag == "--as-of") {
        if (i + 1 >= argc) throw std::invalid_argument("--as-of requires a value");
        runtime_options.dialectic.as_of = argv[++i];
      } else if (skip_common_option(flag, i)) {
        continue;
      }
    }
    ModelWorld world;
    CompleteHypoKoshRuntime runtime(db, &world);
    const auto result = runtime.reason(q, sig, runtime_options);
    if (json) {
      std::cout << "{"
                << "\"status\":\"" << governed_status_name(result.status) << "\","
                << "\"primary_node\":" << result.primary_node << ","
                << "\"confidence\":" << result.confidence << ","
                << "\"initial_bundle_hash\":" << result.receipt.initial_bundle_hash << ","
                << "\"final_bundle_hash\":" << result.receipt.final_bundle_hash << ","
                << "\"rounds\":" << result.receipt.expansion_rounds << ","
                << "\"fiber_bundle_built\":" << (result.receipt.fiber_bundle_built ? "true" : "false") << ","
                << "\"stability_critic_executed\":" << (result.receipt.stability_critic_executed ? "true" : "false") << ","
                << "\"opposition_executed\":" << (result.receipt.opposition_executed ? "true" : "false") << ","
                << "\"no_silent_promotion\":" << (result.receipt.no_silent_promotion ? "true" : "false")
                << "}\n";
    } else {
      std::cout << "status=" << governed_status_name(result.status) << "\n";
      std::cout << "primary_node=" << result.primary_node << " confidence=" << result.confidence << "\n";
      std::cout << "initial_bundle_hash=" << result.receipt.initial_bundle_hash << "\n";
      std::cout << "final_bundle_hash=" << result.receipt.final_bundle_hash << "\n";
      std::cout << "rounds=" << result.receipt.expansion_rounds << "\n";
      for (uint32_t edge : result.evidence_edges) std::cout << "evidence_edge=" << edge << "\n";
      for (const auto& uncertainty : result.residual_uncertainty) std::cout << "uncertainty=" << uncertainty << "\n";
    }
    return 0;
  }
  if (cmd == "search") {
    if (argc < 6) { usage(); return 1; }
    auto q = parse_vec(argv[4]);
    auto sig = std::stoull(argv[5]);
    auto b = db.causal_search(q, sig, QueryMode::Empirical);
    if (b.abstain) {
      std::cout << "ABSTAIN " << b.reason << "\n";
      return 0;
    }
    std::cout << "target=" << b.target_node << " confidence=" << b.confidence << " paths=" << b.paths.size() << "\n";
    for (const auto& why : b.why_retrieved) std::cout << "why=" << why << "\n";
    return 0;
  }
  if (cmd == "neighbors") {
    if (argc < 5) { usage(); return 1; }
    uint32_t node = static_cast<uint32_t>(std::stoul(argv[4]));
    uint32_t hops = argc >= 6 ? static_cast<uint32_t>(std::stoul(argv[5])) : 1;
