#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/tpch-common.sh"

tpch_require_tables
tpch_prepare_result_dir "tpch_q6_opaque" "${1:-}"

tpch_run_python_cached \
  "q6_prep_1" \
  "${tpch_artifact_root}/data/TPCH/figure11_q6_1.py" \
  /dev/shm/q6_1.txt
tpch_run_stage "q6_stage_1" "${tpch_artifact_root}/tpch_q6/opaque_1" "/dev/shm/q6_1.txt"

tpch_run_python_cached \
  "q6_prep_2" \
  "${tpch_artifact_root}/data/TPCH/figure11_q6_2.py" \
  /dev/shm/q6_2.txt
tpch_run_stage "q6_stage_2" "${tpch_artifact_root}/tpch_q6/opaque_2" "/dev/shm/q6_2.txt"

tpch_copy_outputs "q6*"

if [[ -f /dev/shm/q6_2_output.txt ]]; then
  cp /dev/shm/q6_2_output.txt "${tpch_result_dir}/outputs/q6_final.txt"
fi

summary_path="${tpch_result_dir}/summary.txt"
tpch_write_summary_header "TPC-H Q6 Opaque" "${summary_path}"
tpch_append_stage_total "${summary_path}" "stage_1" "${tpch_result_dir}/q6_stage_1_run.log"
tpch_append_stage_total "${summary_path}" "stage_2" "${tpch_result_dir}/q6_stage_2_run.log"
tpch_append_sum \
  "${summary_path}" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q6_stage_1_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q6_stage_2_run.log")"

{
  echo
  echo "final_output: ${tpch_result_dir}/outputs/q6_final.txt"
} >> "${summary_path}"

echo "Logs and outputs written to: ${tpch_result_dir}"
