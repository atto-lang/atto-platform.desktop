#!/usr/bin/env bash
# Compatibility wrapper for the canonical .atto LLVM callback smoke.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd -P)"
exec bash "$root/scripts/llvm-link-smoke.sh" "$@"
