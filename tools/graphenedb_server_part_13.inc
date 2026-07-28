    for (size_t index = 0; index < nodes->array.size(); ++index) {
      const JsonDocumentValue& value = nodes->array[index];
      const std::string context = "nodes[" + std::to_string(index) + "]";
      if (value.kind != JsonDocumentValue::Kind::Object) {
        *error = context + " must be an object";
        return false;
      }
      if (!reject_unknown_json_fields(
              value,
              {"external_id", "content", "signature", "incident", "role",
               "metadata", "lattice", "defect_type"},
              context, error)) {
        return false;
      }
      ExtractionNode node;
      std::string role;
      std::string defect;
      if (!extraction_string_field(value, "external_id",
                                   kMaxExtractionIdentifierBytes, true,
                                   &node.external_id, error) ||
          !extraction_string_field(value, "content", kMaxNodeTextBytes, true,
                                   &node.content, error) ||
          !extraction_integer_field(value, "signature", uint64_t{0},
                                    &node.signature, error) ||
          !extraction_integer_field(value, "incident", uint32_t{0},
                                    &node.incident, error) ||
          !extraction_string_field(value, "role", 32, false, &role, error) ||
          !extraction_string_field(value, "defect_type", 32, false, &defect,
                                   error) ||
          !extraction_metadata(value, &node.metadata, error)) {
        *error = context + ": " + *error;
        return false;
      }
      if (!parse_extraction_role(role, &node.role)) {
        *error = context + ": role must be node, root, symptom, or impact";
        return false;
      }
      if (!parse_defect_type(defect, &node.defect_type)) {
        *error = context + ": unsupported defect_type";
        return false;
      }
      const JsonDocumentValue* lattice = json_member(value, "lattice");
      if (lattice) {
        if (lattice->kind != JsonDocumentValue::Kind::Object ||
            !reject_unknown_json_fields(*lattice, {"q", "r", "layer"},
                                        context + ".lattice", error)) {
          if (error->empty()) *error = context + ": lattice must be an object";
          return false;
        }
        if (!json_member(*lattice, "q") || !json_member(*lattice, "r")) {
          *error = context + ".lattice: q and r are required";
          return false;
        }
        LatticeCoord coordinate;
        if (!extraction_integer_field(*lattice, "q", int32_t{0},
                                      &coordinate.q, error) ||
            !extraction_integer_field(*lattice, "r", int32_t{0},
                                      &coordinate.r, error) ||
            !extraction_integer_field(*lattice, "layer", input.layer,
                                      &coordinate.layer, error)) {
          *error = context + ".lattice: " + *error;
          return false;
        }
        node.lattice = coordinate;
      }
      node.vector = embed_text(node.content, dimension);
      input.nodes.push_back(std::move(node));
    }
  }

  input.relations.reserve(relation_count);
  if (relations) {
    for (size_t index = 0; index < relations->array.size(); ++index) {
      const JsonDocumentValue& value = relations->array[index];
      const std::string context = "relations[" + std::to_string(index) + "]";
      if (value.kind != JsonDocumentValue::Kind::Object) {
        *error = context + " must be an object";
        return false;
      }
      if (!reject_unknown_json_fields(
              value,
              {"from_external_id", "to_external_id", "origin", "role",
               "confidence", "evidence_id", "evidence_uri", "evidence_text",
               "metadata", "bond_type", "defect_type", "layer_coupling",
               "bond_strength"},
              context, error)) {
        return false;
      }
      ExtractionRelation relation;
      relation.bond_type = BondType::None;
      std::string origin;
      std::string role;
      std::string bond;
      std::string defect;
      std::string coupling;
      if (!extraction_string_field(value, "from_external_id",
                                   kMaxExtractionIdentifierBytes, true,
                                   &relation.from_external_id, error) ||
          !extraction_string_field(value, "to_external_id",
                                   kMaxExtractionIdentifierBytes, true,
                                   &relation.to_external_id, error) ||
          !extraction_string_field(value, "origin", 32, false, &origin,
                                   error) ||
          !extraction_string_field(value, "role", 32, false, &role, error) ||
          !extraction_double_field(value, "confidence", 0.9,
                                   &relation.confidence, error) ||
          !extraction_string_field(value, "evidence_id",
                                   kMaxExtractionIdentifierBytes, false,
                                   &relation.evidence_id, error) ||
          !extraction_string_field(value, "evidence_uri",
                                   kMaxExtractionMetadataValueBytes, false,
                                   &relation.evidence_uri, error) ||
          !extraction_string_field(value, "evidence_text",
                                   kMaxNodeTextBytes, false,
                                   &relation.evidence_text, error) ||
          !extraction_string_field(value, "bond_type", 32, false, &bond,
                                   error) ||
          !extraction_string_field(value, "defect_type", 32, false, &defect,
                                   error) ||
          !extraction_string_field(value, "layer_coupling", 32, false,
                                   &coupling, error) ||
          !extraction_double_field(value, "bond_strength", 0.85,
                                   &relation.bond_strength, error) ||
          !extraction_metadata(value, &relation.metadata, error)) {
        *error = context + ": " + *error;
        return false;
      }
      if (!parse_strict_origin(origin, &relation.origin)) {
        *error = context + ": unsupported origin";
        return false;
      }
      if (!parse_strict_edge_role(role, &relation.role)) {
        *error = context + ": unsupported role";
        return false;
      }
      if (!parse_bond_type(bond, &relation.bond_type)) {
        *error = context + ": unsupported bond_type";
        return false;
      }
      if (!parse_defect_type(defect, &relation.defect_type)) {
        *error = context + ": unsupported defect_type";
        return false;
      }
      if (!parse_layer_coupling(coupling, &relation.layer_coupling)) {
        *error = context + ": unsupported layer_coupling";
        return false;
      }
      if (relation.confidence < 0.0 || relation.confidence > 1.0) {
        *error = context + ": confidence must be between 0 and 1";
        return false;