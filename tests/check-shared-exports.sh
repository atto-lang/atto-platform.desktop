#!/bin/sh
# Linux ELF shared-library allowlist: only the two published host ABI symbols.
# This deliberately excludes the C11 SPSC host-internal mailbox functions.
set -eu
[ "$#" -eq 2 ] || { echo "usage: check-shared-exports.sh nm-tool shared-library" >&2; exit 2; }
nm_tool=$1
library=$2
[ -f "$library" ] || { echo "missing shared object: $library" >&2; exit 2; }
actual=$("$nm_tool" -D -g --defined-only "$library" | awk '{ print $NF }' | LC_ALL=C sort -u)
expected=$(printf '%s\n' ores_desktop_abi_version ores_desktop_run | LC_ALL=C sort -u)
if [ "$actual" != "$expected" ]; then
    printf 'UNEXPECTED_DESKTOP_SHARED_EXPORTS\nexpected:\n%s\nactual:\n%s\n' "$expected" "$actual" >&2
    exit 1
fi
echo "desktop shared library export allowlist: PASS"
