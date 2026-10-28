# MoolBase Docker distribution (developer preview)

This document prepares distribution; it does not assert that an image has been published or that the Docker Hub or Hugging Face applications are deployed.

## Local Docker Desktop / Docker Engine

```bash
cp .env.example .env
# Edit .env: set GRAPHENEDB_API_KEY to a strong random value from openssl rand -hex 32
docker compose build
docker compose up -d
docker compose ps
docker compose logs -f moolbase
```

On Windows PowerShell copy the file using `Copy-Item .env.example .env`. Create a strong key using `[Convert]::ToHexString([System.Security.Cryptography.RandomNumberGenerator]::GetBytes(32))` and place it in `.env` as `GRAPHENEDB_API_KEY=...`. The local `.env` is ignored by Git. Do not share this key.

**Security:** The native server explicitly refuses non-loopback binding without `GRAPHENEDB_API_KEY`. A pullable image without the key is expected to refuse to start rather than expose an unauthenticated API.

To run the released image without Compose, first set `GRAPHENEDB_API_KEY` in your terminal, then:

```bash
docker run -d --name moolbase --platform linux/amd64 \
  -e GRAPHENEDB_API_KEY \
  -p 127.0.0.1:8080:8080 \
  -v moolbase-data:/var/lib/graphenedb \
  vaibhavrastogi90/moolbase:0.6.0-audit.3
```

This versioned tag is proposed and must be confirmed on Docker Hub before using it.


The server listens on host loopback port 8080 by default (container port 8080). Storage is in the named Docker volume `moolbase-data`, mounted at `/var/lib/graphenedb`. The existing Dockerfile ships a native C++ executable, not an interactive browser frontend. Do not expose this evaluation server publicly without authentication, a TLS reverse proxy and resource controls. Verify API endpoints with the repo's actual server API documentation; do not assume an undocumented REST endpoint.

To use a published image instead of a local build, set `MOOLBASE_IMAGE=docker.io/vaibhavrastogi90/moolbase:<version>` in `.env` and run `docker compose up -d --no-build`. Do not use an unqualified `latest` for scientific evaluations.

## Docker Hub

The `docker-distribution.yml` workflow builds on pull requests and on master. Publication is **manual**, not automatic:
1. Configure GitHub Actions secrets `DOCKERHUB_USERNAME` and `DOCKERHUB_TOKEN` (Docker Hub access token with push permissions).
2. Run the **MoolBase container build and optional Docker Hub publish** workflow on the default branch, set `publish=true`, and specify a reviewed immutable version tag.
3. Check the build result and verify the digest, architecture variants and application smoke test before advertising the published image.

The current Dockerfile uses Debian amd64 runtime dynamic-library paths. The workflow therefore publishes **linux/amd64 only**. Before supporting ARM64, update the runtime dependency packaging and add architecture-specific build and runtime verification.

## Hugging Face Docker Space

Hugging Face builds the **Space repository**. It does not automatically clone GitHub. Create a Docker Space and place the MoolBase source and Dockerfile into its repository (or use a controlled sync workflow). The Space `README.md` front matter should contain:

```yaml
---
title: MoolBase Evidence Lab
emoji: 🧪
sdk: docker
app_port: 8080
---
```

The native image currently exposes the HTTP server on port 8080. It is **not** the Evidence Lab frontend, and publishing it directly may expose an unauthenticated API. For a public Space, build a separate authenticated or read-only experiment gateway, and a web frontend, before opening access. Persistent evidence requires a Storage Bucket or external persistent store; default Space filesystem changes are ephemeral. Do not publish private research fixtures or unreviewed records.

## Docker Desktop Extension

A Docker Desktop Extension is **not the same thing as** a regular container. It is a second image with Docker Extension SDK metadata and UI/backend integration. An extension should offer start/stop/status, persistent data location, open local Evidence Lab, and a cautious reset/export workflow. Keep this separate from the MoolBase engine image. New public Extension Marketplace submissions are currently paused; custom extensions remain locally installable.

## Release acceptance gates

- Native image builds with no unexplained failures.
- Healthcheck succeeds after launch.
- Persistent state survives restart.
- API can be exercised against a real, documented route.
- Regression fixtures retain their historical versions and immutable findings.
- No production/GA claims, no calibrated probability claims, and no unfounded autonomous inference claims.
- Only publish multiarchitecture tags after an arm64 build/test gate.
