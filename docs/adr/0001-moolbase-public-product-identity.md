# ADR-0001: Public product identity — MoolBase by RASVAI

- **Status:** Provisional
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

Adopt the following **provisional** public naming architecture:

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

> GrapheneDB is the historical research name of the system now being prepared for public distribution as MoolBase by RASVAI.

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

This is **not** legal trademark clearance and does not prove domain/package availability. Domain registry availability and formal trademark clearance must be confirmed before the first branded public release.

## Migration constraints

Until #34 is closed:

1. Do not mass-rename C++ symbols, namespaces, benchmark folders or frozen artifacts.
2. Do not alter score-bearing benchmark semantics or hashes for naming reasons.
3. Keep compatibility aliases when source/API renaming eventually begins.
4. Public documentation may introduce **MoolBase by RASVAI** prospectively once namespace/domain checks are complete.
5. Repository rename, package publication, SEO push and conference branding remain gated until the final namespace decision.
6. HypoKosh and DWM remain valid research names in historical and technical material.

## Exit criteria for the naming migration

#34 can close only when:

- MoolBase passes a deeper trademark/company/product collision review;
- primary domain strategy is selected and registered/controlled;
- GitHub/package namespaces needed for distribution are confirmed;
- repository description and current docs use one canonical public sentence;
- migration/compatibility note is published;
- no frozen scientific artifact was rewritten;
- broad distribution no longer risks confusion with the existing GrapheneDB service.

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
- the public product name remains provisional until domain and formal clearance gates are complete.
