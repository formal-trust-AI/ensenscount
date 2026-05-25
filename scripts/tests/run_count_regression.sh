#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN_PATH="${1:-${ROOT_DIR}/ensenscount}"
CASE_FILE="${2:-${ROOT_DIR}/tests/naive_regression_cases.tsv}"

COMMON_ARGS=( -g 2 -p 3 -s 2 -k 1 -M naive )

if [[ ! -x "${BIN_PATH}" ]]; then
  echo "Error: binary not found or not executable at ${BIN_PATH}" >&2
  exit 1
fi

if [[ ! -f "${CASE_FILE}" ]]; then
  echo "Error: test case file not found at ${CASE_FILE}" >&2
  exit 1
fi

echo "Running naive count regression tests"
echo "Binary: ${BIN_PATH}"
echo "Cases:  ${CASE_FILE}"
echo

pass_count=0
fail_count=0
case_count=0

while IFS=$'\t' read -r model_path expected_count; do
  [[ -z "${model_path}" ]] && continue
  [[ "${model_path}" =~ ^# ]] && continue

  case_count=$((case_count + 1))
  full_model_path="${ROOT_DIR}/${model_path}"

  if [[ ! -f "${full_model_path}" ]]; then
    echo "[FAIL] ${model_path} (missing model file)"
    fail_count=$((fail_count + 1))
    continue
  fi

  set +e
  cmd_output="$("${BIN_PATH}" -f "${full_model_path}" "${COMMON_ARGS[@]}" 2>&1)"
  cmd_status=$?
  set -e

  if [[ ${cmd_status} -ne 0 ]]; then
    echo "[FAIL] ${model_path} (ensenscount exited with ${cmd_status})"
    fail_count=$((fail_count + 1))
    continue
  fi

  actual_count="$(printf '%s\n' "${cmd_output}" | sed -n 's/^Total count (naive counting): //p' | tail -n1)"

  if [[ -z "${actual_count}" ]]; then
    echo "[FAIL] ${model_path} (could not parse count)"
    fail_count=$((fail_count + 1))
    continue
  fi

  if [[ "${actual_count}" == "${expected_count}" ]]; then
    echo "[PASS] ${model_path}: expected=${expected_count}, actual=${actual_count}"
    pass_count=$((pass_count + 1))
  else
    echo "[FAIL] ${model_path}: expected=${expected_count}, actual=${actual_count}"
    fail_count=$((fail_count + 1))
  fi
done < "${CASE_FILE}"

echo
if [[ ${fail_count} -eq 0 ]]; then
  echo "All tests passed (${pass_count}/${case_count})."
  exit 0
fi

echo "Tests failed: ${fail_count} failed, ${pass_count} passed, ${case_count} total."
exit 1
