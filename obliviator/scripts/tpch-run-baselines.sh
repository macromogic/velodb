#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
artifact_root="$(cd "${script_dir}/.." && pwd)"
timestamp="$(date +%Y%m%d_%H%M%S)"
run_root="${1:-${artifact_root}/result/tpch_baselines_${timestamp}}"

mkdir -p "${run_root}"

stage_csv="${run_root}/stage-times.csv"
summary_csv="${run_root}/summary.csv"

printf 'family,query,stage,mode,threads,runtime\n' > "${stage_csv}"
printf 'family,query,threads,obl_total,nobl_total,slowdown\n' > "${summary_csv}"

extract_threads() {
  local summary_path="$1"
  awk -F': ' '/^Threads: /{print $2; exit}' "${summary_path}"
}

extract_query_total() {
  local summary_path="$1"
  awk -F': ' '/^query_total: /{print $2; exit}' "${summary_path}"
}

append_stage_rows() {
  local family="$1"
  local query="$2"
  local mode="$3"
  local summary_path="$4"
  local threads

  threads="$(extract_threads "${summary_path}")"
  awk -F': ' -v family="${family}" -v query="${query}" -v mode="${mode}" -v threads="${threads}" '
    /^stage_/ { printf "%s,%s,%s,%s,%s,%s\n", family, query, $1, mode, threads, $2 }
  ' "${summary_path}" >> "${stage_csv}"
}

run_one() {
  local family="$1"
  local mode="$2"
  local query="$3"
  local script_name="$4"
  local result_dir="${run_root}/${family}_${mode}/${query}"

  mkdir -p "${result_dir}"
  "${script_dir}/${script_name}" "${result_dir}"
  append_stage_rows "${family}" "${query}" "${mode}" "${result_dir}/summary.txt"
}

append_summary_row() {
  local family="$1"
  local query="$2"
  local obl_summary="$3"
  local nobl_summary="$4"
  local threads
  local obl_total
  local nobl_total
  local slowdown

  threads="$(extract_threads "${obl_summary}")"
  obl_total="$(extract_query_total "${obl_summary}")"
  nobl_total="$(extract_query_total "${nobl_summary}")"
  slowdown="$(awk -v obl="${obl_total}" -v nobl="${nobl_total}" '
    BEGIN {
      if (obl == "NA" || nobl == "NA" || nobl == 0) {
        print "NA"
      } else {
        printf "%.6f\n", obl / nobl
      }
    }
  ')"

  printf '%s,%s,%s,%s,%s,%s\n' \
    "${family}" "${query}" "${threads}" "${obl_total}" "${nobl_total}" "${slowdown}" \
    >> "${summary_csv}"
}

run_one "our" "obl" "q3" "tpch-q3.sh"
run_one "our" "nobl" "q3" "tpch-q3-nobl.sh"
run_one "our" "obl" "q5" "tpch-q5.sh"
run_one "our" "nobl" "q5" "tpch-q5-nobl.sh"
run_one "our" "obl" "q6" "tpch-q6.sh"
run_one "our" "nobl" "q6" "tpch-q6-nobl.sh"

run_one "opaque" "obl" "q3" "tpch-opaque-q3.sh"
run_one "opaque" "nobl" "q3" "tpch-opaque-q3-nobl.sh"
run_one "opaque" "obl" "q5" "tpch-opaque-q5.sh"
run_one "opaque" "nobl" "q5" "tpch-opaque-q5-nobl.sh"
run_one "opaque" "obl" "q6" "tpch-opaque-q6.sh"
run_one "opaque" "nobl" "q6" "tpch-opaque-q6-nobl.sh"

append_summary_row "our" "q3" "${run_root}/our_obl/q3/summary.txt" "${run_root}/our_nobl/q3/summary.txt"
append_summary_row "our" "q5" "${run_root}/our_obl/q5/summary.txt" "${run_root}/our_nobl/q5/summary.txt"
append_summary_row "our" "q6" "${run_root}/our_obl/q6/summary.txt" "${run_root}/our_nobl/q6/summary.txt"
append_summary_row "opaque" "q3" "${run_root}/opaque_obl/q3/summary.txt" "${run_root}/opaque_nobl/q3/summary.txt"
append_summary_row "opaque" "q5" "${run_root}/opaque_obl/q5/summary.txt" "${run_root}/opaque_nobl/q5/summary.txt"
append_summary_row "opaque" "q6" "${run_root}/opaque_obl/q6/summary.txt" "${run_root}/opaque_nobl/q6/summary.txt"

echo "Stage CSV: ${stage_csv}"
echo "Summary CSV: ${summary_csv}"
