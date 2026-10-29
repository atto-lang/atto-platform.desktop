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
import sys
from pathlib import Path

REPO = 'ores-truffle-oreslang/oreslang-platform.desktop'
OWNER = 'ORESoftware'
SHA = re.compile(r'^[a-f0-9]{40}$')
SENSITIVE = ('.github/', 'scripts/verify-checkout-raw.py',
             'scripts/test_', 'scripts/trusted_policy.py')

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


def decision(event, repo, event_name, get):
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
    require((reviews_rule.get('required_approving_review_count') or 0) >= 1,
            'independent reviews not enforced')
    require((protection.get('enforce_admins') or {}).get('enabled') is True,
            'admin enforcement missing')
    files = page_all(get, f'repos/{REPO}/pulls/{number}/files', maximum=5000)
    require(len(files) == pr.get('changed_files') and bool(files), 'incomplete PR files')
    sensitive = any(any(isinstance(f.get(key), str) and
                        any(f[key].startswith(prefix) for prefix in SENSITIVE)
                        for key in ('filename', 'previous_filename')) for f in files)
    require(all(isinstance(f.get('filename'), str) for f in files), 'malformed changed path')
    reviews = page_all(get, f'repos/{REPO}/pulls/{number}/reviews', maximum=1000)
    latest = {}
    for item in reviews:
        user = (item.get('user') or {}).get('login')
        if isinstance(user, str):
            latest[user.casefold()] = item
    author = (pr.get('user') or {}).get('login', '').casefold()
    qualified = [r for login, r in latest.items()
                 if login != author and r.get('state') == 'APPROVED'
                 and r.get('commit_id') == head_sha]
    require(any((r.get('user') or {}).get('login', '').casefold() == OWNER.casefold()
                for r in qualified), 'current-head CODEOWNER approval missing')
    require(len(qualified) >= (2 if sensitive else 1),
            'sensitive PR requires two independent current-head approvals')
    for review in qualified:
        login = review['user']['login']
        require(re.fullmatch(r'[A-Za-z0-9-]{1,39}', login) is not None,
                'invalid reviewer identity')
        permission = get(f'repos/{REPO}/collaborators/{login}/permission')
        require(permission.get('permission') in ('write', 'maintain', 'admin'),
                'reviewer lacks verified write-level repository access')
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
