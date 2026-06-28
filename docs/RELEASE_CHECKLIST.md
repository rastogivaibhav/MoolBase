# Release checklist

## Before publishing RC3

- [ ] Replace placeholder license if publishing externally.
- [ ] Run `./scripts/run_all_tests.sh`.
- [ ] Run `./scripts/run_sanitizers.sh`.
- [ ] Run `./scripts/run_graphene_uniqueness_demo.sh`.
- [ ] Run `./scripts/run_100k_stress.sh`.
- [ ] Review `docs/V1_RC_ACCEPTANCE_REPORT.md`.
- [ ] Review `docs/GA_READINESS_SCORECARD.md`.
- [ ] Confirm no build folders are packaged.

## Before v1.0 controlled pilot

- [ ] Run `./scripts/run_1m_stress.sh` on target hardware.
- [ ] Run `./scripts/run_crash_matrix.sh` on target OS.
- [ ] Run `RUNS=100000 ./scripts/run_fuzz_smoke.sh` or longer.
- [ ] Validate one live LLM-Kosh/KoshDB integration path.
- [ ] Add benchmark evidence for vector-only vs Graphene retrieval.

## Before enterprise GA

- [ ] 24-hour soak with preserved logs.
- [ ] Multi-hour fuzzing with corpus archive.
- [ ] Disk-pressure crash testing.
- [ ] Real 1M-node rich-vector/rich-edge graph test.
- [ ] Final license.
- [ ] Security review.
