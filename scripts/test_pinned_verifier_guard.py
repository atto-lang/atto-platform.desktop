"""Adversarial regressions for the staged, SHA-256-pinned CI verifier snapshot.

An immutable Git SHA proves content identity, not independent approval: the
workflow/commit still need protected-main review and authentic hosted checks.
"""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest

SOURCE = Path(__file__).with_name("verify-checkout-raw.py")
WORKFLOW = Path(__file__).resolve().parent.parent / ".github" / "workflows" / "ci.yml"
PIN_COMMIT = "db3717cd0b2e2bc3ef542848582dc51238c7d768"
PIN_SHA256 = "bb2b487c59826eefe075edc93cf3cb7ed56ae81c6efa09ac40afb25d3f51e6d9"
SNAPSHOT_FILE = "ores-desktop-pinned-verifier.py"


def copy_command(workflow):
    """Extract the literal Python preflight, never execute the workflow itself."""
    commands = re.findall(r"^\s+python(?:3)? -I -c '([^'\n]+)'\s*$", workflow, re.M)
    return commands


class PinnedVerifierTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="pinned_verifier_fixture_")
        self.addCleanup(self.tmp.cleanup)
        base = Path(self.tmp.name)
        self.source_root = base / "checkout"
        self.source_root.mkdir()
        self.temp_dir = base / "runner_temp"
        self.temp_dir.mkdir()
        self.scripts = self.source_root / "scripts"
        self.scripts.mkdir()
        self.pinned_bytes = SOURCE.read_bytes()
        # This fixture specifically tests the reviewed pin. If the source script
        # changes, refresh the reviewed pin and its approvals deliberately.
        self.assertEqual(hashlib.sha256(self.pinned_bytes).hexdigest(), PIN_SHA256)
        (self.scripts / "verify-checkout-raw.py").write_bytes(self.pinned_bytes)

    def snapshot(self, script=None):
        code = script or (
            'import hashlib,os,pathlib,sys; '
            'raw=pathlib.Path("scripts/verify-checkout-raw.py").read_bytes(); '
            'digest=hashlib.sha256(raw).hexdigest(); '
            f'digest=="{PIN_SHA256}" or sys.exit("PINNED_VERIFIER_REJECTED: SHA256 mismatch"); '
            'pathlib.Path(os.environ["RUNNER_TEMP"],"ores-desktop-pinned-verifier.py").write_bytes(raw)'
        )
        return subprocess.run([sys.executable, "-I", "-c", code], cwd=self.source_root,
                              env={**os.environ, "RUNNER_TEMP": str(self.temp_dir)},
                              capture_output=True, text=True, timeout=12)

    def test_pinned_source_is_cryptographically_identified(self):
        self.assertEqual(hashlib.sha256(self.pinned_bytes).hexdigest(), PIN_SHA256)
        self.assertEqual(len(PIN_COMMIT), 40)

    def test_snapshot_succeeds_outside_checkout(self):
        p = self.snapshot()
        self.assertEqual(p.returncode, 0, p.stderr)
        snap = self.temp_dir / SNAPSHOT_FILE
        self.assertEqual(snap.read_bytes(), self.pinned_bytes)
        self.assertFalse(snap.is_relative_to(self.source_root))

    def test_modified_source_rejected_before_snapshot(self):
        (self.scripts / "verify-checkout-raw.py").write_text('print("forge pass")\n')
        p = self.snapshot()
        self.assertNotEqual(p.returncode, 0)
        self.assertIn("PINNED_VERIFIER_REJECTED", p.stderr)
        self.assertFalse((self.temp_dir / SNAPSHOT_FILE).exists())

    def test_swapped_source_does_not_change_pinned_snapshot(self):
        p = self.snapshot()
        self.assertEqual(p.returncode, 0, p.stderr)
        # The candidate checkout may replace the script after snapshot creation.
        (self.scripts / "verify-checkout-raw.py").write_text("raise SystemExit(0)\n")
        self.assertEqual((self.temp_dir / SNAPSHOT_FILE).read_bytes(), self.pinned_bytes)
        self.assertNotEqual((self.scripts / "verify-checkout-raw.py").read_bytes(), self.pinned_bytes)

    def test_verified_snapshot_rejects_tampered_checkout(self):
        # Prove real Git file-byte verification uses the frozen temp snapshot.
        def git(*args):
            return subprocess.run(["git", *args], cwd=self.source_root, check=True,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout.decode().strip()
        git("init", "-q")
        git("config", "user.email", "test@example.invalid")
        git("config", "user.name", "Pinned Verifier Contract")
        (self.source_root / "hello.c").write_bytes(b"int good = 1;\n")
        git("add", ".")
        git("commit", "-qm", "trusted contents")
        expected = git("rev-parse", "HEAD")
        self.assertEqual(self.snapshot().returncode, 0)
        # Tamper a tracked file and substitute a deliberately permissive verifier.
        (self.source_root / "hello.c").write_bytes(b"int bad = 1;\n")
        (self.scripts / "verify-checkout-raw.py").write_text("raise SystemExit(0)\n")
        bad = subprocess.run([sys.executable, "-I", str(self.scripts / "verify-checkout-raw.py")],
                             cwd=self.source_root, env={**os.environ, "EXPECTED_SHA": expected},
                             capture_output=True, text=True, timeout=12)
        self.assertEqual(bad.returncode, 0, "control: forged candidate verifier should lie")
        good = subprocess.run([sys.executable, "-I", str(self.temp_dir / SNAPSHOT_FILE)],
                              cwd=self.source_root, env={**os.environ, "EXPECTED_SHA": expected},
                              capture_output=True, text=True, timeout=12)
        self.assertEqual(good.returncode, 1, (good.stdout, good.stderr))
        self.assertIn("RAW_CHECKOUT_REJECTED", good.stderr)

    def test_all_five_workflows_pin_and_execute_outside_checkout(self):
        wf = WORKFLOW.read_text()
        commands = copy_command(wf)
        self.assertEqual(len(commands), 5)
        for code in commands:
            self.assertIn(PIN_SHA256, code)
            self.assertIn(SNAPSHOT_FILE, code)
            self.assertEqual(self.snapshot(code).returncode, 0)
        self.assertEqual(wf.count(f"ref: {PIN_COMMIT}"), 5)
        self.assertEqual(wf.count("Checkout pinned verifier commit"), 5)
        self.assertEqual(wf.count("Checkout candidate exact source HEAD"), 5)
        self.assertEqual(wf.count("Verify exact source head"), 5)
        self.assertEqual(wf.count('"$RUNNER_TEMP/ores-desktop-pinned-verifier.py"'), 4)
        self.assertEqual(wf.count('"$env:RUNNER_TEMP/ores-desktop-pinned-verifier.py"'), 1)
        self.assertNotIn("python3 scripts/verify-checkout-raw.py", wf)
        self.assertNotIn("python scripts/verify-checkout-raw.py", wf)

    def test_windows_powershell_snapshot_and_exit_are_explicit(self):
        wf = WORKFLOW.read_text()
        self.assertIn('shell: pwsh\n        run: |\n          python -I -c', wf)
        self.assertIn('if ($LASTEXITCODE -ne 0) { throw "raw checkout source verification failed" }', wf)
        self.assertIn('git config --global core.autocrlf false', wf)
        windows = wf.split("  windows:\n", 1)[1].split("  linux-sanitizers:\n", 1)[0]
        self.assertLess(windows.index("Disable Windows Git newline rewriting"),
                        windows.index("Checkout pinned verifier commit"))

    def test_security_warning_does_not_claim_independent_workflow_trust(self):
        self.assertIn("not an independent trust root until reviewed/protected", WORKFLOW.read_text())


if __name__ == "__main__":
    unittest.main()
