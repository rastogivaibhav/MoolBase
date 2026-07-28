    } else if (point <= 0x7ff) {
      output->push_back(static_cast<char>(0xc0 | (point >> 6)));
      output->push_back(static_cast<char>(0x80 | (point & 0x3f)));
    } else if (point <= 0xffff) {
      output->push_back(static_cast<char>(0xe0 | (point >> 12)));
      output->push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3f)));
      output->push_back(static_cast<char>(0x80 | (point & 0x3f)));
    } else {
      output->push_back(static_cast<char>(0xf0 | (point >> 18)));
      output->push_back(static_cast<char>(0x80 | ((point >> 12) & 0x3f)));
      output->push_back(static_cast<char>(0x80 | ((point >> 6) & 0x3f)));
      output->push_back(static_cast<char>(0x80 | (point & 0x3f)));
    }
  }

  bool parse_hex_quad(uint32_t* output) {
    if (position_ + 4 > input_.size()) return fail("truncated unicode escape");
    uint32_t value = 0;
    for (size_t i = 0; i < 4; ++i) {
      const unsigned char c = static_cast<unsigned char>(input_[position_++]);
      value <<= 4;
      if (c >= '0' && c <= '9') value |= c - '0';
      else if (c >= 'a' && c <= 'f') value |= c - 'a' + 10;
      else if (c >= 'A' && c <= 'F') value |= c - 'A' + 10;
      else return fail("invalid unicode escape");
    }
    *output = value;
    return true;
  }

  bool parse_string(std::string* output) {
    if (!consume('"')) return fail("expected JSON string");
    output->clear();
    while (position_ < input_.size()) {
      const unsigned char c = static_cast<unsigned char>(input_[position_++]);
      if (c == '"') return true;
      if (c < 0x20) return fail("unescaped control character in string");
      if (c != '\\') {
        output->push_back(static_cast<char>(c));
        continue;
      }
      if (position_ >= input_.size()) return fail("truncated string escape");
      const char escape = input_[position_++];
      switch (escape) {
        case '"': output->push_back('"'); break;
        case '\\': output->push_back('\\'); break;
        case '/': output->push_back('/'); break;
        case 'b': output->push_back('\b'); break;
        case 'f': output->push_back('\f'); break;
        case 'n': output->push_back('\n'); break;
        case 'r': output->push_back('\r'); break;
        case 't': output->push_back('\t'); break;
        case 'u': {
          uint32_t point = 0;
          if (!parse_hex_quad(&point)) return false;
          if (point >= 0xd800 && point <= 0xdbff) {
            if (position_ + 2 > input_.size() || input_[position_] != '\\' ||
                input_[position_ + 1] != 'u') {
              return fail("high surrogate without low surrogate");
            }
            position_ += 2;
            uint32_t low = 0;
            if (!parse_hex_quad(&low)) return false;
            if (low < 0xdc00 || low > 0xdfff) {
              return fail("invalid low surrogate");
            }
            point = 0x10000 + ((point - 0xd800) << 10) + (low - 0xdc00);
          } else if (point >= 0xdc00 && point <= 0xdfff) {
            return fail("low surrogate without high surrogate");
          }
          append_utf8(point, output);
          break;
        }
        default: return fail("invalid string escape");
      }
    }
    return fail("unterminated JSON string");
  }

  bool parse_number(JsonDocumentValue* output) {
    const size_t start = position_;
    if (position_ < input_.size() && input_[position_] == '-') ++position_;
    if (position_ >= input_.size()) return fail("invalid JSON number");
    if (input_[position_] == '0') {
      ++position_;
    } else if (input_[position_] >= '1' && input_[position_] <= '9') {
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_]))) {
        ++position_;
      }
    } else {
      return fail("invalid JSON number");
    }
    if (position_ < input_.size() && input_[position_] == '.') {
      ++position_;
      const size_t fraction = position_;
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_]))) {
        ++position_;
      }
      if (fraction == position_) return fail("invalid JSON fraction");
    }
    if (position_ < input_.size() &&
        (input_[position_] == 'e' || input_[position_] == 'E')) {
      ++position_;
      if (position_ < input_.size() &&
          (input_[position_] == '+' || input_[position_] == '-')) {
        ++position_;
      }
      const size_t exponent = position_;
      while (position_ < input_.size() &&
             std::isdigit(static_cast<unsigned char>(input_[position_]))) {
        ++position_;
      }
      if (exponent == position_) return fail("invalid JSON exponent");
    }
    output->kind = JsonDocumentValue::Kind::Number;
    output->scalar = input_.substr(start, position_ - start);
    return true;
  }

  bool parse_array(JsonDocumentValue* output, size_t depth) {
    consume('[');
    output->kind = JsonDocumentValue::Kind::Array;
    skip_space();
    if (consume(']')) return true;
    while (true) {
      JsonDocumentValue item;
      if (!parse_value(&item, depth + 1)) return false;
      output->array.push_back(std::move(item));
      skip_space();
      if (consume(']')) return true;
      if (!consume(',')) return fail("expected ',' or ']' in array");
      skip_space();
    }
  }

  bool parse_object(JsonDocumentValue* output, size_t depth) {
    consume('{');
    output->kind = JsonDocumentValue::Kind::Object;
    skip_space();
    if (consume('}')) return true;
    while (true) {
      std::string key;
      if (!parse_string(&key)) return false;
      skip_space();
      if (!consume(':')) return fail("expected ':' after object key");
      skip_space();
      JsonDocumentValue value;
      if (!parse_value(&value, depth + 1)) return false;
