# GDB-DH-AM-1 Foundation Attempt

| Field | Result |
|---|---|
| Benchmark | `GDB-DH-AM-1` |
| Decision level attempted | Foundation |
| Result | **INCOMPLETE** |
| Critical gates passed | `G1`, `G3` |
| Critical gates failed/incomplete | `G0`, `G2` |
| Strongest baseline | Not applicable to foundation correctness gates |
| Primary effect | Not applicable; blinded efficacy was not attempted |
| Safety exceptions | Zero observed false promotions; full `D0` not run |
| Scale actually tested | D1: 183 nodes, 432 edges, deterministic vectors; 37 release tests |
| Known limitations | Dirty worktree; no clean rerun; no 5,000-graph D0 harness |

## Decision

The repository does **not** yet earn the Foundation Pass because every gate
`G0` through `G3` is mandatory. This attempt passed the complete executable
`G1` durability/replay gate and the pinned `G3` public regression gate.

`G0` is mechanically healthy—37/37 CTest, OpenAPI parity, package consumption,
non-root image execution, and `git diff --check` all passed—but fails the
clean-checkout and pre-run manifest requirements.

`G2` remains the substantive missing test. The existing 12-case deterministic
ablation reported root hit rate `1.0`, contradiction and provenance challenge
rates `1.0`, and false-promotion rate `0.0`. It cannot substitute for the
preregistered `D0` suite of 5,000 graphs and 20,000 exact-truth queries.

## G3 result

The pinned `icco/postmortems` corpus at commit
`0ed8afb0f8cfd83a34bbda270943ebdbf9661062` produced:

- 176 usable records;
- 183 inserted nodes and 432 inserted edges;
- zero writes on same-process and post-restart replay;
- dialectic category micro-recall `1.000`;
- complete multi-root rate `1.000`;
- provenance-safe record rate `1.000`;
- false-promotion rate `0`;
- passing storage and provenance validation.

This is annotated-graph retrieval evidence, not autonomous root-cause
discovery.

## Required next action

Implement the deterministic `D0` generator and exact scorer, including all
required graph families, provenance/temporal/hyperedge/no-evidence quotas,
explicit seed, raw predictions, configured budgets, and non-zero exit on any
critical threshold failure. Then commit the current source and rerun `G0–G3`
from a clean checkout with the complete manifest created before execution.
