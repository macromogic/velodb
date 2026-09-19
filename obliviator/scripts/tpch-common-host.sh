#!/usr/bin/env bash
set -euo pipefail

tpch_script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
tpch_artifact_root="$(cd "${tpch_script_dir}/.." && pwd)"
tpch_data_root="${OBLIVIATOR_TPCH_DBGEN_DIR:-/dev/shm/tpch/dbgen}"
tpch_skip_build="${OBLIVIATOR_SKIP_BUILD:-0}"
tpch_num_threads="${OBLIVIATOR_NUM_THREADS:-1}"
tpch_reuse_prep="${OBLIVIATOR_REUSE_PREP:-1}"
tpch_hostonly_target="${OBLIVIATOR_HOSTONLY_TARGET:-hostonly}"
tpch_hostonly_cflags="${OBLIVIATOR_HOSTONLY_CFLAGS:--march=native -mno-avx512f -O3 -Wall -Wextra -Werror -DOE_SIMULATION_CERT -include string.h -include stdbool.h}"

tpch_require_tables() {
  local table
  for table in customer orders lineitem supplier nation region; do
    if [[ ! -f "${tpch_data_root}/${table}.tbl" ]]; then
      echo "Missing TPC-H table: ${tpch_data_root}/${table}.tbl" >&2
      exit 1
    fi
  done
}

tpch_prepare_result_dir() {
  local query_name="$1"
  local custom_dir="${2:-}"
  if [[ -n "${custom_dir}" ]]; then
    tpch_result_dir="${custom_dir}"
  else
    tpch_result_dir="${tpch_artifact_root}/result/${query_name}_$(date +%Y%m%d_%H%M%S)"
  fi
  mkdir -p "${tpch_result_dir}"
}

tpch_run_python() {
  local label="$1"
  local script_path="$2"
  local log_path="${tpch_result_dir}/${label}.log"

  echo "[prep] ${label}" | tee "${log_path}"
  python3 "${script_path}" 2>&1 | tee -a "${log_path}"
}

tpch_run_python_cached() {
  local label="$1"
  local script_path="$2"
  shift 2
  local log_path="${tpch_result_dir}/${label}.log"
  local output_path
  local should_run=0

  if [[ "$#" -eq 0 ]]; then
    should_run=1
  elif [[ "${tpch_reuse_prep}" != "1" ]]; then
    should_run=1
  else
    for output_path in "$@"; do
      if [[ ! -f "${output_path}" ]]; then
        should_run=1
        break
      fi
    done
  fi

  if [[ "${should_run}" -eq 0 ]]; then
    {
      echo "[prep] ${label}"
      echo "Reusing existing outputs:"
      printf '  %s\n' "$@"
    } | tee "${log_path}"
    return
  fi

  tpch_run_python "${label}" "${script_path}"
}

tpch_run_stage() {
  local label="$1"
  local stage_dir="$2"
  local input_path="$3"
  local build_log="${tpch_result_dir}/${label}_build.log"
  local run_log="${tpch_result_dir}/${label}_run.log"
  local status

  if [[ "${tpch_skip_build}" != "1" ]]; then
    echo "[build-host] ${label}" | tee "${build_log}"
    (
      cd "${stage_dir}"
      make clean \
        HOSTONLY_DEP= \
        COMMON_DEPS= \
        HOST_DEPS= \
        ENCLAVE_DEPS= \
        BASELINE_DEPS=
      make "${tpch_hostonly_target}" \
        CFLAGS="${tpch_hostonly_cflags}" \
        HOSTONLY_DEP= \
        COMMON_DEPS= \
        HOST_DEPS= \
        ENCLAVE_DEPS= \
        BASELINE_DEPS=
    ) 2>&1 | tee -a "${build_log}"
  fi

  echo "[run-host] ${label} <- ${input_path}" | tee "${run_log}"
  set +e
  (
    cd "${stage_dir}"
    "./${tpch_hostonly_target}" hostonly "${tpch_num_threads}" "${input_path}"
  ) 2>&1 | tee -a "${run_log}"
  status=$?
  set -e

  if [[ "${status}" -ne 0 && "${status}" -ne 1 ]]; then
    echo "Stage ${label} failed with exit code ${status}" | tee -a "${run_log}" >&2
    return "${status}"
  fi

  if ! awk '/^[0-9]+(\.[0-9]+)?$/{found=1} END{exit !found}' "${run_log}"; then
    echo "Stage ${label} did not emit a numeric timing line" | tee -a "${run_log}" >&2
    return 1
  fi
}

tpch_last_numeric_line() {
  local file_path="$1"
  awk '/^[0-9]+(\.[0-9]+)?$/{value=$0} END{if (value != "") print value; else print "NA"}' "${file_path}"
}

tpch_copy_outputs() {
  local pattern="$1"
  local target_dir="${tpch_result_dir}/outputs"
  mkdir -p "${target_dir}"
  find /dev/shm -maxdepth 1 -type f -name "${pattern}" -exec cp {} "${target_dir}/" \;
}

tpch_write_summary_header() {
  local query_name="$1"
  local summary_path="$2"
  {
    echo "Query: ${query_name}"
    echo "Generated: $(date -Is)"
    echo "TPC-H tables: ${tpch_data_root}"
    echo "Threads: ${tpch_num_threads}"
    echo "Mode: host-only"
    echo "Host-only target: ${tpch_hostonly_target}"
    echo "Host-only CFLAGS: ${tpch_hostonly_cflags}"
    echo "Reuse prep outputs: ${tpch_reuse_prep}"
    echo
  } > "${summary_path}"
}

tpch_append_stage_total() {
  local summary_path="$1"
  local stage_name="$2"
  local run_log="$3"
  local stage_total
  stage_total="$(tpch_last_numeric_line "${run_log}")"
  echo "${stage_name}: ${stage_total}" >> "${summary_path}"
}

tpch_append_sum() {
  local summary_path="$1"
  shift
  local total
  total="$(
    awk '
      BEGIN { sum = 0; ok = 1 }
      {
        if ($1 == "NA") {
          ok = 0
        } else {
          sum += $1
        }
      }
      END {
        if (ok) {
          printf "%.6f\n", sum
        } else {
          print "NA"
        }
      }
    ' < <(printf "%s\n" "$@")
  )"
  echo >> "${summary_path}"
  echo "query_total: ${total}" >> "${summary_path}"
}
