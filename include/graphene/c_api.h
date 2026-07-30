#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#  ifdef GRAPHENEDB_BUILDING_DLL
#    define GRAPHENEDB_C_API __declspec(dllexport)
#  else
#    define GRAPHENEDB_C_API
#  endif
#else
#  define GRAPHENEDB_C_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct graphenedb_handle graphenedb_handle;

typedef struct graphenedb_status {
  int code;
  char* message;
} graphenedb_status;

typedef struct graphenedb_node_input {
  const char* content;
  const float* vector;
  size_t vector_len;
  uint64_t signature;
  uint32_t incident;
  int root;
  int symptom;
  int impact;
} graphenedb_node_input;

GRAPHENEDB_C_API graphenedb_status graphenedb_open(const char* path, uint32_t dimension, graphenedb_handle** out);
GRAPHENEDB_C_API graphenedb_status graphenedb_close(graphenedb_handle* db);
GRAPHENEDB_C_API graphenedb_status graphenedb_put_node(graphenedb_handle* db, const graphenedb_node_input* input, uint32_t* out_id);
GRAPHENEDB_C_API graphenedb_status graphenedb_inspect(graphenedb_handle* db, char** out_text);
GRAPHENEDB_C_API graphenedb_status graphenedb_validate(graphenedb_handle* db, char** out_report);
GRAPHENEDB_C_API void graphenedb_free_string(char* value);
GRAPHENEDB_C_API int graphenedb_status_ok(graphenedb_status status);
GRAPHENEDB_C_API void graphenedb_status_free(graphenedb_status status);

#ifdef __cplusplus
}
#endif
