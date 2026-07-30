  if (value.empty() || value == "observed") *output = EdgeOrigin::Observed;
  else if (value == "discovered") *output = EdgeOrigin::Discovered;
  else if (value == "inferred") *output = EdgeOrigin::Inferred;
  else if (value == "reinforced") *output = EdgeOrigin::Reinforced;
  else if (value == "hypothetical") *output = EdgeOrigin::Hypothetical;
  else return false;
  return true;
}

static bool parse_strict_edge_role(const std::string& raw, EdgeRole* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "supports" || value == "support") {
    *output = EdgeRole::Supports;
  } else if (value == "mechanistic") *output = EdgeRole::Mechanistic;
  else if (value == "compressed") *output = EdgeRole::Compressed;
  else if (value == "analogical") *output = EdgeRole::Analogical;
  else if (value == "predictive") *output = EdgeRole::Predictive;
  else if (value == "causal") *output = EdgeRole::Causal;
  else if (value == "contradicts" || value == "contradiction") {
    *output = EdgeRole::Contradicts;
  } else if (value == "supersedes" || value == "supersession") {
    *output = EdgeRole::Supersedes;
  } else {
    return false;
  }
  return true;
}

static bool parse_bond_type(const std::string& raw, BondType* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "none") *output = BondType::None;
  else if (value == "sigma") *output = BondType::Sigma;
  else if (value == "pi") *output = BondType::Pi;
  else if (value == "van_der_waals" || value == "vanderwaals") {
    *output = BondType::VanDerWaals;
  } else if (value == "defect") *output = BondType::Defect;
  else if (value == "synthetic") *output = BondType::Synthetic;
  else return false;
  return true;
}

static bool parse_defect_type(const std::string& raw, DefectType* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "none") *output = DefectType::None;
  else if (value == "vacancy") *output = DefectType::Vacancy;
  else if (value == "substitution") *output = DefectType::Substitution;
  else if (value == "stone_wales" || value == "stonewales") {
    *output = DefectType::StoneWales;
  } else if (value == "strain") *output = DefectType::Strain;
  else if (value == "doped") *output = DefectType::Doped;
  else if (value == "boundary") *output = DefectType::Boundary;
  else return false;
  return true;
}

static bool parse_layer_coupling(const std::string& raw,
                                 LayerCoupling* output) {
  const std::string value = lower_copy(raw);
  if (value.empty() || value == "same_layer") {
    *output = LayerCoupling::SameLayer;
  } else if (value == "none") *output = LayerCoupling::None;
  else if (value == "van_der_waals" || value == "vanderwaals") {
    *output = LayerCoupling::VanDerWaals;
  } else if (value == "bernal_stacked") *output = LayerCoupling::BernalStacked;
  else if (value == "twisted") *output = LayerCoupling::Twisted;
  else if (value == "synthetic") *output = LayerCoupling::Synthetic;
  else return false;
  return true;
}

static bool parse_extraction_request(const std::string& body,
                                     uint32_t dimension,
                                     size_t maximum_nodes,
                                     ExtractionInput* output,
                                     std::string* error) {
  JsonDocumentValue root;
  JsonDocumentParser parser(body);
  if (!parser.parse(&root, error)) return false;
  if (root.kind != JsonDocumentValue::Kind::Object) {
    *error = "request body must be a JSON object";
    return false;
  }
  if (!reject_unknown_json_fields(
          root,
          {"schema_version", "source_id", "source_uri", "extraction_run_id",
           "layer", "incident", "signature", "place_missing_lattice",
           "idempotent", "nodes", "relations"},
          "request", error)) {
    return false;
  }

  ExtractionInput input;
  if (!extraction_integer_field(root, "schema_version",
                                kExtractionSchemaVersion,
                                &input.schema_version, error) ||
      !extraction_string_field(root, "source_id",
                               kMaxExtractionIdentifierBytes, true,
                               &input.source_id, error) ||
      !extraction_string_field(root, "source_uri",
                               kMaxExtractionMetadataValueBytes, false,
                               &input.source_uri, error) ||
      !extraction_string_field(root, "extraction_run_id",
                               kMaxExtractionIdentifierBytes, false,
                               &input.extraction_run_id, error) ||
      !extraction_integer_field(root, "layer", int32_t{0}, &input.layer,
                                error) ||
      !extraction_integer_field(root, "incident", uint32_t{0},
                                &input.incident, error) ||
      !extraction_integer_field(root, "signature", uint64_t{0},
                                &input.signature, error) ||
      !extraction_bool_field(root, "place_missing_lattice", true,
                             &input.place_missing_lattice, error) ||
      !extraction_bool_field(root, "idempotent", true, &input.idempotent,
                             error)) {
    return false;
  }
  if (input.schema_version != kExtractionSchemaVersion) {
    *error = "unsupported extraction schema_version";
    return false;
  }

  const JsonDocumentValue* nodes = json_member(root, "nodes");
  const JsonDocumentValue* relations = json_member(root, "relations");
  if (nodes && nodes->kind != JsonDocumentValue::Kind::Array) {
    *error = "nodes must be an array";
    return false;
  }
  if (relations && relations->kind != JsonDocumentValue::Kind::Array) {
    *error = "relations must be an array";
    return false;
  }
  const size_t node_count = nodes ? nodes->array.size() : 0;
  const size_t relation_count = relations ? relations->array.size() : 0;
  if (node_count == 0 && relation_count == 0) {
    *error = "at least one node or relation is required";
    return false;
  }
  if (node_count > maximum_nodes) {
    *error = "nodes exceeds configured maximum of " +
             std::to_string(maximum_nodes);
    return false;
  }
  if (relation_count > kMaxExtractionRelations) {
    *error = "relations exceeds maximum of " +
             std::to_string(kMaxExtractionRelations);
    return false;
  }

  input.nodes.reserve(node_count);
  if (nodes) {