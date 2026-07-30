#include "graphene/c_api.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void require_status(graphenedb_status st, const char* what) {
  if (!graphenedb_status_ok(st)) {
    fprintf(stderr, "FAIL %s: %s\n", what, st.message ? st.message : "(no message)");
    graphenedb_status_free(st);
    abort();
  }
  graphenedb_status_free(st);
}

int main(void) {
#ifdef _WIN32
  (void)system("rmdir /s /q c_api_smoke_db >NUL 2>NUL");
#else
  (void)system("rm -rf c_api_smoke_db");
#endif
  graphenedb_handle* db = NULL;
  require_status(graphenedb_open("c_api_smoke_db", 3, &db), "open");
  assert(db != NULL);

  float root_vec[3] = {0.9f, 0.1f, 0.0f};
  graphenedb_node_input root;
  memset(&root, 0, sizeof(root));
  root.content = "C API root";
  root.vector = root_vec;
  root.vector_len = 3;
  root.signature = 1;
  root.incident = 7;
  root.root = 1;

  uint32_t id = 999;
  require_status(graphenedb_put_node(db, &root, &id), "put_node");
  assert(id == 0);

  char* inspect = NULL;
  require_status(graphenedb_inspect(db, &inspect), "inspect");
  assert(inspect != NULL);
  assert(strstr(inspect, "nodes_visible=1") != NULL);
  graphenedb_free_string(inspect);

  char* report = NULL;
  require_status(graphenedb_validate(db, &report), "validate");
  assert(report != NULL);
  assert(strstr(report, "OK") != NULL);
  graphenedb_free_string(report);

  require_status(graphenedb_close(db), "close");
#ifdef _WIN32
  (void)system("rmdir /s /q c_api_smoke_db >NUL 2>NUL");
#else
  (void)system("rm -rf c_api_smoke_db");
#endif
  printf("graphenedb_c_api_tests_passed=true\n");
  return 0;
}
