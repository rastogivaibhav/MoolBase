# MoolBase Launch Pack v1

## Launch objective

Move MoolBase from an internally validated developer preview to a project that external developers can discover, understand, run and challenge.

The launch is not a claim of universal superiority. It is an invitation to test a specific developer problem:

**How should a long-running agent change its answer when the evidence that justified the old answer changes?**

## Message hierarchy

### One-line description

**Evidence-aware memory for agents whose answers must change when the evidence changes.**

### One-sentence explanation

MoolBase combines durable evidence/provenance storage, a graphene-inspired dense hexagonal lattice, deterministic FiberBundle evidence projections, HypoKosh competing-hypothesis reasoning, DWM dialectical challenge/reopen and inspectable receipts so an application can show why an agent's current answer is justified, when it stops being justified and what caused it to change.

### Technical stack to preserve in launch messaging

When space allows, describe the architecture in this order:

1. **MoolBase Core** — durable evidence/provenance database and causal-memory substrate.
2. **Dense hexagonal lattice topology** — durable axial coordinates, validated bonds and optional lattice-aware retrieval.
3. **FiberBundle** — deterministic, hashable target/role/lineage-aware evidence projection.
4. **HypoKosh / Hypothesis Engine** — competing hypotheses plus governed convergence/abstention.
5. **DWM / Dialectic Engine** — material opposition, challenge, bounded reopen/re-expansion and revision.
6. **Epistemic receipts** — inspectable machine-readable decision provenance.

Do not imply that every user must adopt every layer, and do not describe the lattice as a material-science simulation.

### The canonical demo

Use the public Evidence Lab customer-memory correction:

~~~text
EU fulfilment resolved
        |
old preference is superseded
        |
no operative answer
        |
independent US evidence arrives
        |
known stale copies are retired
        |
US fulfilment resolved
~~~

The key message is not merely that the answer changes. It is that the transition is explicit and inspectable.

### The canonical proof

From a clean checkout:

~~~bash
git clone https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
python3 scripts/run_flagship_proof.py
~~~

Expected mechanism receipt:

~~~text
12f2c843774027b33b2e81869fc24b232f849e81f84936d7d7b8b1be189fde89
~~~

No hosted model or API key is required.

## Primary links

- Repository: https://github.com/rastogivaibhav/MoolBase
- Evidence Lab: https://moolbase-evidence-lab.vaibhav-rastogi90.chatgpt.site
- Developer preview: https://github.com/rastogivaibhav/MoolBase/releases/tag/v0.6.0-alpha.2
- Canonical article: ../articles/agent-memory-is-not-enough.md
- Independent reproduction: ../INDEPENDENT_REPRODUCTION.md
- Public reproduction issue: https://github.com/rastogivaibhav/MoolBase/issues/38

## Existing launch assets

### Asset 1 — Evidence Lab replay

Use:

docs/images/evidence-lab-memory.gif

This is the preferred visual asset because it shows the actual Evidence Lab and actual engine path. Do not replace it with generic AI-generated imagery.

### Asset 2 — customer-memory state transition

~~~text
Resolved: EU
    |
supersession
    |
Open: no operative answer
    |
independent corrected evidence
    |
Resolved: US
~~~

Use this in text-first channels or recreate it as a simple technical diagram.

### Asset 3 — architecture diagram

Reuse the Mermaid diagram from the README or the canonical article. Keep it secondary to the customer-memory demo.

## LinkedIn launch post

I have been working on a problem that becomes more important as AI agents become long-lived:

**Memory is not enough when the evidence behind the memory changes.**

Imagine an agent has enough evidence to use EU fulfilment for a customer.

Later, a corrected profile supersedes that preference.

Should the agent immediately overwrite EU with US? Should it keep retrieving both and ask an LLM to decide? Or should it first recognise that the old commitment is no longer justified?

That is the problem I have been exploring with **MoolBase**.

In the public demo the state moves through:

EU resolved -> no operative answer -> US resolved.

The interesting part is not the final US answer. It is that the system preserves why the old answer stopped being justified, which evidence is actually independent, what was superseded, and why the new answer eventually earned support.

MoolBase is an experimental evidence-aware memory and persistent reasoning substrate for agents. Underneath that story are its durable C++ core, graphene-inspired dense hexagonal lattice, deterministic FiberBundle evidence projections, HypoKosh/Hypothesis Engine, DWM/Dialectic Engine, and inspectable epistemic receipts.

I have released a developer preview and a public Evidence Lab.

I would especially value engineers trying to break the assumptions rather than simply starring the repository. The current flagship mechanism can be reproduced locally without a model API key.

Repository: https://github.com/rastogivaibhav/MoolBase

Evidence Lab: https://moolbase-evidence-lab.vaibhav-rastogi90.chatgpt.site

If duplicate evidence can manufacture confidence, contradiction disappears, or a belief changes without an explainable receipt, I want to know.

#AIAgents #AgenticAI #AIInfrastructure #Databases #AIMemory #OpenSource #LLMOps

## Show HN

### Title

Show HN: MoolBase – evidence-aware memory for agents whose answers need to change

### Body

I built MoolBase to explore a problem I kept running into with long-lived agents: storing memory is much easier than maintaining the justification for a belief when evidence changes.

The simplest demo is customer memory.

The system initially has enough support for EU fulfilment. A corrected profile later supersedes the old preference, which clears the operative answer. Only after independent corrected evidence arrives and stale derived copies are retired does it resolve to US fulfilment.

So the state is:

EU resolved -> no operative answer -> US resolved.

The project tries to persist evidence/provenance, competing explanations, contradiction, revisions and compact decision receipts rather than treating all remembered state as one blob.

It is C++20 with released Python/HTTP examples and a browser Evidence Lab.

Repo:
https://github.com/rastogivaibhav/MoolBase

Evidence Lab:
https://moolbase-evidence-lab.vaibhav-rastogi90.chatgpt.site

The current flagship proof is deterministic and does not require an LLM/API key:

~~~bash
python3 scripts/run_flagship_proof.py
~~~

I am particularly interested in counterexamples: duplicate/correlated evidence being counted independently, contradiction being lost, order-dependent results, unsupported reopen, or unexplained receipt drift.

Current boundary: developer preview, not semantic truth and not enterprise GA.

## Reddit technical post

### Suggested title

I built an evidence-aware memory layer for agents because storing memory is not the same as maintaining a justified belief

### Body

A question I have been exploring is what happens to a long-lived agent when its stored information is not merely updated, but **invalidates the evidence that justified an earlier answer**.

A small example:

1. an agent has enough evidence to use EU fulfilment;
2. a corrected profile supersedes the original preference;
3. the previous commitment is cleared;
4. the state is temporarily unresolved;
5. independent corrected evidence arrives;
6. known stale copies are retired;
7. the agent can now resolve to US fulfilment.

So the state becomes:

EU resolved -> no operative answer -> US resolved.

That seems different from ordinary retrieval/memory. The system needs to preserve provenance, correlated-vs-independent evidence, alternatives, contradiction and the reason for revision.

I have been building this as an open-source experimental database/runtime called MoolBase.

Repo:
https://github.com/rastogivaibhav/MoolBase

Public Evidence Lab:
https://moolbase-evidence-lab.vaibhav-rastogi90.chatgpt.site

There is also a deterministic flagship proof that runs without an LLM/API key.

I am not claiming semantic truth or universal benchmark superiority. I am looking for criticism of the underlying abstraction.

In particular, I would value cases where:
- correlated evidence is accidentally counted as independent;
- contradiction is lost;
- a supported alternative is incorrectly treated as material opposition;
- input ordering changes the epistemic outcome;
- search limits cause hidden widening;
- a receipt changes without an explainable reason.

Would you model this as database state, an agent-runtime concern, event sourcing, a knowledge graph problem, or something else?

## X / short-thread launch

### Post 1

Agent memory solves: "what should I retrieve later?"

It does not automatically solve: "why should I still believe this after the evidence changes?"

That is the problem I built MoolBase to explore.

https://github.com/rastogivaibhav/MoolBase

### Post 2

The simplest demo:

EU fulfilment resolved
-> old preference superseded
-> no operative answer
-> independent corrected evidence
-> stale copies retired
-> US fulfilment resolved

The transition matters as much as the final answer.

### Post 3

MoolBase persists evidence + provenance, alternatives, contradiction, revisions and inspectable receipts.

A copied signal should not magically become independent corroboration.

A plausible alternative should not automatically become a dialectical attack.

### Post 4

The current flagship mechanism is reproducible without an LLM/API key:

~~~bash
python3 scripts/run_flagship_proof.py
~~~

I am explicitly looking for counterexamples and failure cases.

### Post 5

Public Evidence Lab:
https://moolbase-evidence-lab.vaibhav-rastogi90.chatgpt.site

If you can make it silently promote a claim, lose contradiction, fabricate independence or drift a receipt without explanation, please open an issue.

## Direct technical outreach

### Short DM

Hi — I have been working on an open-source project called MoolBase around a specific agent-infrastructure problem: maintaining evidence/provenance and explainable belief revision when a long-running agent's evidence changes.

The quickest example is EU fulfilment -> no operative answer after supersession -> US fulfilment after independent corrected evidence.

I am not looking for a promotional endorsement. I would value a technical counterexample or your view on whether this belongs at the database/runtime layer.

Repo: https://github.com/rastogivaibhav/MoolBase

Evidence Lab: https://moolbase-evidence-lab.vaibhav-rastogi90.chatgpt.site

## Distribution rules

- Use the maintainer/founder identity. Do not create fake users, fake testimonials or synthetic community consensus.
- Adapt each post to the norms of the community rather than cross-posting identical marketing copy.
- Lead with the technical problem and a reproducible example, not claims of being revolutionary, best or first.
- Invite criticism and preserve negative results publicly.
- Do not present historical benchmark scores as current product superiority.
- Link to the Evidence Lab only when it helps the reader understand the mechanism.
- Prefer one useful technical reply to many low-information promotional replies.

## 14-day execution sequence

### Days 0–2 — launch surface

- merge the corrected public proof/reproduction path;
- fix GitHub repository description/homepage/topics;
- fix the published v0.6.0-alpha.2 release body clone command;
- verify the README, article, Evidence Lab and reproduction issue all point to the same newcomer path;
- publish the canonical article.

### Days 2–4 — first distribution

- LinkedIn founder launch;
- one X thread;
- Show HN submission;
- one relevant Reddit technical post after checking community rules;
- reuse the actual Evidence Lab replay GIF.

### Days 4–7 — external validation

- invite 10–15 relevant engineers to reproduce or challenge the mechanism;
- respond to technical questions publicly where possible;
- convert genuine confusion into documentation fixes;
- label independent reproductions and counterexamples.

### Days 7–10 — ecosystem proof

Build only two small integration examples based on demand, preferably from:
- LangGraph;
- Google ADK;
- OpenAI Agents SDK.

Do not build all integrations speculatively.

### Days 10–14 — evidence review

Review:
- repository visitors and unique cloners;
- release asset downloads;
- Evidence Lab usage if available;
- stars/forks only as secondary signals;
- external reproduction attempts;
- issues/counterexamples;
- inbound technical conversations;
- evidence of anyone integrating MoolBase into another project.

Decide the next engineering work from external friction, not from an internally invented roadmap.

## Launch success signals

The first launch is working if at least some of the following occur:

- an unrelated engineer can explain the EU -> open -> US transition correctly;
- an unrelated engineer runs the flagship proof without founder assistance;
- at least one external counterexample/reproduction is submitted;
- someone asks how to integrate MoolBase into an existing agent stack;
- an external project references or experiments with MoolBase;
- technical discussion centres on the mechanism rather than confusion about what the product is.

## Stop conditions

Do not reopen a broad research/development programme merely because launch numbers are low.

Classify the problem first:

- people do not click -> distribution problem;
- people click but do not understand -> positioning problem;
- people understand but do not try -> relevance/problem-selection issue;
- people try but cannot install -> developer-experience issue;
- people install but hit a capability gap -> engineering issue;
- people reproduce and find a scientific failure -> research issue.

Only the last three justify returning immediately to code.
