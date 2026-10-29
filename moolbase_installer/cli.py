"""Local first-run Docker setup for MoolBase.

The actual reasoning engine is the native C++ MoolBase server in its container.
"""
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import secrets
import shutil
import subprocess
import sys

DEFAULT_IMAGE = "docker.io/vaibhavrastogi90/moolbase:0.6.0-audit.3"
DEFAULT_HOME = Path.home() / ".moolbase"
IMAGE_PATTERN = re.compile(r"^[a-zA-Z0-9][a-zA-Z0-9._/-]*:[a-zA-Z0-9_.-]+$")
KEY_PATTERN = re.compile(r"^[a-fA-F0-9]{64}$")


class SetupError(Exception):
    """Actionable setup failure without revealing credentials."""


def _parse_env(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        if "=" not in line:
            raise SetupError("Existing .env has an invalid line; repair it without deleting your key.")
        name, value = line.split("=", 1)
        values[name] = value
    return values


def _store_exclusive(path: Path, body: str) -> None:
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    fd = os.open(str(path), flags, 0o600)
    try:
        with os.fdopen(fd, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(body)
    except BaseException:
        path.unlink(missing_ok=True)
        raise
    if os.name == "posix":
        path.chmod(0o600)


def _compose_yaml(build_source: Path | None) -> str:
    build = ""
    if build_source:
        import json
        build = ("    build:\n"
                 f"      context: {json.dumps(str(build_source))}\n"
                 "      dockerfile: Dockerfile\n")
    return (
        "name: moolbase\n"
        "services:\n"
        "  moolbase:\n"
        '    image: ${MOOLBASE_IMAGE:?MOOLBASE_IMAGE must be set}\n'
        "    platform: linux/amd64\n"
        + build +
        "    environment:\n"
        '      GRAPHENEDB_API_KEY: ${GRAPHENEDB_API_KEY:?Local API key must be set}\n'
        "    ports:\n"
        '      - "127.0.0.1:${MOOLBASE_PORT:-8080}:8080"\n'
        "    volumes:\n"
        "      - moolbase-data:/var/lib/graphenedb\n"
        "    restart: unless-stopped\n"
        "    security_opt:\n"
        "      - no-new-privileges:true\n"
        "    read_only: true\n"
        "    tmpfs:\n"
        "      - /tmp\n"
        "volumes:\n"
        "  moolbase-data:\n"
    )


def _validate_home(home: Path) -> None:
    if home.is_symlink():
        raise SetupError("Refusing a symlinked installation directory.")
    if home.exists() and not home.is_dir():
        raise SetupError("Installation path exists but is not a directory.")


def _ensure_config(home: Path, image: str, port: int, build_source: Path | None) -> tuple[dict[str, str], bool]:
    _validate_home(home)
    env_path, compose_path = home / ".env", home / "compose.yaml"
    if env_path.is_symlink() or compose_path.is_symlink():
        raise SetupError("Refusing symlinked configuration files.")
    if env_path.exists() != compose_path.exists():
        raise SetupError("Partial configuration detected. Restore the missing file; the existing key was not overwritten.")
    if env_path.exists():
        values = _parse_env(env_path)
        if not KEY_PATTERN.fullmatch(values.get("GRAPHENEDB_API_KEY", "")):
            raise SetupError("Existing API key must be 64 hexadecimal characters; repair it manually.")
        if not IMAGE_PATTERN.fullmatch(values.get("MOOLBASE_IMAGE", "")):
            raise SetupError("Existing image reference is invalid.")
        if not values.get("MOOLBASE_PORT", "").isdigit() or not 1024 <= int(values["MOOLBASE_PORT"]) <= 65535:
            raise SetupError("Existing host port is invalid.")
        if values.get("MOOLBASE_MODE", "registry") not in ("registry", "source"):
            raise SetupError("Existing installation mode is invalid.")
        return values, False
    home.mkdir(parents=True, exist_ok=True, mode=0o700)
    if os.name == "posix":
        home.chmod(0o700)
    # No shared/default keys; generated independently for each installation.
    key = secrets.token_hex(32)
    values = {
        "GRAPHENEDB_API_KEY": key,
        "MOOLBASE_IMAGE": "moolbase:local" if build_source else image,
        "MOOLBASE_PORT": str(port),
        "MOOLBASE_MODE": "source" if build_source else "registry",
    }
    body = "# MoolBase local instance; never share or commit this file.\n" + "".join(
        f"{name}={value}\n" for name, value in values.items()
    )
    try:
        _store_exclusive(env_path, body)
        _store_exclusive(compose_path, _compose_yaml(build_source))
    except BaseException:
        if not compose_path.exists():
            env_path.unlink(missing_ok=True)
        raise
    return values, True


def _docker(args: list[str], values: dict[str, str], home: Path) -> str:
    env = os.environ.copy()
    # OS-level env values must not override this installation's saved key.
    env.update(values)
    cmd = ["docker", "compose", "-f", str(home / "compose.yaml"),
           "--env-file", str(home / ".env")] + args
    try:
        result = subprocess.run(cmd, cwd=str(home), env=env, capture_output=True, text=True, check=False)
    except OSError as exc:
        raise SetupError("Could not run Docker; check Docker Desktop or Docker Engine.") from exc
    if result.returncode:
        raw = (result.stderr or result.stdout).strip()
        raw = raw.replace(values["GRAPHENEDB_API_KEY"], "[REDACTED]")
        raise SetupError(f"Docker command failed ({' '.join(args)}): {raw[:1600]}")
    return result.stdout


def _run_init(options: argparse.Namespace) -> None:
    home = options.dir.expanduser().absolute()
    source = options.build_source.expanduser().absolute() if options.build_source else None
    if source and not (source / "Dockerfile").is_file():
        raise SetupError("--build-source requires a MoolBase directory with Dockerfile.")
    if source and not (source / "CMakeLists.txt").is_file():
        raise SetupError("--build-source requires the MoolBase source checkout.")
    if not IMAGE_PATTERN.fullmatch(options.image):
        raise SetupError("--image requires an explicitly version-tagged Docker image.")
    if not options.no_start and not shutil.which("docker"):
        raise SetupError("Docker CLI not found. Install and start Docker Desktop or use --no-start.")
    values, created = _ensure_config(home, options.image, options.port, source)
    print(f"{'Created' if created else 'Reusing'} private MoolBase configuration in {home} (key hidden).")
    if options.no_start:
        print("Configuration only; Docker was not started (--no-start).")
        return
    if values.get("MOOLBASE_MODE", "registry") == "source":
        _docker(["up", "-d", "--build"], values, home)
    else:
        try:
            _docker(["pull", "moolbase"], values, home)
        except SetupError as exc:
            raise SetupError(str(exc) +
                "\nThe image tag may not yet be published. Retry later, or use --build-source PATH with a fresh directory.") from exc
        _docker(["up", "-d", "--no-build"], values, home)
    print(f"Container start requested. Check with: moolbase status --dir {home}")
    print(f"Local authenticated API: http://127.0.0.1:{values['MOOLBASE_PORT']} (not a web UI)")


def _run_status(options: argparse.Namespace) -> None:
    home = options.dir.expanduser().absolute()
    if not (home / ".env").is_file() or not (home / "compose.yaml").is_file():
        raise SetupError("No MoolBase configuration found. Run moolbase init first.")
    values = _parse_env(home / ".env")
    if not KEY_PATTERN.fullmatch(values.get("GRAPHENEDB_API_KEY", "")):
        raise SetupError("Existing installation key is invalid.")
    output = _docker(["ps"], values, home)
    print(output.strip() or "Docker Compose reports no containers.")
    print("Inspect the container in Docker Desktop for full health details.")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="moolbase", description="MoolBase local Docker setup")
    sub = parser.add_subparsers(dest="command", required=True)
    init = sub.add_parser("init", help="Create a unique local key and start the native Docker server")
    init.add_argument("--dir", type=Path, default=DEFAULT_HOME, help="Private installation directory")
    init.add_argument("--image", default=DEFAULT_IMAGE, help="Explicit Docker image and tag")
    init.add_argument("--port", type=int, default=8080, choices=range(1024, 65536), metavar="1024..65535")
    init.add_argument("--build-source", type=Path, help="Build the native engine from a local source checkout")
    init.add_argument("--no-start", action="store_true", help="Write configuration only, without Docker")
    status = sub.add_parser("status", help="Show Docker Compose status")
    status.add_argument("--dir", type=Path, default=DEFAULT_HOME)
    args = parser.parse_args(argv)
    try:
        if args.command == "init":
            _run_init(args)
        else:
            _run_status(args)
    except (SetupError, OSError) as exc:
        print(f"moolbase: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
