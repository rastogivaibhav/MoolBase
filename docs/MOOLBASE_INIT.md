# MoolBase first-run setup

The optional `moolbase init` command configures and starts the **real native C++ MoolBase server** inside Docker. This is an evaluation installer, not a hosted service or a browser dashboard.

## Installation (once this branch is merged)

Requires Python 3.9+, Git, Docker Desktop / Docker Engine with Docker Compose v2. Install the launcher from a source checkout:

    python -m pip install .
    moolbase init

For a source-free command installation after merge, use:

    pipx install 'git+https://github.com/rastogivaibhav/MoolBase.git'
    moolbase init

On Windows, the standard Python package console entrypoint is installed as `moolbase.exe`; ensure your Python scripts directory is on PATH. You can use `py -m pip install .` to install from a checkout.

By default this generates a new 256-bit API key and saves `~/.moolbase/.env` privately, creates `~/.moolbase/compose.yaml`, pulls `docker.io/vaibhavrastogi90/moolbase:0.6.0-audit.3`, then requests container startup using Docker Compose. The Docker Hub tag **must be published first**; until then the pull will fail without losing the locally generated key.

### Build from MoolBase source before Docker Hub publication

From the actual MoolBase repository checkout:

    moolbase init --dir ~/.moolbase-source --build-source .

Windows PowerShell also supports `moolbase init --dir "$HOME/.moolbase-source" --build-source .`.

Configure without starting anything:

    moolbase init --no-start

Inspect the container from the same installation:

    moolbase status

### Security and reproducibility

- Each installation uses a different CSPRNG-generated 64-hex-character key. The key is never printed, embedded in the Docker image or placed in source control.
- The local configuration directory is 0700 and the key file 0600 on POSIX. On Windows, ACL inheritance applies: store it under your private user profile.
- Repeated `moolbase init` does not rotate a valid existing key or overwrite existing settings. A corrupt or partial install causes an explicit error.
- Only local loopback port 8080 is exposed by default. This is an authenticated HTTP API, not a web UI.
- Docker environment variables remain visible to users with Docker daemon privileges. This is a trusted local development setup, **not** multi-tenant secret management.
- Persistent data uses Docker's named `moolbase-data` volume. Never remove volumes if you need to retain the stored evidence.
- The current native image is linux/amd64 only. ARM systems require emulation until a verified ARM64 build is available.
- A pull error such as `manifest unknown` can mean the versioned Docker image has not yet been published. Retry `moolbase init` after publication, retaining the original key; or use `--build-source` with a *different* configuration directory.
- This command is an optional developer CLI. The separate Docker Desktop extension remains an independent package.

### Tests

Run the lightweight tests (no Docker daemon required):

    python -m unittest discover -s tests -p 'test_moolbase_init.py' -v

The full native-image integration and persistent-state checks remain release gates.
