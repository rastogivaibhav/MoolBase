# Experiment 3 Artifact Manifest

Local validated artifact hashes for the 2026-08-09 Experiment-3 run:

```text
arc_agi3_epistemic_agent_exp3_raw_ls20.zip
57d40a60c6cbc5c999e825b9d5f81714145a0b2edb7d775787a6521583ddf9d3

agent/my_agent.py
91e4d245a43245bed64ac58bc9dd542864233adc828397940d00f2bb560857e4

notebooks/submission.ipynb
fddea41a38292190321ab05b7a9ade6a3c3ee451522f51637181c3bca7b204b3

reports/EXPERIMENT_3_RAW_LS20_PROXY_ABLATION.json
7f24a50b71d27a135912aa8f215ceddc0a2e3d029d18104e7b031fc383e51f2e
```

Validation commands:

```text
python -m pytest -q
python tests/run_experiment3_ls20_raw_proxy.py
python tests/run_experiment3_ablation.py
python scripts/build_notebook.py
```

Observed gates:

- unit/regression suite: 19 passed;
- representative raw-frame proxy run: solved, 20 actions;
- 20-seed full Experiment-3 run: 20/20 solved, mean 17.6 actions, min 14, max 20;
- no-affordance ablation: 0/20 solved within 80 actions;
- state_then_contact goal learned: 20/20 full runs;
- silent goal promotion: 0 runs.

Claim boundary: source-derived LS20 Level-1 proxy mechanism evidence only; not an official ARC-AGI-3 score or full LS20 solve.
