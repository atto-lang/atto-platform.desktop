import copy
import importlib.util
from pathlib import Path
import unittest

FILE = Path(__file__).with_name('trusted_policy.py')
spec = importlib.util.spec_from_file_location('trusted_desktop_policy', FILE)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
REPO = m.REPO
HEAD = 'a'*40
BASE = 'b'*40

class PolicyTests(unittest.TestCase):
    def setUp(self):
        self.event = {'pull_request': {'number': 16, 'head': {'sha': HEAD}}}
        self.pr = {'number': 16, 'state': 'open', 'draft': False,
                   'head': {'sha': HEAD, 'repo': {'full_name': REPO}},
                   'base': {'sha': BASE, 'ref': 'main', 'repo': {'full_name': REPO}},
                   'user': {'login': 'writer'}, 'changed_files': 1}
        self.db = {
            f'repos/{REPO}/pulls/16': self.pr,
            f'repos/{REPO}/branches/main': {'protected': True, 'commit': {'sha': BASE}},
            f'repos/{REPO}/branches/main/protection': {
                'required_status_checks': {'strict': True, 'checks': [
                    {'context': name, 'app_id': 1234} for name in sorted(m.REQUIRED_CHECKS)]},
                'required_pull_request_reviews': {'required_approving_review_count': 2,
                    'dismiss_stale_reviews': True, 'require_code_owner_reviews': True},
                'enforce_admins': {'enabled': True},
                'allow_force_pushes': {'enabled': False},
                'allow_deletions': {'enabled': False}},
            f'repos/{REPO}/pulls/16/files?per_page=100&page=1': [{'filename':'src/main.c'}],
            f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1': [
                {'user': {'login': 'ORESoftware'}, 'state': 'APPROVED', 'commit_id': HEAD}],
            f'repos/{REPO}/collaborators/ORESoftware/permission': {'permission': 'admin'},
            f'repos/{REPO}/collaborators/security-second/permission': {'permission': 'write'}}
        self.calls = []
    def get(self, path):
        self.calls.append(path)
        if path not in self.db:
            raise m.Denied('missing API fixture')
        return copy.deepcopy(self.db[path])
    def decision(self):
        return m.decision(self.event, REPO, 'pull_request_target', self.get)
    def deny(self):
        with self.assertRaises(m.Denied): self.decision()
    def test_clean_metadata_pass_is_not_release_approval(self):
        result = self.decision()
        self.assertEqual(result['head_sha'], HEAD)
        self.assertEqual(result['kind'], 'METADATA_ONLY_NOT_RELEASE_APPROVAL')
    def test_unprotected_main_denied(self):
        self.db[f'repos/{REPO}/branches/main']['protected'] = False
        self.deny()
    def test_missing_protection_api_denied(self):
        del self.db[f'repos/{REPO}/branches/main/protection']
        self.deny()
    def test_non_strict_checks_denied(self):
        self.db[f'repos/{REPO}/branches/main/protection']['required_status_checks']['strict'] = False
        self.deny()
    def test_admin_bypass_denied(self):
        self.db[f'repos/{REPO}/branches/main/protection']['enforce_admins']['enabled'] = False
        self.deny()
    def test_missing_one_desktop_required_check_denied(self):
        c = self.db[f'repos/{REPO}/branches/main/protection']['required_status_checks']['checks']
        c.pop()
        self.deny()
    def test_any_app_check_source_denied(self):
        c = self.db[f'repos/{REPO}/branches/main/protection']['required_status_checks']['checks']
        c[0]['app_id'] = None
        self.deny()
    def test_missing_stale_review_dismissal_denied(self):
        self.db[f'repos/{REPO}/branches/main/protection']['required_pull_request_reviews']['dismiss_stale_reviews'] = False
        self.deny()
    def test_codeowner_reviews_not_required_denied(self):
        self.db[f'repos/{REPO}/branches/main/protection']['required_pull_request_reviews']['require_code_owner_reviews'] = False
        self.deny()
    def test_force_push_allowed_denied(self):
        self.db[f'repos/{REPO}/branches/main/protection']['allow_force_pushes']['enabled'] = True
        self.deny()
    def test_deletion_allowed_denied(self):
        self.db[f'repos/{REPO}/branches/main/protection']['allow_deletions']['enabled'] = True
        self.deny()
    def test_stale_head_event_denied(self):
        self.event['pull_request']['head']['sha'] = 'c'*40
        self.deny()
    def test_stale_base_denied(self):
        self.db[f'repos/{REPO}/branches/main']['commit']['sha'] = 'c'*40
        self.deny()
    def test_draft_denied(self):
        self.pr['draft'] = True
        self.deny()
    def test_cross_repo_pr_denied(self):
        self.pr['head']['repo']['full_name'] = 'other/fork'
        self.deny()
    def test_synthetic_workflow_modification_requires_second_review(self):
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': '.github/workflows/ci.yml'}]
        self.deny()
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
            {'user': {'login': 'security-second'}, 'state': 'APPROVED', 'commit_id': HEAD})
        self.assertTrue(self.decision()['sensitive_paths'])
    def test_renamed_workflow_requires_second_review(self):
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': 'ordinary.txt', 'previous_filename': '.github/workflows/ci.yml'}]
        self.deny()
    def test_unprivileged_second_reviewer_is_denied(self):
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': '.github/workflows/ci.yml'}]
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
            {'user': {'login': 'security-second'}, 'state': 'APPROVED', 'commit_id': HEAD})
        self.db[f'repos/{REPO}/collaborators/security-second/permission'] = {'permission':'read'}
        self.deny()
    def test_verifier_tampering_requires_second_review(self):
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': 'scripts/verify-checkout-raw.py'}]
        self.deny()
    def test_review_on_previous_commit_denied(self):
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'][0]['commit_id'] = BASE
        self.deny()
    def test_last_review_dismissed_denied(self):
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
            {'user': {'login': 'ORESoftware'}, 'state': 'DISMISSED', 'commit_id': HEAD})
        self.deny()
    def test_self_approval_denied(self):
        self.pr['user']['login'] = 'ORESoftware'
        self.deny()
    def test_incomplete_file_pagination_denied(self):
        self.pr['changed_files'] = 2
        self.deny()
    def test_git_api_only_fetched_with_static_paths(self):
        self.decision()
        self.assertTrue(all(path.startswith(f'repos/{REPO}/') for path in self.calls))
    def test_reviews_page_size_100_requires_next_page(self):
        reviews = self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1']
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'] = reviews*100
        self.deny()
    def test_bad_event_denied(self):
        with self.assertRaises(m.Denied):
            m.decision(self.event, REPO, 'pull_request', self.get)

if __name__=='__main__': unittest.main()
