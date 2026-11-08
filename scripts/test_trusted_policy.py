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
                    {'context': name, 'app_id': m.TRUSTED_STATUS_APP_ID} for name in sorted(m.REQUIRED_CHECKS)]},
                'required_pull_request_reviews': {'required_approving_review_count': 1,
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
    def test_wrong_positive_app_id_denied(self):
        c = self.db[f'repos/{REPO}/branches/main/protection']['required_status_checks']['checks']
        c[0]['app_id'] = 1234
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


    def test_mixed_case_workflow_requires_second_review(self):
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': '.GITHUB/WORKFLOWS/ci.yml'}]
        self.deny()
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
            {'user': {'login': 'security-second'}, 'state': 'APPROVED', 'commit_id': HEAD})
        self.assertTrue(self.decision()['sensitive_paths'])

    def test_case_variant_sensitive_script_path_requires_second_review(self):
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': 'SCRIPTS/TRUSTED_POLICY.PY'}]
        self.deny()

    def test_renamed_mixed_case_sensitive_path_requires_second_review(self):
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': 'ordinary.txt', 'previous_filename': '.GITHUB/WORKFLOWS/ci.yml'}]
        self.deny()

    def test_duplicate_changed_files_are_rejected(self):
        self.pr['changed_files'] = 2
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': 'src/main.c'}, {'filename': 'src/main.c'}]
        self.deny()

    def test_case_colliding_changed_files_are_rejected(self):
        self.pr['changed_files'] = 2
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': 'src/main.c'}, {'filename': 'SRC/MAIN.C'}]
        self.deny()

    def test_nonportable_changed_file_paths_are_rejected(self):
        for raw in ('../.github/workflows/ci.yml', '/.github/workflows/ci.yml',
                    'src\\trusted_policy.py', 'src//main.c', 'src/./main.c',
                    'cafe\u0301.txt', '', 'src/\tfile.c'):
            with self.subTest(raw=repr(raw)):
                self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
                    {'filename': raw}]
                self.deny()

    def test_malformed_previous_filename_is_rejected(self):
        for value in (None, 4, [], {'filename': '.github/workflows/x.yml'}):
            with self.subTest(previous=repr(value)):
                self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
                    {'filename': 'ordinary.txt', 'previous_filename': value}]
                self.deny()

    def test_boolean_required_review_count_is_not_an_integer_quorum(self):
        self.db[f'repos/{REPO}/branches/main/protection'][
            'required_pull_request_reviews']['required_approving_review_count'] = True
        self.deny()

    def test_moving_pr_head_during_api_audit_is_rejected(self):
        count = 0
        def moving_get(path):
            nonlocal count
            if path == f'repos/{REPO}/pulls/16':
                count += 1
                if count > 1:
                    changed = copy.deepcopy(self.db[path])
                    changed['head']['sha'] = 'c' * 40
                    return changed
            return self.get(path)
        with self.assertRaises(m.Denied):
            m.decision(self.event, REPO, 'pull_request_target', moving_get)

    def test_moving_main_head_during_api_audit_is_rejected(self):
        count = 0
        def moving_get(path):
            nonlocal count
            if path == f'repos/{REPO}/branches/main':
                count += 1
                if count > 1:
                    changed = copy.deepcopy(self.db[path])
                    changed['commit']['sha'] = 'c' * 40
                    return changed
            return self.get(path)
        with self.assertRaises(m.Denied):
            m.decision(self.event, REPO, 'pull_request_target', moving_get)

    def test_missing_author_login_is_rejected(self):
        self.pr['user']['login'] = None
        self.deny()

    def test_non_object_event_is_rejected(self):
        with self.assertRaises(m.Denied):
            m.decision([], REPO, 'pull_request_target', self.get)


    def test_unicode_format_bidi_and_noncharacter_controls_are_denied(self):
        for path in ('src/ordinary\u202ebypass.txt', 'src/ordinary\u200ctitle.txt',
                     'src/ordinary\ufefftitle.txt', 'src/ordinary\u0085title.txt',
                     'src/ordinary\ud800title.txt'):
            with self.subTest(path=repr(path)):
                self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
                    {'filename': path}]
                self.deny()

    def test_safe_nfc_international_path_still_admitted(self):
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': 'src/caf\u00e9.txt'}]
        self.assertFalse(self.decision()['sensitive_paths'])


    def test_branch_required_quorum_is_enforced_on_ordinary_pr(self):
        rule = self.db[f'repos/{REPO}/branches/main/protection']['required_pull_request_reviews']
        rule['required_approving_review_count'] = 2
        self.deny()
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
            {'user': {'login': 'security-second'}, 'state': 'APPROVED', 'commit_id': HEAD})
        result = self.decision()
        self.assertEqual(result['qualified_review_count'], 2)
        self.assertFalse(result['sensitive_paths'])

    def test_branch_required_quorum_three_remains_enforced_for_sensitive_pr(self):
        self.db[f'repos/{REPO}/branches/main/protection'][
            'required_pull_request_reviews']['required_approving_review_count'] = 3
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': '.github/workflows/security.yml'}]
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
            {'user': {'login': 'security-second'}, 'state': 'APPROVED', 'commit_id': HEAD})
        self.deny()
        self.db[f'repos/{REPO}/collaborators/security-third/permission'] = {
            'permission': 'maintain'}
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
            {'user': {'login': 'security-third'}, 'state': 'APPROVED', 'commit_id': HEAD})
        result = self.decision()
        self.assertEqual(result['qualified_review_count'], 3)
        self.assertTrue(result['sensitive_paths'])

    def test_outstanding_changes_requested_blocks_positive_metadata(self):
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
            {'user': {'login': 'security-second'}, 'state': 'CHANGES_REQUESTED',
             'commit_id': HEAD})
        self.deny()

    def test_resolved_changes_requested_allows_new_current_head_approval(self):
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].extend([
            {'user': {'login': 'security-second'}, 'state': 'CHANGES_REQUESTED',
             'commit_id': HEAD},
            {'user': {'login': 'security-second'}, 'state': 'APPROVED',
             'commit_id': HEAD},
        ])
        self.assertEqual(self.decision()['qualified_review_count'], 2)

    def test_moving_review_approval_to_dismissed_during_audit_denied(self):
        calls = 0
        path = f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'
        def mutating_get(current):
            nonlocal calls
            if current == path:
                calls += 1
                if calls >= 2:
                    return [{'user': {'login': 'ORESoftware'}, 'state': 'DISMISSED',
                             'commit_id': HEAD}]
            return self.get(current)
        with self.assertRaises(m.Denied):
            m.decision(self.event, REPO, 'pull_request_target', mutating_get)
        self.assertGreaterEqual(calls, 2)

    def test_review_list_addition_during_audit_denied(self):
        calls = 0
        path = f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'
        def mutating_get(current):
            nonlocal calls
            result = self.get(current)
            if current == path:
                calls += 1
                if calls >= 2:
                    result.append({'user': {'login': 'security-second'},
                                   'state': 'COMMENTED', 'commit_id': HEAD})
            return result
        with self.assertRaises(m.Denied):
            m.decision(self.event, REPO, 'pull_request_target', mutating_get)
        self.assertGreaterEqual(calls, 2)

    def test_untrusted_third_party_cannot_satisfy_review_quorum(self):
        self.db[f'repos/{REPO}/branches/main/protection'][
            'required_pull_request_reviews']['required_approving_review_count'] = 2
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
            {'user': {'login': 'security-second'}, 'state': 'APPROVED', 'commit_id': HEAD})
        self.db[f'repos/{REPO}/collaborators/security-second/permission'] = {
            'permission': 'read'}
        self.deny()

    def test_malformed_review_record_is_denied(self):
        self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(None)
        self.deny()


    def test_source_free_ci_helpers_require_two_independent_approvals(self):
        for filename in ('scripts/source-local-admin-probe.sh', 'scripts/new_ci_helper.py',
                         'SCRIPTS/SOURCE-LOCAL-ADMIN-PROBE.SH'):
            with self.subTest(filename=filename):
                self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
                    {'filename': filename}]
                self.deny()
                self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].append(
                    {'user': {'login': 'security-second'}, 'state': 'APPROVED', 'commit_id': HEAD})
                self.assertTrue(self.decision()['sensitive_paths'])
                self.db[f'repos/{REPO}/pulls/16/reviews?per_page=100&page=1'].pop()

    def test_build_and_git_policy_paths_require_security_approval(self):
        for filename in ('CMakeLists.txt', 'cmake/toolchains/host.cmake', '.gitattributes',
                         '.gitmodules', 'Makefile', 'TOOLS/compiler-fixtures/check.py'):
            with self.subTest(filename=filename):
                self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
                    {'filename': filename}]
                self.deny()

    def test_renamed_build_or_source_policy_path_requires_security_approval(self):
        for original in ('CMakeLists.txt', 'SCRIPTS/helper.sh', '.gitattributes'):
            with self.subTest(previous_filename=original):
                self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
                    {'filename': 'src/ordinary.c', 'previous_filename': original}]
                self.deny()

    def test_windows_reserved_device_filenames_are_denied(self):
        for basename in ('CON', 'con.txt', 'PRN', 'AUX.log', 'NUL',
                         'COM1.txt', 'LPT9', 'COM\u00b9.txt', 'LPT\u00b2.dat',
                         'CONIN
, 'CONOUT
):
            with self.subTest(name=basename):
                self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
                    {'filename': 'src/' + basename}]
                self.deny()

    def test_cross_platform_trailing_space_dot_and_special_names_are_denied(self):
        for name in ('trailing.', 'trailing ', 'folder./file.c', '.GIT/config',
                     'a:b.c', 'a?b', 'a*b', 'a|b', 'a<b', 'a>b', 'quote".c'):
            with self.subTest(name=name):
                self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
                    {'filename': 'src/' + name}]
                self.deny()

    def test_cross_platform_safe_naming_controls_are_admitted(self):
        for name in ('COM10.txt', 'LPT10.txt', 'CONTRA.txt', 'file..txt',
                     'safe-thing.txt', 'caf\u00e9.txt'):
            with self.subTest(name=name):
                self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
                    {'filename': 'src/' + name}]
                self.assertFalse(self.decision()['sensitive_paths'])

    def test_renamed_windows_reserved_source_path_denied(self):
        self.db[f'repos/{REPO}/pulls/16/files?per_page=100&page=1'] = [
            {'filename': 'src/safe.c', 'previous_filename': 'src/CON.txt'}]
        self.deny()
if __name__=='__main__': unittest.main()
