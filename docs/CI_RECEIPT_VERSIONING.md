# CI receipt versions and frozen campaigns

The flagship V1 manifest and runner remain immutable. They describe canonical parent `51b2908f869547617283ccd8c12ea0520026810d` and receipt hash `36ca5817494325870b81dbe96c261086c13ff09e040b7604242bcbf92d6dedef`. Reproduce that historical protocol from its recorded source, not a newer engine checkout.

Commit `e7465b09f59c50d836a462601cca998d4adac25e` froze the V3-aligned flagship receipt `12f2c843774027b33b2e81869fc24b232f849e81f84936d7d7b8b1be189fde89`. Commit `1dbb497` preregistered `benchmarks/flagship/perturbations_v2.json` for that canonical parent, but the V2 runner was missing and current CI still invoked V1. The new V2 runner uses the existing V2 contract, checks its complete file digest, and writes separately labelled V2 evidence. It distinguishes corroboration search from dialectical challenge exactly as preregistered. No historical hash or expected outcome was edited.

Current-engine perturbation and pre-freeze CI invoke V2. V1 artifacts, manifests, runner and frozen scoring contracts are unchanged. A V2 failure remains a failure; every executed perturbation receipt, including failed cases, is written before the runner returns nonzero. No scientific score is computed by this repair.

Cycle 1 and Cycle 5 production-immutability guards apply when the PR modifies their campaign inputs or runner, and on manual historical campaign dispatch. A CMake-only test registration is not a campaign change. Workflow maintenance can still run inherited current-engine contracts without claiming that it reproduced the frozen campaign. Cycle 5's frozen input digest checks always run. The guard itself still rejects production changes accompanying campaign changes.

Reproduce the current perturbation check:

```sh
python3 scripts/test_flagship_perturbations_v2.py
python3 scripts/run_flagship_perturbations_v2.py
```

CI must pass on the exact PR head before merging. Existing versioned research evidence does not automatically establish accuracy or performance claims for a new engine revision.

The Cycle 1 probe intentionally demonstrates the old lower-node-ID tie bias. Later Cycle 4 changes repaired that behavior. CI therefore builds this historical diagnostic from its original commit `094db8715ab820fb18d86723e09a01088425fded`, records its separate head/tree identity, and still runs inherited production contracts against the current PR engine. Historical defect reproduction is not an expected defect in the current engine.
