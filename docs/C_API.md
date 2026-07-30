# C API

GrapheneDB exposes a small C ABI for non-C++ callers and future language bindings.

Header:

```c
#include "graphene/c_api.h"
```

The current ABI intentionally covers a narrow controlled-pilot surface:

- open/close a database handle
- insert a simple node
- inspect database state
- validate database state
- free strings/status messages allocated by GrapheneDB

Example:

```c
graphenedb_handle* db = NULL;
graphenedb_status st = graphenedb_open("/tmp/gdb-c", 3, &db);
if (!graphenedb_status_ok(st)) {
  fprintf(stderr, "%s\n", st.message);
  graphenedb_status_free(st);
  return 1;
}

float vec[3] = {0.9f, 0.1f, 0.0f};
graphenedb_node_input node = {0};
node.content = "C API root memory";
node.vector = vec;
node.vector_len = 3;
node.root = 1;

uint32_t id = 0;
st = graphenedb_put_node(db, &node, &id);
graphenedb_status_free(st);

char* inspect = NULL;
st = graphenedb_inspect(db, &inspect);
if (graphenedb_status_ok(st)) {
  puts(inspect);
  graphenedb_free_string(inspect);
}
graphenedb_status_free(st);

graphenedb_close(db);
```

## Ownership

Strings returned through `char**` and `graphenedb_status.message` are allocated by GrapheneDB. Release them with:

```c
graphenedb_free_string(value);
graphenedb_status_free(status);
```

## Scope

The C ABI is not yet a full mirror of the C++ API. It is a stable bridge for embedding, smoke testing, and language-binding work. Lattice/extraction/batch APIs can be added as the downstream binding requirements become clear.
