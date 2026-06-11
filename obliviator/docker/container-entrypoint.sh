#!/bin/bash
set -euo pipefail

artifact_root="/root/obliviator"
join_kks_variant="${OBLIVIATOR_JOIN_KKS_VARIANT:-join_kks_INT_INT}"

export PKG_CONFIG_PATH="${PKG_CONFIG_PATH:-}"
export CMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-}"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"
export LIBRARY_PATH="${LIBRARY_PATH:-}"
export CPATH="${CPATH:-}"

if [[ -f /opt/openenclave/share/openenclave/openenclaverc ]]; then
    set +u
    # shellcheck disable=SC1091
    source /opt/openenclave/share/openenclave/openenclaverc
    set -u
fi

if [[ -f /opt/intel/sgxsdk/environment ]]; then
    set +u
    # shellcheck disable=SC1091
    source /opt/intel/sgxsdk/environment
    set -u
fi

if [[ -d "${artifact_root}" ]]; then
    ln -sfn "${artifact_root}" /root/Parallel-join

    if [[ ! -e "${artifact_root}/join_kks" && -d "${artifact_root}/${join_kks_variant}" ]]; then
        ln -sfn "${artifact_root}/${join_kks_variant}" "${artifact_root}/join_kks"
    fi
fi

exec "$@"
