# Bounded Dialectic Reasoning

## Status

This module is an experimental, deterministic reasoning layer over the
authoritative GrapheneDB embedded API. It implements the first safe vertical
slice of the Dialectic and HypoKosh architecture:

```text
bounded expansion
-> immutable bundle set
-> convergence
-> opposition
-> bounded re-expansion
-> structured synthesis
```

It does not implement autonomous discovery, external actions, durable
model-world writes, or an LLM critic. It does not change the durable format.

## Public API

Include:

```cpp
#include "graphene/dialectic.hpp"
```

Construct an engine over an open database:

```cpp
DialecticEngine engine(db);

DialecticOptions options;
options.mode = QueryMode::Balanced;
options.semantic_candidates = 12;
options.max_hops = 6;
options.max_paths = 32;
options.max_paths_per_root = 8;
options.max_visited_states = 20000;
options.max_opposition_rounds = 1;

DialecticResult result =
    engine.reason(query_vector, query_signature, options);
```

The existing Kosh adapter exposes the same boundary:

```cpp
KoshAdapter adapter(db);
DialecticResult result =
    adapter.retrieve_dialectic(query_vector, query_signature, options);
```

`DialecticResult` contains:

- the immutable initial `BundleSet`;
- the initial `ConvergedAnswer`;
- a deterministic `OppositionReport`;
- an optional bounded re-expanded bundle;
- the final convergence and opposition state;
- a structured synthesis with `supported`, `provisional`, `contested`, or
  `abstain` epistemic status;
- `durable_writes=false`.

## Reasoning mode contract

The embedded causal retrieval and dialectic module use the same policy:

| Mode | Allowed origins |
|---|---|
| Empirical | Observed and discovered; analogical roles are excluded |
| Balanced | Observed, discovered, inferred, and reinforced |
| Theoretical | All origins, including hypothetical and analogical |

Mode selection changes traversal eligibility. It never changes a stored edge's
origin or promotion status.

## Expansion

Expansion starts from bounded semantic candidates and walks incoming causal
edges toward root nodes. It preserves competing roots and multiple acyclic
paths subject to configured budgets.

Every path records:

- nodes and edges;
- root and semantic anchor;
- confidence product;
- contradiction and hypothetical flags;
- evidence references;
- provenance findings.
- any n-ary all-source requirements traversed through a hyperedge.

The engine rejects cycles per path and stops at hard internal caps even if a
caller supplies larger values.

## Provenance rules

The v0 opposition layer reports:

- observed or discovered edges without source evidence;
- inferred edges without `derived_from`;
- reinforced edges marked as discovered;
- compressed shortcuts without a mechanistic derivation.

Evidence is currently read from compatible metadata fields:

```text
source_id
evidence_ref
source
span
observed_at
derived_from
promotion_status
```

`graphene/epistemic.hpp` supplies reusable typed views and strict RFC3339
parsing. Malformed `observed_at`, `valid_from`, or `valid_until` values are
reported; malformed temporal validity is not silently admitted into a
dialectic path.

## Joint causality

`graphene/hyperedge.hpp` exposes `put_hyperedge()`. It validates at least two
unique, existing source nodes and one distinct existing target, then commits
all member edges through one `put_batch()` transaction.

The representation is compatible with storage format 2:

```text
hyperedge_id
hyperedge_group_id
hyperedge_semantics=all_sources
hyperedge_sources=<sorted comma-separated node IDs>
hyperedge_arity
```

Traversal groups those member edges and admits the causal step only when every
declared source/member is visible, temporally valid, and permitted by the
query mode. `JointRequirement` keeps the full source/member set in every
returned branch. A missing or disallowed member rejects the whole group.

## Convergence

Convergence selects a primary root and a bounded set of its strongest paths.
It does not mutate the input bundle. Every unselected path remains addressable
through `PathReference` in `discarded_paths`.

Confidence combines:

- mean path confidence;
- degeneracy;
- edge diversity;
- evidence coverage;
- contradiction penalty;
- provenance-risk penalty.

These weights are explicit v0 engineering defaults, not scientific constants.
They require held-out evaluation and calibration.

## Opposition

Opposition is deterministic. It challenges:

- contradiction paths;
- incomplete or unsafe provenance;
- single-path conclusions;
- competing causal roots;
- truncated expansion.

It emits falsification questions and nodes to reopen. When the opposition
score crosses the configured threshold, the controller performs a larger but
still bounded expansion. The module never calls an LLM or an external tool.

## Temporal behavior

`DialecticOptions::as_of` filters nodes and edges using typed `valid_from` and
`valid_until` metadata. Values must be RFC3339 timestamps with `Z` or an
explicit numeric offset. Fractional seconds up to nanosecond precision are
supported and offsets are normalized before comparison. Invalid query times
return an `INVALID_AS_OF` warning and no roots. Invalid stored intervals are
excluded and reported.

## Pilot HTTP endpoint

The optional POSIX pilot server exposes `POST /v1/reason/dialectic`. It is
authenticated by the same API-key policy as other non-probe endpoints and
runs inside the existing bounded worker pool, request-size limit, and rate
limiter. Request-specific candidate, path, hop, state, and opposition bounds
are clamped to hard server limits. The response includes bundles, provenance
findings, joint requirements, convergence, opposition, synthesis, and
`durable_writes=false`.

See `docs/api/openapi-v1.yaml` for the versioned request contract. The server
remains a controlled-pilot surface and must be placed behind the documented
TLS reverse proxy for non-loopback use.

## Safety properties

- No durable writes.
- No evidence deletion.
- No silent promotion.
- Deterministic ordering and tie-breaking.
- Bounded candidates, hops, paths, selected paths, visited states, and rounds.
- Cycle rejection.
- Snapshot-pinned reads.
- Existing WAL/checkpoint formats remain unchanged.

## Reproducible demonstration

```bash
cmake -S . -B build -DGRAPHENEDB_BUILD_EXAMPLES=ON
cmake --build build --target graphenedb_dialectic_reasoning
./build/graphenedb_dialectic_reasoning
```

Contract test:

```bash
ctest --test-dir build -R "graphenedb_(dialectic|hyperedge)_tests" --output-on-failure
```

Deterministic ablation:

```bash
cmake --build build --target graphenedb_dialectic_ablation_bench
./build/graphenedb_dialectic_ablation_bench 12 32
```

The ablation compares vector top-1, the existing causal bundle,
convergence-only reasoning, and the complete dialectic controller. It is a
synthetic regression fixture, not production efficacy evidence.

## Remaining work

Before enterprise use:

1. benchmark complete path recall against reference enumeration;
2. calibrate convergence and opposition on held-out organisational data;
3. add endpoint-level Linux integration and load evidence;
4. design model-world promotion APIs with explicit human authorization and a
   durable-format/migration decision before implementation;
5. run adversarial, fuzz, scale, 24-hour, and 72-hour gates.
