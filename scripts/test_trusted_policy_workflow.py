from pathlib import Path
import hashlib
import unittest
import yaml

WF = Path(__file__).resolve().parent.parent / '.github' / 'workflows' / 'desktop-trusted-pr-metadata-policy.yml'
BLOB = '94eb55af6fd1e1c505c5663568d3a2e0b51321c3'
DIGEST = 'fdbad5892896312ca70bb754f87e46549579f3e8ef5f79bbcfe870d0ecedbb95'

class WorkflowTests(unittest.TestCase):
    def setUp(self):
        self.raw = WF.read_text()
        self.workflow = yaml.load(self.raw, Loader=yaml.BaseLoader)
    def test_pr_target_uses_default_branch_policy(self):
        events = self.workflow['on']
        self.assertEqual(set(events), {'pull_request_target', 'pull_request_review'})
        self.assertNotIn('pull_request:', self.raw)
    def test_read_only_permissions(self):
        self.assertEqual(self.workflow['permissions'], {'contents':'read', 'pull-requests':'read'})
        self.assertNotIn('checks: write', self.raw)
    def test_no_checkout_no_pr_code(self):
        steps = self.workflow['jobs']['sourcefree-policy']['steps']
        self.assertEqual(len(steps), 2)
        self.assertNotIn('actions/checkout@', self.raw)
        self.assertNotIn('github.event.pull_request.head.sha', self.raw)
        self.assertNotIn('git checkout', self.raw)
        self.assertNotIn('git fetch', self.raw)
    def test_immutable_policy_blob_and_digest(self):
        self.assertIn(f'/git/blobs/{BLOB}', self.raw)
        self.assertIn(DIGEST, self.raw)
        policy = WF.resolve().parent.parent.parent / 'scripts' / 'trusted_policy.py'
        self.assertEqual(hashlib.sha256(policy.read_bytes()).hexdigest(), DIGEST)
        self.assertIn('gh api --method GET', self.raw)
        self.assertIn('python3 -I "$RUNNER_TEMP/desktop-trusted-policy.py"', self.raw)
    def test_no_candidate_execution_or_artifact(self):
        self.assertNotIn('actions/upload-artifact', self.raw)
        self.assertNotIn('git clone', self.raw)
        self.assertNotIn('pull_request_target\n    with:\n      ref:', self.raw)
    def test_gated_main_only(self):
        expression = self.workflow['jobs']['sourcefree-policy']['if']
        self.assertIn("github.event.pull_request.base.ref == 'main'", expression)
        self.assertIn("github.repository == 'ores-truffle-oreslang/oreslang-platform.desktop'", expression)

if __name__ == '__main__': unittest.main()
