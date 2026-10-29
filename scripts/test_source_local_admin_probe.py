import os
from pathlib import Path
import subprocess
import tempfile
import unittest

PROBE = Path(__file__).with_name('source-local-admin-probe.sh')

class SourceLocalAdminProbe(unittest.TestCase):
    def test_script_valid_bash(self):
        p = subprocess.run(['bash', '-n', str(PROBE)], capture_output=True)
        self.assertEqual(p.returncode, 0, p.stderr)

    def test_no_gh_fails_closed(self):
        with tempfile.TemporaryDirectory() as d:
            env = dict(os.environ, PATH=d)
            p = subprocess.run(['/bin/bash', str(PROBE)], env=env, capture_output=True, text=True)
            self.assertEqual(p.returncode, 2)
            self.assertIn('BLOCKED', p.stderr)

    def test_invalid_run_id_rejected_before_gh(self):
        with tempfile.TemporaryDirectory() as d:
            env = dict(os.environ, PATH=d)
            p = subprocess.run(['/bin/bash', str(PROBE), '9;exit 0'], env=env,
                               capture_output=True, text=True)
            self.assertEqual(p.returncode, 2)
            self.assertIn('Usage', p.stderr)

    def test_fake_get_only_and_fail_closed(self):
        with tempfile.TemporaryDirectory() as d:
            script = Path(d) / 'gh'
            calls = Path(d) / 'calls'
            script.write_text('''#!/bin/sh
printf '%s\\n' "$*" >> "$PROBE_CALLS"
case "$*" in
*branches/main*) echo false ;;
*rulesets*) echo 403 >&2; exit 1 ;;
*protection*) echo 403 >&2; exit 1 ;;
*actions/runs*) echo 'unavailable';;
*) echo unexpected >&2; exit 1;;
esac
''')
            script.chmod(0o755)
            env = dict(os.environ, PATH=f'{d}:/usr/bin:/bin', PROBE_CALLS=str(calls))
            p = subprocess.run(['/bin/bash', str(PROBE), '40928128150', '40928128075'],
                               env=env, capture_output=True, text=True)
            self.assertEqual(p.returncode, 2)
            self.assertIn('NO-GO', p.stdout)
            self.assertEqual(p.stdout.count('main=UNPROTECTED/UNVERIFIED'), 7)
            observed = calls.read_text().splitlines()
            self.assertEqual(len(observed), 25)  # 7*3 protection queries + two run+job pairs
            self.assertTrue(all(x.startswith('api --method GET ') for x in observed))
            self.assertFalse(any('contents/' in x or 'POST' in x or 'PATCH' in x for x in observed))

    def test_reject_too_many_args(self):
        p = subprocess.run(['/bin/bash', str(PROBE), '1', '2', '3'], capture_output=True, text=True)
        self.assertEqual(p.returncode, 2)

if __name__ == '__main__': unittest.main()
