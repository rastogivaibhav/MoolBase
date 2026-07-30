#include "graphene/c_api.h"
#include "graphene/db.hpp"
#include <cstring>
#include <exception>
#include <memory>
#include <new>
#include <string>
#include <vector>

using namespace graphene;

struct graphenedb_handle {
  GrapheneDB db;
};

namespace {

char* copy_string(const std::string& value) {
  char* out = new char[value.size() + 1];
  std::memcpy(out, value.c_str(), value.size() + 1);
  return out;
}

graphenedb_status ok_status() {
  return {static_cast<int>(ErrorCode::Ok), nullptr};
}

graphenedb_status error_status(ErrorCode code, const std::string& message) {
  return {static_cast<int>(code), copy_string(message)};
}

graphenedb_status from_status(const Status& status) {
  if (status) return ok_status();
  return error_status(status.code, status.message);
}

graphenedb_status exception_status(const char* where, const std::exception& e) {
  return error_status(ErrorCode::InvalidInput, std::string(where) + ": " + e.what());
}

} // namespace

extern "C" {

graphenedb_status graphenedb_open(const char* path, uint32_t dimension, graphenedb_handle** out) {
  if (!path || !out) return error_status(ErrorCode::InvalidInput, "path and out handle are required");
  *out = nullptr;
  try {
    std::unique_ptr<graphenedb_handle> handle(new graphenedb_handle());
    DBOptions opt;
    opt.dimension = dimension;
    auto st = handle->db.open(path, opt);
    if (!st) return from_status(st);
    *out = handle.release();
    return ok_status();
  } catch (const std::exception& e) {
    return exception_status("graphenedb_open", e);
  } catch (...) {
    return error_status(ErrorCode::InvalidInput, "graphenedb_open: unknown exception");
  }
}

graphenedb_status graphenedb_close(graphenedb_handle* db) {
  if (!db) return ok_status();
  try {
    auto st = db->db.close();
    delete db;
    return from_status(st);
  } catch (const std::exception& e) {
    delete db;
    return exception_status("graphenedb_close", e);
  } catch (...) {
    delete db;
    return error_status(ErrorCode::InvalidInput, "graphenedb_close: unknown exception");
  }
}

graphenedb_status graphenedb_put_node(graphenedb_handle* db, const graphenedb_node_input* input, uint32_t* out_id) {
  if (!db || !input || !input->content || !input->vector) {
    return error_status(ErrorCode::InvalidInput, "db, input, content, and vector are required");
  }
  try {
    NodeInput node;
    node.content = input->content;
    node.vector.assign(input->vector, input->vector + input->vector_len);
    node.signature = input->signature;
    node.incident = input->incident;
    node.root = input->root != 0;
    node.symptom = input->symptom != 0;
    node.impact = input->impact != 0;
    auto st = db->db.put_node(node, out_id);
    return from_status(st);
  } catch (const std::exception& e) {
    return exception_status("graphenedb_put_node", e);
  } catch (...) {
    return error_status(ErrorCode::InvalidInput, "graphenedb_put_node: unknown exception");
  }
}

graphenedb_status graphenedb_inspect(graphenedb_handle* db, char** out_text) {
  if (!db || !out_text) return error_status(ErrorCode::InvalidInput, "db and out_text are required");
  *out_text = nullptr;
  try {
    std::string text;
    auto st = db->db.inspect(&text);
    if (!st) return from_status(st);
    *out_text = copy_string(text);
    return ok_status();
  } catch (const std::exception& e) {
    return exception_status("graphenedb_inspect", e);
  } catch (...) {
    return error_status(ErrorCode::InvalidInput, "graphenedb_inspect: unknown exception");
  }
}

graphenedb_status graphenedb_validate(graphenedb_handle* db, char** out_report) {
  if (!db || !out_report) return error_status(ErrorCode::InvalidInput, "db and out_report are required");
  *out_report = nullptr;
  try {
    std::string report;
    auto st = db->db.validate(&report);
    *out_report = copy_string(report);
    return from_status(st);
  } catch (const std::exception& e) {
    return exception_status("graphenedb_validate", e);
  } catch (...) {
    return error_status(ErrorCode::InvalidInput, "graphenedb_validate: unknown exception");
  }
}

void graphenedb_free_string(char* value) {
  delete[] value;
}

int graphenedb_status_ok(graphenedb_status status) {
  return status.code == static_cast<int>(ErrorCode::Ok);
}

void graphenedb_status_free(graphenedb_status status) {
  graphenedb_free_string(status.message);
}

} // extern "C"
