# Independent flagship reproduction

This is the shortest external path for testing the current GrapheneDB epistemic thesis.

You do **not** need to read the GrapheneDB, HypoKosh or DWM papers first.

## What this exercises

The reproduction pack runs the canonical flagship proof plus five pre-registered attacks against:

- evidence-family de-correlation;
- preservation of competing hypotheses;
- contradiction-aware non-convergence;
- bounded reopen/recovery;
- ingestion-order stability;
- explicit search-budget exhaustion;
- deterministic mechanism receipts.

GrapheneDB is the durable epistemic substrate. HypoKosh is the competing-hypothesis/recovery runtime. DWM is the bounded challenge/reopen/synthesis loop.

## Prerequisites

- Git
- Python 3
- CMake 3.16 or newer
- a C++20 compiler

No model API key, hosted LLM or external service is required after cloning the repository.

## One-command reproduction

```bash
git clone https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
python3 scripts/run_flagship_perturbations_v1.py
```

The perturbation runner first rebuilds and replays the canonical flagship. It refuses to interpret the attacks if the frozen flagship mechanism receipt has drifted.

## Expected canonical flagship mechanism receipt

```text
36ca5817494325870b81dbe96c261086c13ff09e040b7604242bcbf92d6dedef
```

## Expected perturbation mechanism receipts

```text
P1 f5e8579270e4325fb063aa60b0cad673d107979d6a8ca38ac8d52450e1d073f1
P2 bcc9a100e2bc07ce883639bb4dfba195ce8644c9f11bcb673a560e4550b9bcfd
P3 44fc5802e023c0c5d354e2dfc0c554a68e952648f24d559a71aa7f3c4ed405bd
P4 3dffe6e7bb944f6575b32a10e042eae728ad9cb0a51d27be05609b544f9cbfa1
P5 8d66770b62342b816d74dbad9dbfa532609ad0be3aaba173cdb8f90940d2f9ac
```

Mechanism hashes exclude the current commit identity. Each receipt records a separate provenance hash that binds the observed mechanism result to the tested source commit.

## Output

The command writes:

```text
reports/flagship-perturbations-v1/
  aggregate.json
  summary.md
  raw_output.txt
  canonical-baseline/
  receipts/
    P1.json
    P2.json
    P3.json
    P4.json
    P5.json
```

A red run is useful evidence. The runner writes all available receipts before returning failure for a contract violation.

## What the attacks mean

**P1 — duplicate-family injection**  
Three graph-distinct support routes from one evidence family must remain one independent family. The system must require external verification/reopen rather than treating duplication as corroboration.

**P2 — decisive-family removal**  
Removing one of two independent support families must remove sufficient corroboration and restore the external-verification/reopen requirement.

**P3 — material contradiction injection**  
A material contradiction must remain visible and block final resolution.

**P4 — ingestion-order permutation**  
The same semantic evidence inserted/presented in a different order must produce the same canonical bundle identity and epistemic outcome.

**P5 — reduced recovery budget**  
A one-cycle recovery budget must be visible in the trace. The runtime may stop unresolved, but it must not silently widen another search dimension to manufacture an answer.

## Report an independent reproduction

Use GitHub issue #38 or the **Independent reproduction report** issue template.

Please include:

- operating system;
- compiler and version;
- Python version;
- CMake version;
- exact commit;
- canonical flagship hash;
- P1-P5 hashes/results;
- whether the instructions worked without assistance;
- anything confusing or incorrect;
- any adversarial case that should be added.

Failed reproductions and criticism are explicitly welcome.

## Claim boundary

Passing this pack does not establish semantic truth, autonomous scientific discovery, automatic hidden-dependence discovery, durable cross-run DWM belief promotion, general superiority over other systems, or enterprise readiness.

The programme exit criteria remain governed by issue #25 and require independent usability, outside validation, and independent recognition/adoption in addition to internal evidence.
