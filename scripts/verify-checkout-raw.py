#!/usr/bin/env python3
"""Filter-free, read-only exact-HEAD checkout preflight for desktop CI.

Git's configured content filters must not execute during integrity verification.
This is a point-in-time workspace check, NOT a trusted review root or an immutable
build sandbox. Verified files must still be built on a controlled runner.
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
REPARSE_POINT = getattr(stat, 'FILE_ATTRIBUTE_REPARSE_POINT', 0x400)


def git(*args):
    return subprocess.run(['git', '-c', 'core.fsmonitor=false', '--no-optional-locks', *args], check=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE).stdout


def fail(message):
    raise RuntimeError(message)


def valid_path(path):
    if not path or path.startswith((b'/', b'\\')) or b'\\' in path or b'\x00' in path:
        fail('unsafe tracked path')
    parts = path.split(b'/')
    for part in parts:
        if part in (b'', b'.', b'..') or any(c in part for c in b'<>:"|?*'):
            fail('unsafe tracked path')
        if part.endswith((b'.', b' ')) or any(c < 32 for c in part):
            fail('unsafe Windows path')
        # Names that alias Windows devices are never valid portable source files.
        base = part.split(b'.', 1)[0].upper()
        if base in (b'CON', b'PRN', b'AUX', b'NUL') or base in {
            b'COM' + bytes([n]) for n in range(49, 58)
        } or base in {b'LPT' + bytes([n]) for n in range(49, 58)}:
            fail('reserved Windows device name')
    return parts


def parse_table(raw, index):
    records = {}
    folded = set()
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
        valid_path(path)
        folded_path = os.fsdecode(path).casefold()
        if path in records or folded_path in folded:
            fail('duplicate/case-colliding tracked path')
        folded.add(folded_path)
        records[path] = (mode, sha)
        if len(records) > MAX_FILES:
            fail('file-count limit')
    return records


def verify_index_flags(expected_paths):
    """Do not accept assume-unchanged / skip-worktree concealment flags."""
    raw = git('ls-files', '-v', '-z')
    paths = set()
    for record in raw.split(b'\0'):
        if not record:
            continue
        if not record.startswith(b'H '):
            fail('hidden or unexpected Git index flags')
        path = record[2:]
        if path in paths:
            fail('duplicate Git index flags entry')
        paths.add(path)
    if paths != set(expected_paths):
        fail('Git index flags differ from HEAD')


def identity(s):
    # On Windows, st_ino may be zero, but still compare metadata and raw SHA.
    return (s.st_dev, s.st_ino, s.st_mode, s.st_size, s.st_mtime_ns, s.st_ctime_ns)


def _windows_final_path(fd):
    """Absolute resolved file path from the actual Windows kernel handle."""
    import ctypes
    import msvcrt
    from ctypes import wintypes
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    fn = kernel32.GetFinalPathNameByHandleW
    fn.argtypes = [wintypes.HANDLE, wintypes.LPWSTR, wintypes.DWORD, wintypes.DWORD]
    fn.restype = wintypes.DWORD
    handle = msvcrt.get_osfhandle(fd)
    buf = ctypes.create_unicode_buffer(32768)
    n = fn(handle, buf, len(buf), 0)
    if not n or n >= len(buf):
        fail('cannot resolve Windows file handle')
    path = buf.value
    if path.startswith('\\\\?\\UNC\\'):
        path = '\\\\' + path[8:]
    elif path.startswith('\\\\?\\'):
        path = path[4:]
    return os.path.normcase(os.path.normpath(path))


def open_tracked(raw_path, root_fd):
    """Return a no-follow file descriptor anchored to the verified checkout.

    POSIX uses dirfd + O_NOFOLLOW for *every* path component. Windows uses
    native reparse-point-open flags and validates the handle's final path.
    """
    parts = valid_path(raw_path)
    if os.name != 'nt':
        directory = os.dup(root_fd)
        try:
            flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | getattr(os, 'O_CLOEXEC', 0)
            for part in parts[:-1]:
                next_dir = os.open(os.fsdecode(part), flags, dir_fd=directory)
                os.close(directory)
                directory = next_dir
            leaf_flags = os.O_RDONLY | os.O_NOFOLLOW | getattr(os, 'O_CLOEXEC', 0) | os.O_NONBLOCK
            return os.open(os.fsdecode(parts[-1]), leaf_flags, dir_fd=directory)
        finally:
            os.close(directory)

    # Windows CreateFileW(FILE_FLAG_OPEN_REPARSE_POINT) does not follow a
    # final-component symlink/junction. Compare kernel-resolved final file
    # identity with the original checkout-relative path to catch parent swaps.
    import ctypes
    import msvcrt
    from ctypes import wintypes
    if not hasattr(ctypes, 'WinDLL'):
        fail('Windows native no-follow API unavailable')
    candidate = Path.cwd().joinpath(*[os.fsdecode(p) for p in parts])
    for parent in (candidate, *candidate.parents):
        if parent == Path.cwd().parent:
            break
        st = parent.lstat()
        if st.st_file_attributes & REPARSE_POINT:
            fail('Windows reparse point in source path')
    kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
    create = kernel32.CreateFileW
    create.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                       wintypes.LPVOID, wintypes.DWORD, wintypes.DWORD, wintypes.HANDLE]
    create.restype = wintypes.HANDLE
    GENERIC_READ = 0x80000000
    FILE_SHARE_ALL = 0x00000007
    OPEN_EXISTING = 3
    FILE_FLAG_OPEN_REPARSE_POINT = 0x00200000
    FILE_FLAG_BACKUP_SEMANTICS = 0x02000000
    handle = create(str(candidate), GENERIC_READ, FILE_SHARE_ALL, None,
                    OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, None)
    invalid = ctypes.c_void_p(-1).value
    if handle == invalid or handle is None:
        fail('cannot open Windows tracked path safely')
    try:
        fd = msvcrt.open_osfhandle(handle, os.O_RDONLY | os.O_BINARY)
    except BaseException:
        kernel32.CloseHandle(wintypes.HANDLE(handle))
        raise
    try:
        expected = os.path.normcase(os.path.normpath(str(candidate)))
        if _windows_final_path(fd) != expected:
            fail('Windows source path was redirected through reparse point')
        if os.fstat(fd).st_file_attributes & REPARSE_POINT:
            fail('Windows reparse-point file')
        return fd
    except BaseException:
        os.close(fd)
        raise


def check(expected):
    if len(expected) != 40 or any(c not in '0123456789abcdef' for c in expected):
        fail('expected commit must be a full lowercase SHA1')
    root = Path.cwd().resolve()
    git_root = Path(os.fsdecode(git('rev-parse', '--show-toplevel').strip())).resolve()
    if os.path.normcase(str(git_root)) != os.path.normcase(str(root)):
        fail('not at checkout root')
    if git('rev-parse', '--show-object-format').strip() != b'sha1':
        fail('unexpected Git object format')
    if git('rev-parse', '--verify', 'HEAD').strip().decode('ascii') != expected:
        fail('wrong source HEAD')
    tree = parse_table(git('ls-tree', '-r', '-z', '--full-tree', 'HEAD'), False)
    index = parse_table(git('ls-files', '--stage', '-z'), True)
    if tree != index or not tree:
        fail('index does not match pinned HEAD tree')
    verify_index_flags(tree)
    if git('ls-files', '--others', '--exclude-standard', '-z'):
        fail('untracked checkout paths')
    if git('ls-files', '--others', '--ignored', '--exclude-standard', '-z'):
        fail('ignored checkout paths')

    root_fd = None
    if os.name != 'nt':
        root_fd = os.open('.', os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW)
    try:
        total = 0
        for raw_path, (mode, expected_blob) in tree.items():
            path = Path(os.fsdecode(raw_path))
            before = path.lstat()
            if not stat.S_ISREG(before.st_mode) or stat.S_ISLNK(before.st_mode):
                fail('non-regular checkout file')
            if os.name == 'nt' and before.st_file_attributes & REPARSE_POINT:
                fail('Windows reparse-point file')
            if os.name != 'nt' and bool(before.st_mode & 0o111) != (mode == b'100755'):
                fail('executable-mode mismatch')
            if before.st_size > MAX_FILE_BYTES:
                fail('file byte limit')
            total += before.st_size
            if total > MAX_TOTAL_BYTES:
                fail('total-byte limit')
            h = hashlib.sha1()
            h.update(f'blob {before.st_size}\0'.encode('ascii'))
            fd = open_tracked(raw_path, root_fd)
            try:
                before_fd = os.fstat(fd)
                if not stat.S_ISREG(before_fd.st_mode) or identity(before) != identity(before_fd):
                    fail('tracked path changed between stat and open')
                with os.fdopen(fd, 'rb', closefd=False) as stream:
                    while True:
                        chunk = stream.read(1024 * 1024)
                        if not chunk:
                            break
                        h.update(chunk)
                after_fd = os.fstat(fd)
            finally:
                os.close(fd)
            after = path.lstat()
            if identity(before) != identity(after) or identity(before) != identity(after_fd):
                fail('checkout file changed during verification')
            if h.hexdigest().encode('ascii') != expected_blob:
                fail('raw checkout bytes do not match pinned HEAD blob')
    finally:
        if root_fd is not None:
            os.close(root_fd)
    if (git('rev-parse', '--verify', 'HEAD').strip().decode('ascii') != expected or
        parse_table(git('ls-files', '--stage', '-z'), True) != tree or
        git('ls-files', '--others', '--exclude-standard', '-z') or
        git('ls-files', '--others', '--ignored', '--exclude-standard', '-z')):
        fail('checkout changed during verification')
    verify_index_flags(tree)
    print(f'RAW_CHECKOUT_VERIFIED commit={expected} files={len(tree)} bytes={total}')


if __name__ == '__main__':
    try:
        check(os.environ.get('EXPECTED_SHA', ''))
    except (OSError, RuntimeError, subprocess.CalledProcessError, ValueError) as exc:
        print(f'RAW_CHECKOUT_REJECTED: {exc}', file=sys.stderr)
        sys.exit(1)
