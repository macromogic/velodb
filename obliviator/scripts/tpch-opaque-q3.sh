#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/tpch-common.sh"

tpch_require_tables
tpch_prepare_result_dir "tpch_q3_opaque" "${1:-}"

tpch_run_python_cached \
  "q3_prep_0" \
  "${tpch_artifact_root}/data/TPCH/figure11_q3_0.py" \
  /dev/shm/q3_c.txt \
  /dev/shm/q3_o.txt \
  /dev/shm/q3_l.txt

tpch_run_stage "q3_stage_1_1" "${tpch_artifact_root}/tpch_q3/opaque_1_1" "/dev/shm/q3_c.txt"
tpch_run_stage "q3_stage_1_2" "${tpch_artifact_root}/tpch_q3/opaque_1_2" "/dev/shm/q3_o.txt"
tpch_run_stage "q3_stage_1_3" "${tpch_artifact_root}/tpch_q3/opaque_1_3" "/dev/shm/q3_l.txt"

tpch_run_python_cached \
  "q3_prep_1" \
  "${tpch_artifact_root}/data/TPCH/figure11_q3_1.py" \
  /dev/shm/q3_2_1.txt
tpch_run_stage "q3_stage_2" "${tpch_artifact_root}/tpch_q3/opaque_2" "/dev/shm/q3_2_1.txt"

tpch_run_python_cached \
  "q3_prep_2" \
  "${tpch_artifact_root}/data/TPCH/figure11_q3_2.py" \
  /dev/shm/q3_2_2.txt \
  /dev/shm/q3_2_l.txt
tpch_run_stage "q3_stage_3" "${tpch_artifact_root}/tpch_q3/opaque_3" "/dev/shm/q3_2_2.txt"

tpch_copy_outputs "q3*"

if [[ -f /dev/shm/q3_2_2_output.txt ]]; then
  cp /dev/shm/q3_2_2_output.txt "${tpch_result_dir}/outputs/q3_final.txt"
fi

summary_path="${tpch_result_dir}/summary.txt"
tpch_write_summary_header "TPC-H Q3 Opaque" "${summary_path}"
tpch_append_stage_total "${summary_path}" "stage_1_1" "${tpch_result_dir}/q3_stage_1_1_run.log"
tpch_append_stage_total "${summary_path}" "stage_1_2" "${tpch_result_dir}/q3_stage_1_2_run.log"
tpch_append_stage_total "${summary_path}" "stage_1_3" "${tpch_result_dir}/q3_stage_1_3_run.log"
tpch_append_stage_total "${summary_path}" "stage_2" "${tpch_result_dir}/q3_stage_2_run.log"
tpch_append_stage_total "${summary_path}" "stage_3" "${tpch_result_dir}/q3_stage_3_run.log"
tpch_append_sum \
  "${summary_path}" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q3_stage_1_1_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q3_stage_1_2_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q3_stage_1_3_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q3_stage_2_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q3_stage_3_run.log")"

{
  echo
  echo "final_output: ${tpch_result_dir}/outputs/q3_final.txt"
} >> "${summary_path}"

echo "Logs and outputs written to: ${tpch_result_dir}"
