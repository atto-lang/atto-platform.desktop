from pathlib import Path
import unittest
import yaml

WF = Path(__file__).resolve().parent.parent / '.github' / 'workflows' / 'desktop-trusted-pr-metadata-policy.yml'

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
    def test_base_only_checkout(self):
        steps = self.workflow['jobs']['sourcefree-policy']['steps']
        self.assertEqual(len(steps), 2)
        self.assertTrue(steps[0]['uses'].startswith('actions/checkout@11bd71901bbe5b'))
        self.assertEqual(steps[0]['with']['ref'], '${{ github.event.pull_request.base.sha }}')
        self.assertEqual(steps[0]['with']['persist-credentials'], 'false')
        self.assertNotIn('github.event.pull_request.head.sha', self.raw)
    def test_never_executes_candidate_workflow(self):
        self.assertIn('python3 -I scripts/trusted_policy.py', self.raw)
        self.assertNotIn('pull_request_target: true', self.raw)
        self.assertNotIn('actions/upload-artifact', self.raw)
        self.assertNotIn('pull_request_target\n    with:\n      ref:', self.raw)
    def test_gated_main_only(self):
        expression = self.workflow['jobs']['sourcefree-policy']['if']
        self.assertIn("github.event.pull_request.base.ref == 'main'", expression)
        self.assertIn("github.repository == 'ores-truffle-oreslang/oreslang-platform.desktop'", expression)

if __name__ == '__main__': unittest.main()
