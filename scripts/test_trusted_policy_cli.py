import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import test_trusted_policy as fixtures
REPO = fixtures.REPO
HEAD = fixtures.HEAD

SCRIPT = Path(__file__).with_name('trusted_policy.py')

class PolicyCliTests(unittest.TestCase):
    def setUp(self):
        data = fixtures.PolicyTests()
        data.setUp()
        self.fixture = data.db
        self.event = data.event
    def invoke(self, protected=True):
        with tempfile.TemporaryDirectory(prefix='trusted_policy_cli_') as tmp:
            base = Path(tmp)
            db = json.loads(json.dumps(self.fixture))
            if not protected:
                db[f'repos/{REPO}/branches/main']['protected'] = False
            data = base / 'api.json'
            data.write_text(json.dumps(db))
            event = base / 'event.json'
            event.write_text(json.dumps(self.event))
            calls = base / 'calls.log'
            stub = base / 'gh'
            stub.write_text('#!/usr/bin/env python3\n'
                            'import json,os,sys\n'
                            'assert sys.argv[1:4]==["api","--method","GET"]\n'
                            'with open(os.environ["GH_CALLS"],"a") as f:f.write(" ".join(sys.argv[1:])+"\\n")\n'
                            'data=json.load(open(os.environ["GH_FIXTURE"]))\n'
                            'print(json.dumps(data[sys.argv[4]]))\n')
            stub.chmod(0o755)
            env = {**os.environ, 'GH_TOKEN': 'synthetic-not-real',
                   'GITHUB_EVENT_PATH': str(event),
                   'GITHUB_EVENT_NAME': 'pull_request_target',
                   'GITHUB_REPOSITORY': REPO,
                   'GH_CALLS': str(calls), 'GH_FIXTURE': str(data),
                   'PATH': f'{base}:{os.environ.get("PATH","")}' }
            result = subprocess.run([sys.executable,'-I',str(SCRIPT)],env=env,
                                    capture_output=True,text=True,timeout=10)
            return result, calls.read_text() if calls.exists() else ''
    def test_qualified_policy_cli_uses_only_read_only_api_calls(self):
        result, calls = self.invoke()
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertIn('METADATA_ONLY_NOT_RELEASE_APPROVAL',result.stdout)
        self.assertTrue(calls.strip())
        self.assertTrue(all(line.startswith('api --method GET repos/') for line in calls.splitlines()))
        self.assertNotIn('GH_TOKEN',result.stdout)
    def test_unprotected_main_cli_fails_closed(self):
        result, calls = self.invoke(protected=False)
        self.assertEqual(result.returncode,2)
        self.assertIn('SOURCE_FREE_METADATA_POLICY_NO_GO',result.stderr)
        self.assertIn('not protected',result.stderr)
        self.assertNotIn('POST',calls)

if __name__=='__main__':unittest.main()
