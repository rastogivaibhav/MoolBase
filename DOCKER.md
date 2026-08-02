# GrapheneDB Docker quickstart

Run the packaged server and an end-to-end API contract:

```bash
bash scripts/docker/local_stack.sh smoke
```

Run the critical offline proof suite:

```bash
bash scripts/docker/local_stack.sh proof-smoke
```

Run the complete local alpha gate:

```bash
bash scripts/docker/local_stack.sh proof-full
```

Evidence archives are written to `docker-evidence/` and are bound to the Git commit supplied at image-build time.

See [`docs/DOCKER_LOCAL_PROOF.md`](docs/DOCKER_LOCAL_PROOF.md) for all modes, resource requirements, security boundaries, expected results and independent-validation procedure.
