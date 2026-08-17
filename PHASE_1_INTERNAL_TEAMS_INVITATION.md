# GrapheneDB Phase 1 Pilot: Internal Teams Invitation

**Subject:** Help us validate GrapheneDB v0.6.0-rc1 — Test, break, fix collaboratively

---

Dear [Team/Organization],

We're inviting your team to be an early tester of **GrapheneDB v0.6.0-rc1**, an embedded C++20 database for AI memory systems.

This is not a finished product. It's a **Phase 1 pilot** where we validate core claims through real-world usage. Your job: build with it, break it, and help us fix it.

## What is GrapheneDB?

An embedded library (not a server) for:
- Storing **durable AI memories** with causal relationships
- **Lattice-aware retrieval** that understands memory topology
- **Explainable reasoning** over knowledge graphs
- **Incident memory** (root cause → symptoms → impact)
- **Extraction ingestion** for incident/research data

Think: "A database that understands how memories relate to each other and can reason about them."

## Why We Need You

To answer critical questions:
- ✅ Does it actually prevent data loss?
- ✅ Do causal queries return the right answers?
- ✅ Can it handle your data scale?
- ✅ Is the API intuitive enough to use?
- ❌ Where does it break?

**This is collaborative.** When something breaks, we triage together and fix it fast. You provide feedback, we iterate.

## What You'll Do

1. **Clone and build** (~5 min to get running)
2. **Run the test workbook** (10 concrete tests, ~2-3 hours total)
   - Durability: Does data survive crashes?
   - Causal retrieval: Do dependency chains work?
   - Vector search: Is recall good enough?
   - Scale: Can it handle 100k+ nodes?
   - Extraction: Do incident chains preserve metadata?
3. **Report findings** (GitHub Issues, weekly sync calls)
4. **Iterate with us** as we fix issues

**Estimated time commitment:** 4 weeks, 2-3 hours per week

## What Success Looks Like

Phase 1 is successful if:
- [ ] Your team can store 100k+ nodes without crashes
- [ ] Data persists across process restarts (durability holds)
- [ ] Causal queries produce expected results
- [ ] Vector search recall >= 95%
- [ ] No silent data corruption discovered
- [ ] You feel confident in core promises

If we find critical issues (data loss, crashes), we fix them together before moving forward.

## Getting Started

**1. Read the quickstart** (10 min)
→ `PHASE_1_TEAM_HANDOFF.md` in the repo

**2. Clone and build** (10 min)
```bash
git clone https://github.com/rastogivaibhav/graphenedb_v1.git
cd graphenedb_v1
./scripts/build.sh  # or PowerShell on Windows
```

**3. Run 5-minute smoke test**
```bash
./build/graphenedb_cli init /tmp/test.db 768
./build/graphenedb_cli put-node /tmp/test.db 768 "hello" "0.1,0.2,0.3" 123
./build/graphenedb_cli inspect /tmp/test.db 768
```

**4. Work through the test workbook** (Week 1-4)
→ `PHASE_1_TESTING_WORKBOOK.md`

**5. Report issues**
- GitHub: Create issue tagged `phase-1-pilot`
- Slack: Direct message for urgent issues
- Weekly sync: Thursdays to discuss findings

## Documentation

| Document | Purpose |
|----------|---------|
| [PHASE_1_TEAM_HANDOFF.md](./PHASE_1_TEAM_HANDOFF.md) | **START HERE** — Quick overview + known limitations |
| [PHASE_1_TESTING_WORKBOOK.md](./PHASE_1_TESTING_WORKBOOK.md) | 10 tests to run, checkboxes for validation |
| [PHASE_1_PILOT_CHARTER.md](./PHASE_1_PILOT_CHARTER.md) | Full objectives, timeline, success criteria |
| [QUICK_START_GUIDE.md](./docs/QUICK_START_GUIDE.md) | Step-by-step to store and retrieve your first memory |
| [WIDER_SHARING_READINESS.md](./WIDER_SHARING_READINESS.md) | What works, what doesn't, known limitations |
| [ARCHITECTURE.md](./docs/ARCHITECTURE.md) | Under the hood for curious minds |

## Known Limitations (Be Aware)

- ❌ **Not production-ready** — This is a developer preview
- ❌ **No server mode** — Embedded library only
- ❌ **No replication** — Single-machine operation
- ❌ **Windows server not tested** — CLI works, full server mode TBD
- ⚠️ **No backward compatibility** — v0.6.0-rc1 may not support older databases

[Full limitations in WIDER_SHARING_READINESS.md]

## Support & Issue Escalation

**Quick questions:** Slack DM  
**Bugs/crashes:** GitHub Issue (tag: `phase-1-pilot`) + Slack  
**Tracking issues:** Weekly 30-min sync, Thursdays 2pm UTC  

**Response SLA:**
- Critical (data loss, crashes): 24 hours
- High (API/docs don't match): 1 week
- Medium (performance, nice-to-have): next release

## Timeline

| When | What |
|------|------|
| **This week** | Team signup + setup |
| **Week 1** | Initial testing (Test 1-3: durability, causal retrieval) |
| **Week 2-3** | Scale testing (Test 4-7: vector search, extraction, 100k nodes) |
| **Week 4** | Final validation + Phase 2 decision (go/no-go for wider rollout) |

---

## Why This Matters

We're building a database that **AI teams can trust**. That requires:
1. Honest about what it does and doesn't do
2. Finding real problems early (before wider use)
3. Fixing problems collaboratively
4. Learning from how teams actually use it

**You're helping us get to v0.6.0 GA.** Your feedback shapes what GrapheneDB becomes.

---

## Ready?

**→ Start here:** Clone the repo and read `PHASE_1_TEAM_HANDOFF.md`

**Questions before you start?** Reply to this message or ping us on Slack.

**Let's build something durable together.**

---

**Contacts:**
- Technical questions: [Your Slack/email]
- Issue tracking: GitHub Issues (phase-1-pilot tag)
- Weekly sync: [Calendar invite link]

---

## FAQ

**Q: What if we find a critical bug?**  
A: We triage same day, fix within 24 hours if it's data loss or crash. You test the fix. We iterate.

**Q: Can we use this in production after Phase 1?**  
A: Maybe. Depends on what we find and how confident you are. Phase 1 is validation, not a promise.

**Q: What if our team can't commit 2-3 hours per week?**  
A: That's OK—we'll find teams who can. But more testers = more confidence in the result.

**Q: Where's the enterprise version / server mode?**  
A: Embedded library only for v0.6.0-rc1. Server mode coming post-GA. See `WIDER_SHARING_READINESS.md`.

**Q: Can we modify the code?**  
A: Yes. It's open source. We appreciate PRs for fixes. Major changes: discuss first.

---

**Version:** v0.6.0-rc1  
**Release Date:** August 2026  
**Status:** Developer Preview — Phase 1 Pilot
