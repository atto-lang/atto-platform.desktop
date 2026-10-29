#!/usr/bin/env bash
# Read-only GitHub administration diagnostic; never authorizes a private-source export or merge.
set -euo pipefail

usage() {
  echo 'Usage: source-local-admin-probe.sh [CANARY_RUN_ID] [HOST_RUN_ID]' >&2
  echo 'Requires an existing authenticated gh session. All GitHub calls are GET.' >&2
}
if (( $# > 2 )); then usage; exit 2; fi
canary=${1:-40928128150}
host=${2:-40928128075}
if [[ ! "$canary" =~ ^[0-9]+$ || ! "$host" =~ ^[0-9]+$ ]]; then usage; exit 2; fi
if ! command -v gh >/dev/null 2>&1; then
  echo 'BLOCKED: GitHub CLI not installed or not available on PATH' >&2
  exit 2
fi

owner='ores-truffle-oreslang'
repos=(
  oreslang-platform.mobile oreslang-platform.iot oreslang-platform.embedded
  oreslang-platform.desktop oreslang-platform.fpga oreslang-platform.asic
  oreslang-platform-conformance
)

# Never print stderr from GitHub API: account identifiers or extensions may appear there.
# Each operation uses the explicitly read-only HTTP method and hardcoded repo paths.
read_field() {
  local endpoint=$1 query=$2
  gh api --method GET "$endpoint" --jq "$query" 2>/dev/null
}

printf 'Source-local metadata-only diagnostic (no checkout, no writes)\n'
for name in "${repos[@]}"; do
  endpoint="repos/$owner/$name"
  protected=$(read_field "$endpoint/branches/main" '.protected' || true)
  rulesets=$(read_field "$endpoint/rulesets" 'length' || true)
  protection=$(read_field "$endpoint/branches/main/protection" '.required_status_checks.strict' || true)
  [[ "$protected" == 'true' ]] || protected='UNPROTECTED/UNVERIFIED'
  [[ "$rulesets" =~ ^[0-9]+$ ]] || rulesets='UNAVAILABLE/UNVERIFIED'
  [[ "$protection" == 'true' ]] || protection='UNAVAILABLE/UNVERIFIED'
  printf '%s main=%s rulesets_count=%s strict_protection=%s\n' "$name" "$protected" "$rulesets" "$protection"
done

for entry in "canary:$canary" "host:$host"; do
  label=${entry%%:*}
  run=${entry#*:}
  endpoint="repos/$owner/oreslang-platform.desktop/actions/runs/$run"
  state=$(read_field "$endpoint" '[.id, .event, .head_sha, .status, .conclusion] | @tsv' || true)
  jobs=$(read_field "$endpoint/jobs?per_page=100" '.jobs[] | [.name, (.runner_id // 0), ((.steps // []) | length), .status, .conclusion] | @tsv' || true)
  printf '%s run=%s metadata=%s\n' "$label" "$run" "${state:-UNAVAILABLE}"
  if [[ -n "$jobs" ]]; then printf '%s\n' "$jobs"; else printf 'JOBS UNAVAILABLE/EMPTY\n'; fi
done

cat <<'EOF'
NO-GO: Metadata above does not prove billing eligibility, runner-admission cause,
independent approval, active protection enforcement or source-transfer authorization.
An authorized administrator must inspect organization Billing & licensing and Actions
settings and independently verify every required gate. Keep all repositories PRIVATE.
EOF
exit 2
