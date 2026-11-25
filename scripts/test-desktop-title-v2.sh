#!/usr/bin/env bash
# Host-only verification of title ABI v2, using a real C compiler.
# No AttoLang compiler, SDL3 display server, secrets or remote fetches.
set -euo pipefail
if [ "$#" -ne 1 ]; then
  echo "Usage: test-desktop-title-v2.sh /absolute/path/to/cc-or-clang" >&2
  exit 64
fi
compiler="$1"
if [[ "$compiler" != /* || ! -x "$compiler" ]]; then
  echo "Compiler must be an absolute executable path" >&2
  exit 64
fi
root="$(cd "$(dirname "$0")/.." && pwd -P)"
work="$(mktemp -d)"
trap 'rm -rf -- "$work"' EXIT
"$compiler" -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I"$root/include" \
  "$root/src/ores_desktop.c" \
  "$root/tests/test_desktop_title_v2.c" \
  -o "$work/title-v2"
"$work/title-v2"
