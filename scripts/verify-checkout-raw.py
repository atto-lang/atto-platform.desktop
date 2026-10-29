#!/usr/bin/env python3
"""Filter-free, read-only exact-HEAD checkout preflight for desktop host CI.

This checks checked-out bytes, not a trusted code-review or build provenance boundary.
"""
import hashlib
import os
from pathlib import Path
import stat
import subprocess
import sys

MAX_FILES = 25000
MAX_FILE_BYTES = 128 * 1024 * 1024
MAX_TOTAL_BYTES = 1024 * 1024 * 1024


def git(*args):
    return subprocess.run(['git', '--no-optional-locks', *args], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def fail(message):
    raise RuntimeError(message)


def parse_table(raw, index):
    records = {}
    for line in raw.split(b'\0'):
        if not line:
            continue
        if b'\t' not in line:
            fail('invalid Git object entry')
        header, path = line.split(b'\t', 1)
        fields = header.split(b' ')
        if index:
            if len(fields) != 3 or fields[2] != b'0':
                fail('non-stage-zero index entry')
            mode, sha = fields[:2]
        else:
            if len(fields) != 3 or fields[1] != b'blob':
                fail('non-regular tree entry')
            mode, sha = fields[0], fields[2]
        if mode not in (b'100644', b'100755'):
            fail('non-regular Git mode')
        if len(sha) != 40 or any(c not in b'0123456789abcdef' for c in sha):
            fail('invalid Git blob id')
        if not path or path.startswith((b'/', b'\\')) or b'\\' in path or b'\x00' in path:
            fail('invalid tracked path')
        parts = path.split(b'/')
        if any(part in (b'', b'.', b'..') or b':' in part for part in parts):
            fail('unsafe tracked path')
        if path in records:
            fail('duplicate tracked path')
        records[path] = (mode, sha)
        if len(records) > MAX_FILES:
            fail('file-count limit')
    return records


def check(expected):
    if len(expected) != 40 or any(c not in '0123456789abcdef' for c in expected):
        fail('expected commit must be a full lowercase SHA1')
    git_root = Path(os.fsdecode(git('rev-parse', '--show-toplevel').strip())).resolve()
    if os.path.normcase(str(git_root)) != os.path.normcase(str(Path.cwd().resolve())):
        fail('not at checkout root')
    if git('rev-parse', '--show-object-format').strip() != b'sha1':
        fail('unexpected Git object format')
    if git('rev-parse', '--verify', 'HEAD').strip().decode('ascii') != expected:
        fail('wrong source HEAD')
    tree = parse_table(git('ls-tree', '-r', '-z', '--full-tree', 'HEAD'), False)
    index = parse_table(git('ls-files', '--stage', '-z'), True)
    if tree != index or not tree:
        fail('index does not match the pinned HEAD tree')
    if git('ls-files', '--others', '--exclude-standard', '-z'):
        fail('untracked checkout paths')
    if git('ls-files', '--others', '--ignored', '--exclude-standard', '-z'):
        fail('ignored checkout paths')
    total = 0
    for raw_path, (mode, expected_blob) in tree.items():
        path = Path(os.fsdecode(raw_path))
        for parent in path.parents:
            if str(parent) != '.' and parent.is_symlink():
                fail('symlink parent component')
        before = path.lstat()
        if not stat.S_ISREG(before.st_mode) or stat.S_ISLNK(before.st_mode):
            fail('non-regular checkout file')
        if os.name != 'nt' and bool(before.st_mode & 0o111) != (mode == b'100755'):
            fail('executable-mode mismatch')
        if before.st_size > MAX_FILE_BYTES:
            fail('file byte limit')
        total += before.st_size
        if total > MAX_TOTAL_BYTES:
            fail('total-byte limit')
        h = hashlib.sha1()
        h.update(f'blob {before.st_size}\0'.encode('ascii'))
        with path.open('rb') as fd:
            while True:
                chunk = fd.read(1024 * 1024)
                if not chunk:
                    break
                h.update(chunk)
            after_fd = os.fstat(fd.fileno())
        after = path.lstat()
        identity = lambda st: (st.st_dev, st.st_ino, st.st_mode, st.st_size, st.st_mtime_ns, st.st_ctime_ns)
        if identity(before) != identity(after) or identity(before) != identity(after_fd):
            fail('checkout file changed during verification')
        if h.hexdigest().encode('ascii') != expected_blob:
            fail('raw checkout bytes do not match pinned HEAD blob')
    if (git('rev-parse', '--verify', 'HEAD').strip().decode('ascii') != expected or
        parse_table(git('ls-files', '--stage', '-z'), True) != tree or
        git('ls-files', '--others', '--exclude-standard', '-z') or
        git('ls-files', '--others', '--ignored', '--exclude-standard', '-z')):
        fail('checkout changed during verification')
    print(f'RAW_CHECKOUT_VERIFIED commit={expected} files={len(tree)} bytes={total}')


if __name__ == '__main__':
    try:
        check(os.environ.get('EXPECTED_SHA', ''))
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as exc:
        print(f'RAW_CHECKOUT_REJECTED: {exc}', file=sys.stderr)
        sys.exit(1)
