"""Portable checkout-contract regressions; also run directly on Windows CI.

Does not require shell clean filters or administrator symlink privileges.
"""
import importlib.util
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

VERIFY = Path(__file__).with_name('verify-checkout-raw.py')


class PortableCheckoutTests(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory()
        self.addCleanup(self.work.cleanup)
        self.repo = Path(self.work.name)
        self.run_git('init', '-q')
        self.run_git('config', 'user.email', 'test@invalid')
        self.run_git('config', 'user.name', 'Test')
        (self.repo / 'plain.txt').write_bytes(b'checked bytes\n')
        self.run_git('add', 'plain.txt')
        self.run_git('commit', '-qm', 'source')
        self.head = self.run_git('rev-parse', 'HEAD').stdout.strip()

    def run_git(self, *args):
        return subprocess.run(['git', *args], cwd=self.repo, check=True, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE)

    def verify(self, expected=None):
        return subprocess.run([sys.executable, str(VERIFY)], cwd=self.repo,
                              env={**os.environ, 'EXPECTED_SHA': expected or self.head},
                              capture_output=True, text=True, timeout=15)

    def test_clean_checkout(self):
        result = self.verify()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn('RAW_CHECKOUT_VERIFIED', result.stdout)

    def test_modified_bytes_denied(self):
        (self.repo / 'plain.txt').write_bytes(b'altered bytes\n')
        self.assertEqual(self.verify().returncode, 1)

    def test_staged_index_denied(self):
        (self.repo / 'plain.txt').write_bytes(b'altered bytes\n')
        self.run_git('add', 'plain.txt')
        self.assertEqual(self.verify().returncode, 1)

    def test_untracked_file_denied(self):
        (self.repo / 'untracked.txt').write_bytes(b'new')
        self.assertEqual(self.verify().returncode, 1)

    def test_wrong_pin_denied(self):
        self.assertEqual(self.verify('f' * 40).returncode, 1)

    def test_safe_nested_checkout(self):
        (self.repo / 'nested').mkdir()
        (self.repo / 'nested' / 'source.c').write_bytes(b'int good;\n')
        self.run_git('add', 'nested/source.c')
        self.run_git('commit', '-qm', 'nested')
        self.head = self.run_git('rev-parse', 'HEAD').stdout.strip()
        self.assertEqual(self.verify().returncode, 0)

    def test_invalid_windows_paths_denied(self):
        spec = importlib.util.spec_from_file_location('raw_checkout', VERIFY)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        for name in (b'CON', b'com1.txt', b'NUL.err', b'bad:alt', b'a/../b',
                     b'trailing.', b'a\\name'):
            with self.subTest(name=name), self.assertRaises(RuntimeError):
                module.valid_path(name)

    @unittest.skipUnless(os.name == 'nt', 'Windows-only reparse-point test')
    def test_windows_symlink_rejected_when_privilege_available(self):
        source = self.repo / 'plain.txt'
        source.unlink()
        target = self.repo / 'other.txt'
        target.write_bytes(b'checked bytes\n')
        try:
            source.symlink_to(target)
        except (OSError, NotImplementedError):
            self.skipTest('Windows symlink creation is not permitted')
        self.assertEqual(self.verify().returncode, 1)


if __name__ == '__main__':
    unittest.main()
