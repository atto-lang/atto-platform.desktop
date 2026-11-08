#!/usr/bin/env python3
"""Read-only, source-free PR policy decision for a DEFAULT-BRANCH workflow.

Never execute this file from a PR checkout in a privileged context.
This program only reports metadata readiness; it never issues merge/export
approval, posts checks, downloads artifacts or executes untrusted code.
"""
import json
import os
import re
import subprocess
import unicodedata
import sys
from pathlib import Path

REPO = 'ores-truffle-oreslang/oreslang-platform.desktop'
OWNER = 'ORESoftware'
SHA = re.compile(r'^[a-f0-9]{40}$')
REQUIRED_CHECKS = {'linux-gcc', 'linux-clang', 'linux-sanitizers', 'windows', 'macos'}
TRUSTED_STATUS_APP_ID = 15368  # Verified GitHub Actions app on current source-repo checks
# CI helper scripts, Git admission metadata and build entrypoints need 2 reviewers.
SENSITIVE = ('.github/', 'scripts/', 'tools/', 'cmake/', 'cmakelists.txt',
             '.gitattributes', '.gitmodules', 'makefile')

# Windows device filenames may alias reserved handles; superscript digits count.
WIN_RESERVED = re.compile(r'^(?:CONIN\$|CONOUT\$|CON|PRN|AUX|NUL|COM[1-9¹²³]|LPT[1-9¹²³])(?:\.|$)', re.I)

class Denied(Exception):
    pass


def require(condition, detail):
    if not condition:
        raise Denied(detail)


def page_all(get, base, maximum=1000):
    out = []
    for page in range(1, maximum // 100 + 2):
        records = get(f'{base}?per_page=100&page={page}')
        require(isinstance(records, list), 'non-list paginated response')
        out.extend(records)
        require(len(out) <= maximum, 'pagination budget exceeded')
        if len(records) < 100:
            return out
    raise Denied('pagination incomplete')



def classify_changed_path(path):
    """Fail closed on nonportable PR file names and classify case-folded sensitive paths."""
    require(type(path) is str and 0 < len(path) <= 4096, 'invalid changed path type/size')
    parts = path.split('/')
    require(not path.startswith('/') and '\\' not in path and
            all(piece not in ('', '.', '..') and piece.casefold() != '.git' and
                not piece.endswith((' ', '.')) and not WIN_RESERVED.match(piece) and
                not any(char in '<>:"|?*' for char in piece)
                for piece in parts) and
            all(unicodedata.category(ch) not in ('Cc', 'Cf', 'Cs') for ch in path) and
            unicodedata.normalize('NFC', path) == path,
            'nonportable or ambiguous changed path')
    folded = path.casefold()
    return folded, any(folded.startswith(prefix.casefold()) for prefix in SENSITIVE)

def decision(event, repo, event_name, get):
    require(type(event) is dict, 'invalid GitHub event')
    require(repo == REPO, 'unexpected source repository')
    require(event_name in ('pull_request_target', 'pull_request_review'), 'unsupported event')
    event_pr = event.get('pull_request') or {}
    number = event_pr.get('number')
    require(type(number) is int and 1 <= number <= 1000000000, 'invalid PR ID')
    pr = get(f'repos/{REPO}/pulls/{number}')
    require(pr.get('number') == number, 'PR number mismatch')
    require(pr.get('state') == 'open' and not pr.get('draft'), 'PR not ready')
    head = pr.get('head') or {}
    base = pr.get('base') or {}
    head_sha = head.get('sha', '')
    base_sha = base.get('sha', '')
    require(bool(SHA.fullmatch(head_sha)) and bool(SHA.fullmatch(base_sha)), 'invalid exact SHA')
    require(head_sha == (event_pr.get('head') or {}).get('sha'), 'stale PR event head')
    require(base.get('ref') == 'main' and (base.get('repo') or {}).get('full_name') == REPO,
            'wrong PR base')
    require((head.get('repo') or {}).get('full_name') == REPO, 'cross-repo PR denied')
    branch = get(f'repos/{REPO}/branches/main')
    require(branch.get('protected') is True, 'main branch is not protected')
    require((branch.get('commit') or {}).get('sha') == base_sha, 'stale base commit')
    protection = get(f'repos/{REPO}/branches/main/protection')
    checks = (protection.get('required_status_checks') or {})
    reviews_rule = (protection.get('required_pull_request_reviews') or {})
    require(checks.get('strict') is True, 'strict required checks absent')
    listed_checks = checks.get('checks') or []
    trusted_checks = {item.get('context') for item in listed_checks
                      if isinstance(item, dict) and
                      type(item.get('app_id')) is int and item['app_id'] == TRUSTED_STATUS_APP_ID}
    require(REQUIRED_CHECKS <= trusted_checks,
            'five exact-source desktop checks not required with pinned app issuer')
    require(reviews_rule.get('dismiss_stale_reviews') is True,
            'stale review dismissal not enforced')
    require(reviews_rule.get('require_code_owner_reviews') is True,
            'CODEOWNER reviews not required by branch protection')
    require((protection.get('allow_force_pushes') or {}).get('enabled') is False,
            'force pushes permitted or unverifiable')
    require((protection.get('allow_deletions') or {}).get('enabled') is False,
            'branch deletion permitted or unverifiable')
    needed = reviews_rule.get('required_approving_review_count')
    require(type(needed) is int and 1 <= needed <= 100,
            'independent reviews not enforced')
    require((protection.get('enforce_admins') or {}).get('enabled') is True,
            'admin enforcement missing')
    files = page_all(get, f'repos/{REPO}/pulls/{number}/files', maximum=5000)
    require(type(pr.get('changed_files')) is int and pr['changed_files'] > 0 and
            len(files) == pr['changed_files'], 'incomplete PR files')
    sensitive = False
    seen = set()
    for record in files:
        require(type(record) is dict, 'malformed changed-file record')
        normalized, flag = classify_changed_path(record.get('filename'))
        require(normalized not in seen, 'duplicate or case-colliding changed path')
        seen.add(normalized)
        sensitive = sensitive or flag
        if 'previous_filename' in record:
            _, old_flag = classify_changed_path(record['previous_filename'])
            sensitive = sensitive or old_flag
    reviews = page_all(get, f'repos/{REPO}/pulls/{number}/reviews', maximum=1000)
    latest = {}
    for item in reviews:
        require(type(item) is dict, 'malformed review record')
        user_record = item.get('user') or {}
        require(type(user_record) is dict, 'malformed review user')
        user = user_record.get('login')
        require(type(user) is str and
                re.fullmatch(r'[A-Za-z0-9-]{1,39}', user) is not None,
                'invalid reviewer identity')
        latest[user.casefold()] = item
    author_name = (pr.get('user') or {}).get('login')
    require(type(author_name) is str and
            re.fullmatch(r'[A-Za-z0-9-]{1,39}', author_name) is not None,
            'invalid PR author identity')
    author = author_name.casefold()
    qualified = [r for login, r in latest.items()
                 if login != author and r.get('state') == 'APPROVED'
                 and r.get('commit_id') == head_sha]
    require(any((r.get('user') or {}).get('login', '').casefold() == OWNER.casefold()
                for r in qualified), 'current-head CODEOWNER approval missing')
    require(not any(login != author and review.get('state') == 'CHANGES_REQUESTED'
                    for login, review in latest.items()),
            'unresolved independent change-request review')
    require(len(qualified) >= max(needed, 2 if sensitive else 1),
            'current-head approval count below branch or sensitive-path quorum')
    for review in qualified:
        login = review['user']['login']
        require(re.fullmatch(r'[A-Za-z0-9-]{1,39}', login) is not None,
                'invalid reviewer identity')
        permission = get(f'repos/{REPO}/collaborators/{login}/permission')
        require(permission.get('permission') in ('write', 'maintain', 'admin'),
                'reviewer lacks verified write-level repository access')

    # Prevent a PR/head/base change partway through this multi-request audit
    # from being mistaken for an assessment of a single immutable snapshot.
    refreshed = get(f'repos/{REPO}/pulls/{number}')
    require(type(refreshed) is dict and
            refreshed.get('number') == number and
            refreshed.get('state') == 'open' and refreshed.get('draft') is False and
            (refreshed.get('head') or {}).get('sha') == head_sha and
            (refreshed.get('base') or {}).get('sha') == base_sha and
            refreshed.get('changed_files') == pr['changed_files'] and
            (refreshed.get('user') or {}).get('login', '').casefold() == author,
            'PR identity changed during metadata audit')
    last_branch = get(f'repos/{REPO}/branches/main')
    require(last_branch.get('protected') is True and
            (last_branch.get('commit') or {}).get('sha') == base_sha,
            'main protection or base changed during metadata audit')
    # Review submissions/dismissals may race with the multi-request metadata
    # read. A changed review feed is NOT a stable positive assessment.
    require(page_all(get, f'repos/{REPO}/pulls/{number}/reviews', maximum=1000) == reviews,
            'review state changed during metadata audit')
    return {'kind': 'METADATA_ONLY_NOT_RELEASE_APPROVAL', 'head_sha': head_sha,
            'base_sha': base_sha, 'sensitive_paths': sensitive,
            'qualified_review_count': len(qualified)}


def main():
    try:
        repo = os.environ['GITHUB_REPOSITORY']
        event_name = os.environ['GITHUB_EVENT_NAME']
        event = json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
        require('GH_TOKEN' in os.environ, 'missing GitHub token')
        def get(path):
            # Never shell-execute or interpolate event data into a command.
            result = subprocess.run(['gh', 'api', '--method', 'GET', path],
                                    capture_output=True, text=True, timeout=20, check=True)
            return json.loads(result.stdout)
        output = decision(event, repo, event_name, get)
        print('SOURCE_FREE_METADATA_POLICY_OK ' + json.dumps(output, sort_keys=True))
    except (Denied, KeyError, ValueError, OSError, subprocess.SubprocessError) as exc:
        print('SOURCE_FREE_METADATA_POLICY_NO_GO ' + str(exc), file=sys.stderr)
        sys.exit(2)

if __name__ == '__main__':
    main()
