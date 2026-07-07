# Release checklist

## Before publishing an RC

- [ ] Replace placeholder license if publishing externally.
- [ ] Run `./scripts/run_all_tests.sh`.
- [ ] Run `./scripts/run_sanitizers.sh`.
- [ ] Run `./scripts/run_graphene_uniqueness_demo.sh`.
- [ ] Run `.\scripts\run_operator_flow.ps1` or `scripts/run_operator_flow.sh`.
- [ ] Run `.\scripts\run_recovery_rehearsal.ps1` or `scripts/run_recovery_rehearsal.sh`.
- [ ] Run `.\scripts\run_kosh_adapter_gate.ps1` or `scripts/run_kosh_adapter_gate.sh`.
- [ ] Run `.\scripts\run_vector_baseline_bench.ps1` or `scripts/run_vector_baseline_bench.sh`.
- [ ] Run `.\scripts\run_vector_index_recall_bench.ps1` or `scripts/run_vector_index_recall_bench.sh`.
- [ ] Run `.\scripts\run_100k_stress.ps1` or `./scripts/run_100k_stress.sh`.
- [ ] Run `.\scripts\run_preview_hardware_profile.ps1` or `scripts/run_preview_hardware_profile.sh` and preserve `reports/preview-hardware/<timestamp>/PREVIEW_HARDWARE_SUMMARY.md`.
- [ ] On POSIX, run `graphenedb_real_filesystem_failure_tests` as a non-root user.
- [ ] On POSIX, run `graphenedb_disk_pressure_tests` as a non-root user.
- [ ] Run package install verification: `scripts/verify_package_install.sh` or `.\scripts\verify_package_install.ps1`.
- [ ] Run GA readiness bundle: `scripts/run_ga_readiness.sh` or `.\scripts\run_ga_readiness.ps1`.
- [ ] Build release install archive: `scripts/package_release_install.sh` or `.\scripts\package_release_install.ps1`.
- [ ] Preserve and review package `.sha256` and `.manifest.json` artifacts.
- [ ] Collect evidence bundle: `scripts/collect_ga_evidence.sh` or `.\scripts\collect_ga_evidence.ps1 -Archive`.
- [ ] Generate GA status report: `scripts/write_ga_status_report.sh` or `.\scripts\write_ga_status_report.ps1`.
- [ ] Optionally build one review bundle: `scripts/run_release_candidate_bundle.sh` or `.\scripts\run_release_candidate_bundle.ps1`.
- [ ] Preserve CI run URL and uploaded GA readiness artifact.
- [ ] Review `docs/V1_RC_ACCEPTANCE_REPORT.md`.
- [ ] Review `docs/GA_READINESS_SCORECARD.md`.
- [ ] Review `docs/NEXT_GA_EXECUTION_PLAN.md`.
- [ ] For the review bundle, use the labeled `release-candidate-smoke` invocation and preserve the backend selections in the archive metadata.
- [ ] Confirm no build folders are packaged.

## Before v1.0 controlled pilot

- [ ] Run `./scripts/run_1m_stress.sh` on target hardware.
- [ ] Run `.\scripts\run_target_scale_enterprise_profile.ps1` or `scripts/run_target_scale_enterprise_profile.sh` on the approved host for the final rich-workload scale evidence bundle.
- [ ] Run `.\scripts\run_enterprise_ga_campaign.ps1` or `scripts/run_enterprise_ga_campaign.sh` and preserve `reports/enterprise-ga/<timestamp>/ENTERPRISE_GA_SUMMARY.md`.
- [ ] Run `./scripts/run_crash_matrix.sh` on target OS.
- [ ] Run `RUNS=100000 ./scripts/run_fuzz_smoke.sh` or longer.
- [ ] Validate one live LLM-Kosh/KoshDB integration path.
- [ ] Publish benchmark evidence for vector-only vs Graphene retrieval on target developer-preview hardware.

## Before enterprise GA

- [ ] 24-hour soak with preserved logs.
- [ ] Multi-hour fuzzing with corpus archive.
- [ ] Disk-pressure crash testing.
- [ ] Real 1M-node rich-vector/rich-edge graph test.
- [ ] Preserve `reports/enterprise-ga/<timestamp>/ENTERPRISE_GA_SUMMARY.md` and companion logs for the release.
- [ ] Preserve `reports/ga-readiness/<timestamp>/GA_READINESS_SUMMARY.md` and logs for the release.
- [ ] Final license.
- [ ] Release signing, once signing infrastructure exists.
- [ ] Verify CMake package consumption on every supported platform.
- [ ] Security review.
