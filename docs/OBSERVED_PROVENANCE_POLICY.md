# Observed provenance policy (issue #135)

Observed and Discovered edges carry empirical authority: Empirical retrieval trusts them. Before this
policy, any caller of `put_edge`, `put_batch`, `put_extraction`, or the HTTP edge and extraction routes
could assert that authority just by writing it (or by omitting `origin`, which defaulted to Observed).

## What changed

`DBOptions::observed_provenance` selects who may assert empirical authority:

| Policy | Behaviour |
|---|---|
| `TrustedCallers` (library default) | Unchanged. In-process callers are trusted code and choose any origin. |
| `RequireAuthority` | Observed/Discovered edges without a matching `TrustedAuthority` are stored as `Hypothetical`. |

Under `RequireAuthority`:

- A demoted edge is stamped with `origin_requested` (what was asked for) and `origin_policy=untrusted_demoted_v1`.
  Those two metadata keys are reserved: caller-supplied values are removed, so a stamp cannot be forged.
- `TrustedAuthority` is an opaque capability from `GrapheneDB::issue_authority(key)`. The key is the
  application-held `DBOptions::authority_key`; it is compared in constant time, and nothing is issued when the
  key is empty or the policy is off. A capability from another database instance does not transfer.
- Set `authority` on an `EdgeInput`, a `BatchInput` (all edges) or an `ExtractionInput` (all relations).
- Origins without empirical authority (`Inferred`, `Reinforced`, `Hypothetical`) are not changed.
- Re-submitting a relation with a stronger origin is a conflicting duplicate, so replay cannot promote a demoted edge.

## HTTP server

`graphenedb_server` always runs in `RequireAuthority` mode with a random per-process key that no request can read.
Only requests authenticated by `GRAPHENEDB_API_KEY` / `--api-key` receive the authority. A server with no API key
authorizes everyone but authenticates nobody, so every Observed/Discovered claim on `/v1/edges`,
`/v1/edges/provenance` and extraction is demoted. `/v1/edges/provenance` reports `origin`, `origin_requested`
and `origin_demoted`.

## Route inventory

| Route | Status |
|---|---|
| `put_edge`, `put_batch`, `put_extraction` | Governed under `RequireAuthority` |
| HTTP `/v1/edges`, `/v1/edges/provenance`, extraction | Governed; authority only for API-key callers |
| HTTP bulk nodes | Nodes only, no edges |
| Generic relation ingestion | Hypothetical by default; `LegacyObservedExplicit` is demoted at the database boundary under `RequireAuthority` |
| CLI | Operator-run, runs with the default `TrustedCallers` policy |
| C API | Exposes no edge-writing functions |

## Not covered

- An API-key holder is trusted to assert Observed. An API key proves the caller, not the source or the claim.
- The library default is still `TrustedCallers`; embedding applications that accept untrusted input must opt in.
- Existing stored Observed records are not migrated or audited.
- Windows and macOS packages are not re-verified.
- Forged `source_id`, integrity hashes and fake family independence in evidence metadata are still caller-asserted.
