#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
artifact_root="$(cd "${script_dir}/.." && pwd)"
repo_root="$(cd "${artifact_root}/.." && pwd)"
image_name="${OBLIVIATOR_IMAGE_NAME:-obliviator-ae:ubuntu20.04}"
join_kks_variant="${OBLIVIATOR_JOIN_KKS_VARIANT:-join_kks_INT_INT}"
container_name="${OBLIVIATOR_CONTAINER_NAME:-obliviator-ae}"
build_arg_dcap="${INSTALL_AZ_DCAP_CLIENT:-0}"
tpch_data_dir="${OBLIVIATOR_TPCH_DATA_DIR:-${repo_root}/benchmark_tpch/data/sf_50}"
tpch_shm_dir="${OBLIVIATOR_TPCH_SHM_DIR:-${repo_root}/benchmark_tpch/shm}"

docker build \
  --build-arg "INSTALL_AZ_DCAP_CLIENT=${build_arg_dcap}" \
  -t "${image_name}" \
  -f "${script_dir}/Dockerfile" \
  "${script_dir}"

if [ ! -d "${tpch_shm_dir}/tpch" ]; then
  mkdir -p "${tpch_shm_dir}/tpch"
  echo '*' > "${tpch_shm_dir}/tpch/.gitignore"
fi

docker_args=(
  run
  --rm
  -it
  --name "${container_name}"
  -e "OBLIVIATOR_JOIN_KKS_VARIANT=${join_kks_variant}"
  -v "${artifact_root}:/root/obliviator"
  -v "${tpch_shm_dir}:/dev/shm"
  -w /root/obliviator
)

if [[ -d "${tpch_data_dir}" ]]; then
  docker_args+=(-v "${tpch_data_dir}:/dev/shm/tpch/dbgen")
fi

if [[ -e /dev/sgx_enclave ]]; then
  docker_args+=(--device /dev/sgx_enclave)
fi

if [[ -e /dev/sgx_provision ]]; then
  docker_args+=(--device /dev/sgx_provision)
fi

if [[ -S /var/run/aesmd/aesm.socket ]]; then
  docker_args+=(-v /var/run/aesmd:/var/run/aesmd)
fi

if [[ -S /run/aesmd/aesm.socket ]]; then
  docker_args+=(-v /run/aesmd:/run/aesmd)
fi

docker_args+=("${image_name}")

if [[ $# -gt 0 ]]; then
  docker_args+=("$@")
fi

exec docker "${docker_args[@]}"
