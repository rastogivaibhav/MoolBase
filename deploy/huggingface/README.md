---
title: MoolBase Native Research Server
emoji: 🔬
sdk: docker
app_port: 8080
pinned: false
---

# MoolBase native research server

This template is for a **controlled or private** Docker Space, not an unauthenticated public deployment. It runs the actual native MoolBase server using the root `Dockerfile` from the GitHub source repository. It is not a browser Evidence Lab UI.

## Creation

1. Create a Docker Space with the Hugging Face web UI.
2. Copy the entire reviewed MoolBase source tree into the Space repository; put this file at the Space root as `README.md`, and keep the root `Dockerfile`.
3. Set the Space to **Private** until an authenticated, read-only gateway is integrated.
4. Mount durable storage using a supported persistent storage integration; the Space filesystem can be replaced on rebuild or restart.
5. Validate service health and the engine's documented API before making any public launch decision.

Do not copy secrets, private evidence, or unreviewed evaluation fixtures into the Space.
