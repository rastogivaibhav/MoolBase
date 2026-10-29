"""MoolBase first-run setup security and regression tests."""
import contextlib
from io import StringIO
import os
from pathlib import Path
import stat
import tempfile
import unittest
from unittest.mock import patch
from moolbase_installer.cli import main, _parse_env

class InitTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.home = Path(self.tmp.name) / ".moolbase"

    def init(self, *args):
        out, err = StringIO(), StringIO()
        with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
            rc = main(["init", "--dir", str(self.home), *args])
        return rc, out.getvalue(), err.getvalue()

    def test_init_secure_and_idempotent(self):
        rc, out, err = self.init("--no-start")
        self.assertEqual(rc, 0, err)
        key = _parse_env(self.home / ".env")["GRAPHENEDB_API_KEY"]
        self.assertRegex(key, r"^[a-f0-9]{64}$")
        self.assertNotIn(key, out + err + (self.home / "compose.yaml").read_text())
        self.assertIn("127.0.0.1", (self.home / "compose.yaml").read_text())
        if os.name == "posix":
            self.assertEqual(stat.S_IMODE(self.home.stat().st_mode), 0o700)
            self.assertEqual(stat.S_IMODE((self.home / ".env").stat().st_mode), 0o600)
        before = (self.home / ".env").read_bytes()
        rc, out, err = self.init("--no-start")
        self.assertEqual(rc, 0, err)
        self.assertIn("Reusing", out)
        self.assertEqual((self.home / ".env").read_bytes(), before)

    def test_partial_config_does_not_rotate_key(self):
        self.init("--no-start")
        old = (self.home / ".env").read_bytes()
        (self.home / "compose.yaml").unlink()
        rc, _, err = self.init("--no-start")
        self.assertEqual(rc, 1)
        self.assertIn("Partial", err)
        self.assertEqual((self.home / ".env").read_bytes(), old)

    def test_symlink_refused(self):
        self.home.mkdir()
        target = self.home / "target"
        target.write_text("untouched")
        (self.home / ".env").symlink_to(target)
        rc, _, err = self.init("--no-start")
        self.assertEqual(rc, 1)
        self.assertIn("symlinked", err)
        self.assertEqual(target.read_text(), "untouched")

    def test_without_docker_fails_cleanly(self):
        with patch("moolbase_installer.cli.shutil.which", return_value=None):
            rc, _, err = self.init()
        self.assertEqual(rc, 1)
        self.assertIn("Docker CLI", err)
        self.assertFalse(self.home.exists())

    def test_download_and_start_use_saved_secret(self):
        self.init("--no-start")
        calls = []
        def fake_run(command, **kwargs):
            calls.append((command, kwargs))
            return type("Result", (), {"returncode": 0, "stdout": "", "stderr": ""})()
        with patch("moolbase_installer.cli.shutil.which", return_value="/fake/docker"), patch("moolbase_installer.cli.subprocess.run", side_effect=fake_run):
            rc, out, err = self.init()
        self.assertEqual(rc, 0, err)
        self.assertEqual([c[0][-2:] for c in calls], [["pull", "moolbase"], ["-d", "--no-build"]])
        key = _parse_env(self.home / ".env")["GRAPHENEDB_API_KEY"]
        self.assertEqual(calls[0][1]["env"]["GRAPHENEDB_API_KEY"], key)
        self.assertNotIn(key, out + err + repr(calls[0][0]))

    def test_failed_pull_retains_key(self):
        self.init("--no-start")
        key = _parse_env(self.home / ".env")["GRAPHENEDB_API_KEY"]
        fail = type("Result", (), {"returncode": 1, "stdout": "", "stderr": "manifest unknown"})()
        with patch("moolbase_installer.cli.shutil.which", return_value="/fake/docker"), patch("moolbase_installer.cli.subprocess.run", return_value=fail):
            rc, out, err = self.init()
        self.assertEqual(rc, 1)
        self.assertIn("may not yet be published", err)
        self.assertNotIn(key, out + err)
        self.assertEqual(_parse_env(self.home / ".env")["GRAPHENEDB_API_KEY"], key)

    def test_source_build_and_status(self):
        source = Path(self.tmp.name) / "src"
        source.mkdir()
        (source / "Dockerfile").write_text("FROM scratch")
        (source / "CMakeLists.txt").write_text("# Build")
        calls = []
        def fake_run(command, **kwargs):
            calls.append(command)
            return type("Result", (), {"returncode": 0, "stdout": "healthy", "stderr": ""})()
        with patch("moolbase_installer.cli.shutil.which", return_value="/fake/docker"), patch("moolbase_installer.cli.subprocess.run", side_effect=fake_run):
            rc, _, err = self.init("--build-source", str(source))
            self.assertEqual(rc, 0, err)
            self.assertEqual(calls[0][-3:], ["up", "-d", "--build"])
            out = StringIO()
            with contextlib.redirect_stdout(out):
                rc = main(["status", "--dir", str(self.home)])
            self.assertEqual(rc, 0)
            self.assertIn("healthy", out.getvalue())

if __name__ == "__main__":
    unittest.main()
