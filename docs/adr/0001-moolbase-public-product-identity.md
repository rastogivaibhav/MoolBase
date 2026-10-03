# ADR-0001: Public product identity — MoolBase by RASVAI

- **Status:** Accepted for public developer-preview use
- **Date:** 2026-09-23
- **Issue:** #34
- **Scope:** Public product naming and migration only
- **Scientific protocol impact:** None

## Context

The historical public name **GrapheneDB** collides with an established managed Neo4j service. That creates avoidable search, package, conference and market confusion before wider distribution.

The project also accumulated three research-facing names:

- GrapheneDB — persistent evidence/provenance substrate
- HypoKosh — competing-hypothesis runtime
- Dialectical Model Worlds (DWM) — challenge/reopen/synthesis loop

Those names are valuable as research lineage, but asking a new developer to learn all three before understanding the product increases cognitive and pronunciation friction.

## Decision

Adopt the following public naming architecture for the developer preview:

### Umbrella brand

**RASVAI**

RASVAI is the creator/lab/technology umbrella. It is not the database product name.

### Public product

**MoolBase**

Working public sentence:

> **MoolBase is the epistemic database for reasoning agents — storing not just what an agent knows, but the evidence, alternatives and revisions behind what it believes.**

Working short line:

> **Store the evidence. Preserve the alternatives. Know why belief changed.**

### Public capability names

Use plain technical English for the primary reasoning mechanisms:

- **Hypothesis Engine** — maintains and competes alternative explanations instead of collapsing uncertainty prematurely.
- **Dialectic Engine** — challenges the current belief, reopens evidence when warranted, and drives governed synthesis/revision.

Public conceptual flow:

```text
Evidence
  ↓
MoolBase
  ↓
Hypothesis Engine
  ↓
Dialectic Engine
  ↓
Decision / Revision
```

## Historical/research lineage

Do not rewrite history.

- **GrapheneDB → MoolBase** public product lineage
- **HypoKosh → Hypothesis Engine** research/implementation lineage
- **DWM → Dialectic Engine** research/implementation lineage

Existing commits, benchmark hashes, receipts, frozen protocols, papers, issue history and score-bearing artifacts retain their original names.

A migration note may describe the relationship prospectively, for example:

> GrapheneDB is the historical research and implementation name of the system publicly distributed as MoolBase by RASVAI.

## Why not Dwand / Sanvad as primary public module names

The Sanskrit/Hindi concepts fit the architecture semantically, but create avoidable pronunciation, spelling and recall friction for an international developer audience. They may remain inspiration or internal terminology, but are not required knowledge for installing or using MoolBase.

## Why not RASVAI as the database name

RASVAI is distinctive and useful as an umbrella brand, but does not inherently communicate a database/runtime product. **MoolBase** carries stronger infrastructure semantics while retaining a distinctive identity.

## First-pass collision screening

As of 2026-09-23:

- exact GitHub repository search for **MoolBase** returned no repositories;
- exact GitHub code search returned no matches;
- indexed public-web searches found no obvious active AI/database/software product using the exact name;
- indexed searches of npm, PyPI and crates.io found no obvious package using the exact name;
- indexed UK/US/WIPO trademark searches surfaced no obvious exact match.

This is **not** legal trademark clearance and does not establish registrability or freedom to operate. The project may use MoolBase as its open-source developer-preview identity while formal trademark/domain decisions remain a separate owner/legal matter.

## Migration constraints

1. Do not mass-rename C++ symbols, namespaces, benchmark folders or frozen artifacts.
2. Do not alter score-bearing benchmark semantics or hashes for naming reasons.
3. Keep compatibility aliases if source/API renaming is introduced later.
4. New public documentation should lead with **MoolBase by RASVAI** and explain GrapheneDB only as historical/compatibility lineage.
5. HypoKosh and DWM remain valid research names in historical and technical material.
6. Formal trademark registration, domain strategy and package-namespace expansion remain separate owner/legal decisions and are not implied by this ADR.

## Completion criteria for the repository naming migration

The repository-level migration is complete when:

- the GitHub repository and current documentation use MoolBase consistently;
- current release instructions point to `rastogivaibhav/MoolBase`;
- a compatibility note preserves GrapheneDB/HypoKosh/DWM lineage;
- frozen scientific artifacts remain unchanged.

Formal trademark/domain clearance is tracked separately and is not a prerequisite for preserving historical scientific names.

## Consequences

### Positive

- one memorable product name for developers;
- a clear database/infrastructure signal;
- simpler onboarding and technical storytelling;
- RASVAI can grow into an umbrella for future products without overloading this database identity;
- research provenance is preserved.

### Trade-offs

- GrapheneDB remains visible in historical commits and papers for some time;
- a compatibility/migration period is required;
- formal trademark/domain clearance remains a separate owner/legal matter.

## Decision update — 2026-10-03

The repository rename to `rastogivaibhav/MoolBase`, developer-preview release, public Evidence Lab and current documentation now use MoolBase as the selected public identity. Frozen research artifacts and compatibility APIs continue to retain their historical names. A current public-web search on 2026-10-03 found no obvious exact-name software/product collision; that search is not legal clearance.
