                                    std::string* output,
                                    std::string* error) {
  const JsonDocumentValue* value = json_member(object, key);
  if (!value) {
    if (required) {
      *error = key + " is required";
      return false;
    }
    output->clear();
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::String) {
    *error = key + " must be a string";
    return false;
  }
  if (required && value->scalar.empty()) {
    *error = key + " must not be empty";
    return false;
  }
  if (value->scalar.size() > maximum_bytes) {
    *error = key + " exceeds " + std::to_string(maximum_bytes) + " bytes";
    return false;
  }
  *output = value->scalar;
  return true;
}

template <typename Integer>
static bool extraction_integer_field(const JsonDocumentValue& object,
                                     const std::string& key,
                                     Integer default_value,
                                     Integer* output,
                                     std::string* error) {
  const JsonDocumentValue* value = json_member(object, key);
  if (!value) {
    *output = default_value;
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::Number ||
      value->scalar.find_first_of(".eE") != std::string::npos) {
    *error = key + " must be an integer";
    return false;
  }
  Integer parsed{};
  const char* begin = value->scalar.data();
  const char* end = begin + value->scalar.size();
  const auto result = std::from_chars(begin, end, parsed);
  if (result.ec != std::errc{} || result.ptr != end) {
    *error = key + " is outside its supported integer range";
    return false;
  }
  *output = parsed;
  return true;
}

static bool extraction_double_field(const JsonDocumentValue& object,
                                    const std::string& key,
                                    double default_value,
                                    double* output,
                                    std::string* error) {
  const JsonDocumentValue* value = json_member(object, key);
  if (!value) {
    *output = default_value;
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::Number) {
    *error = key + " must be a number";
    return false;
  }
  try {
    size_t consumed = 0;
    const double parsed = std::stod(value->scalar, &consumed);
    if (consumed != value->scalar.size() || !std::isfinite(parsed)) {
      *error = key + " must be a finite number";
      return false;
    }
    *output = parsed;
    return true;
  } catch (...) {
    *error = key + " is outside its supported numeric range";
    return false;
  }
}

static bool extraction_bool_field(const JsonDocumentValue& object,
                                  const std::string& key,
                                  bool default_value,
                                  bool* output,
                                  std::string* error) {
  const JsonDocumentValue* value = json_member(object, key);
  if (!value) {
    *output = default_value;
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::Boolean) {
    *error = key + " must be a boolean";
    return false;
  }
  *output = value->boolean;
  return true;
}

static bool parse_json_object(const std::string& body,
                              JsonDocumentValue* root,
                              std::string* error) {
  JsonDocumentParser parser(body);
  if (!parser.parse(root, error)) return false;
  if (root->kind != JsonDocumentValue::Kind::Object) {
    *error = "request body must be a JSON object";
    return false;
  }
  return true;
}

static bool parse_retrieval_policy(const JsonDocumentValue& parent,
                                   const std::string& field,
                                   bool required,
                                   RetrievalPolicy* output,
                                   std::string* error) {
  const JsonDocumentValue* value = json_member(parent, field);
  if (!value) {
    if (required) {
      *error = field + " is required";
      return false;
    }
    return true;
  }
  if (value->kind != JsonDocumentValue::Kind::Object) {
    *error = field + " must be an object";
    return false;
  }
  if (!reject_unknown_json_fields(
          *value,
          {"version", "semantic_candidates", "max_hops", "max_paths",
           "max_paths_per_root", "max_visited_states",
           "max_opposition_rounds", "minimum_confidence",
           "reexpansion_threshold"},
          field, error)) {
    return false;
  }
  RetrievalPolicy policy;
  if (!extraction_string_field(*value, "version", 256, true,
                               &policy.version, error) ||
      !extraction_integer_field(*value, "semantic_candidates",
                                uint32_t{12},
                                &policy.semantic_candidates, error) ||
      !extraction_integer_field(*value, "max_hops", uint32_t{6},
                                &policy.max_hops, error) ||
      !extraction_integer_field(*value, "max_paths", uint32_t{32},
                                &policy.max_paths, error) ||