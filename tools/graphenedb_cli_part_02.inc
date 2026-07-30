    auto ids = db.lattice_neighbors(node, hops);
    for (uint32_t id : ids) std::cout << id << "\n";
    return 0;
  }
  if (cmd == "extract-tsv") {
    if (argc < 6) { usage(); return 1; }
    std::ifstream in(argv[5]);
    if (!in) { std::cerr << "cannot read TSV: " << argv[5] << "\n"; return 2; }
    ExtractionInput extraction;
    extraction.source_id = argv[4];
    extraction.place_missing_lattice = true;
    extraction.idempotent = true;
    std::string line;
    size_t line_no = 0;
    while (std::getline(in, line)) {
      ++line_no;
      if (line_no == 1) strip_utf8_bom(line);
      if (line.empty() || line[0] == '#') continue;
      auto p = split_tab(line);
      if (p.size() != 6) { std::cerr << "bad extraction TSV field count at line " << line_no << "\n"; return 2; }
      ExtractionNode n;
      n.external_id = p[0];
      n.content = p[1];
      try {
        n.vector = parse_vec(p[2]);
        n.signature = std::stoull(p[3]);
        n.incident = static_cast<uint32_t>(std::stoul(p[4]));
      } catch (const std::exception& e) {
        std::cerr << "bad extraction TSV numeric/vector field at line " << line_no << ": " << e.what() << "\n";
        return 2;
      }
      n.role = parse_extraction_role(p[5]);
      n.metadata["extract_role"] = p[5];
      extraction.nodes.push_back(n);
    }
    for (size_t i = 1; i < extraction.nodes.size(); ++i) {
      ExtractionRelation rel;
      rel.from_external_id = extraction.nodes[i - 1].external_id;
      rel.to_external_id = extraction.nodes[i].external_id;
      rel.role = EdgeRole::Supports;
      rel.bond_type = BondType::Sigma;
      rel.layer_coupling = LayerCoupling::SameLayer;
      rel.confidence = 0.85;
      rel.bond_strength = 0.85;
      extraction.relations.push_back(rel);
    }
    ExtractionResult result;
    st = db.put_extraction(extraction, &result);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << "inserted_nodes=" << result.inserted_node_ids.size() << "\n";
    std::cout << "existing_nodes=" << result.existing_node_ids.size() << "\n";
    std::cout << "inserted_edges=" << result.inserted_edge_ids.size() << "\n";
    for (const auto& kv : result.external_to_node_id) std::cout << "external=" << kv.first << "\tnode=" << kv.second << "\n";
    return 0;
  }
  if (cmd == "import-tsv") {
    if (argc < 5) { usage(); return 1; }
    uint32_t base_id = static_cast<uint32_t>(db.node_count());
    for (int i = 5; i + 1 < argc; i += 2) {
      std::string flag = argv[i];
      if (skip_common_option(flag, i)) continue;
      if (flag == "--base-id") base_id = static_cast<uint32_t>(std::stoul(argv[i + 1]));
    }
    std::ifstream in(argv[4]);
    if (!in) { std::cerr << "cannot read TSV: " << argv[4] << "\n"; return 2; }
    std::vector<NodeInput> nodes;
    std::string line;
    size_t line_no = 0;
    while (std::getline(in, line)) {
      ++line_no;
      if (line_no == 1) strip_utf8_bom(line);
      if (line.empty() || line[0] == '#') continue;
      auto p = split_tab(line);
      if (p.size() != 5) { std::cerr << "bad TSV field count at line " << line_no << "\n"; return 2; }
      NodeInput n;
      n.content = p[0];
      try {
        n.vector = parse_vec(p[1]);
        n.signature = std::stoull(p[2]);
        n.incident = static_cast<uint32_t>(std::stoul(p[3]));
      } catch (const std::exception& e) {
        std::cerr << "bad TSV numeric/vector field at line " << line_no << ": " << e.what() << "\n";
        return 2;
      }
      n.root = p[4] == "root";
      n.symptom = p[4] == "symptom";
      n.impact = p[4] == "impact";
      n.metadata["import_role"] = p[4];
      n.metadata["importer"] = "graphenedb_cli_import_tsv_v1";
      nodes.push_back(n);
    }
    LatticePlacementOptions placement;
    placement.base_node_id = base_id;
    placement.strategy = PlacementStrategy::SemanticGroups;
    LatticePlacementResult placed;
    st = assign_lattice_batch(std::move(nodes), placement, &placed);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    BatchResult result;
    st = db.put_batch(placed.batch, &result);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    std::cout << "imported_nodes=" << result.node_ids.size() << "\n";
    std::cout << "imported_edges=" << result.edge_ids.size() << "\n";
    for (uint32_t id : result.node_ids) std::cout << "node=" << id << "\n";
    return 0;
  }
  if (cmd == "compact") {
    st = db.compact();
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    if (json) print_json_object({{"compacted", "true"}});
    else std::cout << "compacted\n";
    return 0;
  }
  if (cmd == "backup") {
    if (argc < 5) { usage(); return 1; }
    fs::path destination = argv[4];
    bool verify = true;
    for (int i = 5; i < argc; ++i) {
      std::string flag = argv[i];
      if (skip_common_option(flag, i)) continue;
      if (flag == "--no-verify") verify = false;
      else if (flag == "--verify") verify = true;
    }
    st = db.backup(destination);
    if (!st) { std::cerr << st.message << "\n"; return 2; }
    if (!json) std::cout << "backup=" << destination.string() << "\n";
    if (verify) {
      GrapheneDB restored;
      st = restored.open(destination, opt);
      if (!st) { std::cerr << "backup verification open failed: " << st.message << "\n"; return 2; }
      std::string report;
      st = restored.validate(&report);
      restored.close();
      if (!st) { std::cerr << "backup verification failed:\n" << report; return 2; }
      if (!json) std::cout << "backup_verified=true\n";
    }
    if (json) print_json_object({{"backup", destination.string()}, {"backup_verified", verify ? "true" : "false"}});
    return 0;
  }
  usage();
  return 1;
}
