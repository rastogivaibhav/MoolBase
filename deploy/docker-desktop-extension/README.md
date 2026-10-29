# MoolBase Docker Desktop extension (quickstart prototype)

This first extension is intentionally **UI-only**. It shows reproducible local Docker instructions and links to the Evidence Lab, and does not claim to launch or manage MoolBase. The native engine remains a separate container from the root Dockerfile.

To build and install locally from the repository root, run:

```bash
docker build -t moolbase-extension:dev -f deploy/docker-desktop-extension/Dockerfile deploy/docker-desktop-extension
docker extension install moolbase-extension:dev
```

To uninstall: `docker extension rm moolbase-extension:dev`.

A later release can add controls through Docker Desktop Extension SDK; those controls must be validated on Docker Desktop (Windows/macOS/Linux where supported) before being labelled operational.
