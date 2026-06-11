#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/tpch-common.sh"

tpch_require_tables
tpch_prepare_result_dir "tpch_q5_opaque" "${1:-}"

tpch_run_python_cached \
  "q5_prep_1" \
  "${tpch_artifact_root}/data/TPCH/figure11_q5_1.py" \
  /dev/shm/q5_1_o.txt
tpch_run_stage "q5_stage_1" "${tpch_artifact_root}/tpch_q5/opaque_1" "/dev/shm/q5_1_o.txt"

tpch_run_python_cached \
  "q5_prep_2_1" \
  "${tpch_artifact_root}/data/TPCH/figure11_q5_2_1.py" \
  /dev/shm/q5_2_1.txt
tpch_run_stage "q5_stage_2_1" "${tpch_artifact_root}/tpch_q5/opaque_2" "/dev/shm/q5_2_1.txt"

tpch_run_python_cached \
  "q5_prep_2_2" \
  "${tpch_artifact_root}/data/TPCH/figure11_q5_2_2.py" \
  /dev/shm/q5_2_2.txt
tpch_run_stage "q5_stage_2_2" "${tpch_artifact_root}/tpch_q5/opaque_2" "/dev/shm/q5_2_2.txt"

tpch_run_python_cached \
  "q5_prep_2_3" \
  "${tpch_artifact_root}/data/TPCH/figure11_q5_2_3.py" \
  /dev/shm/q5_2_3.txt
tpch_run_stage "q5_stage_2_3" "${tpch_artifact_root}/tpch_q5/opaque_2" "/dev/shm/q5_2_3.txt"

tpch_run_python_cached \
  "q5_prep_2_4" \
  "${tpch_artifact_root}/data/TPCH/figure11_q5_2_4.py" \
  /dev/shm/q5_2_4.txt
tpch_run_stage "q5_stage_2_4" "${tpch_artifact_root}/tpch_q5/opaque_2" "/dev/shm/q5_2_4.txt"

tpch_run_python_cached \
  "q5_prep_2_5" \
  "${tpch_artifact_root}/data/TPCH/figure11_q5_2_5.py" \
  /dev/shm/q5_2_5.txt
tpch_run_stage "q5_stage_2_5" "${tpch_artifact_root}/tpch_q5/opaque_2" "/dev/shm/q5_2_5.txt"

tpch_run_python_cached \
  "q5_prep_3" \
  "${tpch_artifact_root}/data/TPCH/figure11_q5_3.py" \
  /dev/shm/q5_3.txt
tpch_run_stage "q5_stage_3" "${tpch_artifact_root}/tpch_q5/opaque_3" "/dev/shm/q5_3.txt"

tpch_run_python_cached \
  "q5_prep_4" \
  "${tpch_artifact_root}/data/TPCH/figure11_q5_4.py" \
  /dev/shm/q5_4.txt
tpch_run_stage "q5_stage_4" "${tpch_artifact_root}/tpch_q5/opaque_4" "/dev/shm/q5_4.txt"

tpch_copy_outputs "q5*"

if [[ -f /dev/shm/q5_4_output.txt ]]; then
  cp /dev/shm/q5_4_output.txt "${tpch_result_dir}/outputs/q5_final.txt"
fi

summary_path="${tpch_result_dir}/summary.txt"
tpch_write_summary_header "TPC-H Q5 Opaque" "${summary_path}"
tpch_append_stage_total "${summary_path}" "stage_1" "${tpch_result_dir}/q5_stage_1_run.log"
tpch_append_stage_total "${summary_path}" "stage_2_1" "${tpch_result_dir}/q5_stage_2_1_run.log"
tpch_append_stage_total "${summary_path}" "stage_2_2" "${tpch_result_dir}/q5_stage_2_2_run.log"
tpch_append_stage_total "${summary_path}" "stage_2_3" "${tpch_result_dir}/q5_stage_2_3_run.log"
tpch_append_stage_total "${summary_path}" "stage_2_4" "${tpch_result_dir}/q5_stage_2_4_run.log"
tpch_append_stage_total "${summary_path}" "stage_2_5" "${tpch_result_dir}/q5_stage_2_5_run.log"
tpch_append_stage_total "${summary_path}" "stage_3" "${tpch_result_dir}/q5_stage_3_run.log"
tpch_append_stage_total "${summary_path}" "stage_4" "${tpch_result_dir}/q5_stage_4_run.log"
tpch_append_sum \
  "${summary_path}" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q5_stage_1_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q5_stage_2_1_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q5_stage_2_2_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q5_stage_2_3_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q5_stage_2_4_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q5_stage_2_5_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q5_stage_3_run.log")" \
  "$(tpch_last_numeric_line "${tpch_result_dir}/q5_stage_4_run.log")"

{
  echo
  echo "final_output: ${tpch_result_dir}/outputs/q5_final.txt"
} >> "${summary_path}"

echo "Logs and outputs written to: ${tpch_result_dir}"
