# Agent Memory Is Not Enough

## What happens when an agent's evidence changes?

Long-running AI agents are becoming stateful systems.

They remember users, retrieve prior conversations, cache plans, maintain knowledge graphs and carry context across sessions. That is useful progress.

But persistent memory does not solve the harder problem.

**An agent can remember the wrong thing perfectly.**

Consider a simple customer-memory example.

An assistant has evidence that a customer should use **EU fulfilment**. Under the current policy, the evidence is sufficient, so the agent has an operative answer:

> Use EU fulfilment.

Later, a corrected profile supersedes the original preference.

A conventional memory implementation might simply overwrite the old value with the new value. Another might retrieve both values and ask the model to decide which one looks newer.

MoolBase takes a different approach.

The old commitment is cleared first.

For a period, the correct answer is:

> No operative answer yet.

Then independent evidence supporting **US fulfilment** arrives, known stale derived copies are retired, and the system can resolve again:

> Use US fulfilment.

The important output is not only that the answer changed.

It is that the system can show **why it changed, which evidence counted, which evidence did not count independently, what was superseded, and when the previous commitment stopped being justified**.

That is the problem MoolBase is designed to explore.

---

## Memory retrieval and belief state are different problems

A conventional memory system usually optimizes some combination of storage, semantic similarity, recency, importance, graph connectivity, summarization and retrieval relevance.

Those are useful capabilities.

But a long-running reasoning agent also needs to understand the **epistemic status** of what it retrieves.

Imagine three documents all support the same claim.

That may look like three pieces of evidence.

But what if all three copied the same original source?

Counting them independently creates false confidence.

Or imagine one hypothesis currently has more support than another.

Should the system erase the weaker hypothesis?

Not necessarily.

The weaker explanation may become the correct one when late evidence arrives.

Persistent agent state therefore needs more than retrieval. It needs to preserve how a belief was earned.

---

## Four things an agent should not collapse into one memory

A practical system often needs to distinguish:

1. **Evidence** — observations, documents, events or claims with provenance.
2. **Hypotheses** — competing explanations of that evidence.
3. **Decisions** — the current governed action or belief.
4. **Revisions** — why a previous decision was reopened or changed.

When these are stored as one undifferentiated memory, important information disappears.

A summary such as:

> Vendor A caused the outage.

may be operationally convenient.

But it does not tell us whether Vendor A was one of several plausible causes, whether its supporting alerts came from independent sources, whether contradictory evidence existed, whether the conclusion was provisional, whether it was later reopened, or what new evidence caused the revision.

For agents expected to operate over hours, days or months, that missing structure matters.

---

## What is actually inside MoolBase

The public story starts with evidence-aware memory, but the implementation is a broader stack:

- **MoolBase Core** persists evidence, vectors, provenance, causal relationships and versioned durable state.
- A **graphene-inspired dense hexagonal lattice topology** gives nodes durable `q/r/layer` coordinates, validated bonds and an optional lattice-aware retrieval path.
- **FiberBundle** builds a deterministic, hashable evidence projection that keeps path role, target, lineage and independent-support structure visible.
- **HypoKosh / Hypothesis Engine** preserves competing explanations and performs governed convergence or abstention.
- **DWM / Dialectic Engine** handles material opposition, challenge, bounded reopen/re-expansion and governed revision.
- **Epistemic receipts** preserve the externally inspectable result of that process without storing private model chain-of-thought.

The lattice is a database topology/retrieval primitive, not a physics simulation. The reasoning layers are optional: MoolBase can be used as an evidence/provenance store without adopting the full HypoKosh + DWM runtime.

## The MoolBase model

MoolBase treats persistent reasoning state as a data-system concern.

~~~mermaid
flowchart LR
    E["Evidence + provenance"] --> M["MoolBase persistent state"]
    M --> H["Competing hypotheses"]
    H --> D["Governed decision / abstention"]
    D --> C["Challenge / contradiction"]
    C --> Q{"Reopen needed?"}
    Q -->|Yes| X["Targeted evidence expansion"]
    X --> M
    Q -->|No| R["Terminal governed state"]
    R --> P["Inspectable receipt"]
    N["New evidence"] --> M
~~~

The public developer story is intentionally narrower than the entire research programme:

- **MoolBase** persists evidence, provenance and epistemic state.
- Optional reasoning layers preserve alternatives and challenge the current state.
- Contradiction can block a final commitment.
- New evidence can reopen a previous decision.
- A compact receipt makes the resulting state inspectable after the fact.

Historical implementation identifiers such as GrapheneDB, HypoKosh and DWM remain visible for compatibility.

---

## Copies do not become independent corroboration

One of the easiest ways for an agent to become confidently wrong is to count repeated evidence as independent confirmation.

Suppose an incident agent sees:

~~~text
Alert A: dependency X is failing
Alert B: dependency X is failing
Alert C: dependency X is failing
~~~

Three alerts sound stronger than one.

But if B and C are derivative copies of A, the independent evidence count may still be one.

The public MoolBase Evidence Lab deliberately demonstrates this.

A second graph-distinct alert can arrive while the number of independent evidence families stays:

~~~text
1 -> 1
~~~

Different wording or another event identifier does not make the underlying source independent.

That is why provenance is not merely an audit feature. It changes how much support the system should assign to a claim.

---

## Contradiction should be persistent state

Many AI systems treat contradiction as something the prompt should resolve.

A persistent reasoning substrate should treat contradiction as state.

For example:

~~~text
09:00  Evidence supports H1
09:05  H1 becomes the current decision
09:12  New evidence materially opposes H1
09:13  Final resolution is blocked
09:14  Targeted evidence is reopened
09:16  Discriminating evidence changes the governed state
~~~

The useful artifact is not only the final answer.

The useful artifact is the revision path.

That lets another engineer, agent or auditor understand what changed, what new evidence mattered, whether the contradiction was material, whether reopening was justified, and whether the system silently overwrote its previous state.

---

## A supported alternative is not automatically opposition

This sounds subtle, but it matters.

Suppose H1 is currently stronger and H2 remains a plausible alternative.

The existence of H2 should not automatically be treated as a dialectical attack on H1.

The current MoolBase flagship contract separates:

- **insufficient corroboration**, which can require further evidence; from
- **material opposition**, which can trigger a dialectical challenge and reopen.

That distinction prevents a system from manufacturing conflict merely because another supported path exists.

It also makes the receipt more meaningful: a challenge means actual material opposition was present, not simply that the system had more than one idea.

---

## Decision provenance is not chain-of-thought

MoolBase is not intended to persist private model chain-of-thought.

The goal is machine-readable decision provenance:

- evidence references;
- source and derivation lineage;
- competing-hypothesis state;
- contradiction state;
- governed status;
- selected paths;
- residual uncertainty;
- revision events;
- deterministic receipts.

That is a different abstraction.

It is closer to a durable transaction record for belief change than a transcript of hidden model reasoning.

---

## Try the actual customer-memory correction

The fastest way to understand the project is the public Evidence Lab:

https://moolbase.rasvai.com

The customer-memory scenario shows:

~~~text
EU fulfilment resolved
        |
corrected profile supersedes old preference
        |
no operative answer
        |
independent US support arrives
        |
known stale copies are retired
        |
US fulfilment resolved
~~~

The Lab runs the actual C++ database/reasoning engine in a browser worker. The browser session is local and disposable; the released native examples persist evidence on disk.

You can also reproduce the released workflow from the versioned examples in **v0.6.0-alpha.2**.

---

## Reproduce the current flagship mechanism

The project asks developers to test the mechanism rather than trust the positioning.

From a clean checkout:

~~~bash
git clone https://github.com/rastogivaibhav/MoolBase.git
cd MoolBase
python3 scripts/run_flagship_proof.py
~~~

No hosted model or API key is required.

Expected current mechanism receipt:

~~~text
12f2c843774027b33b2e81869fc24b232f849e81f84936d7d7b8b1be189fde89
~~~

The most useful next step is not another internal benchmark.

It is an external developer trying to break the assumptions.

Try duplicate evidence. Remove a decisive independent family. Inject contradiction. Reorder deterministic input. Restrict the search budget.

If the system silently promotes a claim, loses contradiction, fabricates independence or changes its receipt without explanation, that is useful evidence.

See the [independent reproduction guide](../INDEPENDENT_REPRODUCTION.md).

---

## What MoolBase is not claiming

MoolBase does not claim that structural consistency equals semantic truth, that a database can authenticate the real-world truth of its inputs, that every agent decision should be deterministic, that persistent memory eliminates hallucination, that one benchmark proves universal superiority, or that the current developer preview is enterprise GA.

The claim is narrower:

**when an application supplies evidence and provenance, MoolBase provides persistent structures for evidence state, competing explanations, contradiction, revision and inspectable receipts.**

That is the claim the public examples and flagship proof are meant to make testable.

---

## Why this may matter for agent infrastructure

The early infrastructure question was:

> How do I give the model context?

Then:

> How do I give the agent memory?

The next question may be:

> How do I maintain a persistent, inspectable belief state that can survive contradiction and revision?

That requires different primitives.

Not just vectors.

Not just retrieval.

Not just chat history.

Persistent evidence. Provenance. Alternatives. Contradiction. Revision. Receipts.

That is the space MoolBase is exploring.

---

**MoolBase by RASVAI**

*Store the evidence. Preserve the alternatives. Know why belief changed.*
