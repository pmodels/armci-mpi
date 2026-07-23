#!/usr/bin/env bash

set -euo pipefail

account=${SBATCH_ACCOUNT:-nvidia}
root=${1:?sweep artifact root is required}
script_root=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
build_root="${root}/builds"
result_root="${root}/results"

mkdir -p "${result_root}" "${root}/slurm"

sbatch --account="${account}" --partition=iris \
    --array=0-28 \
    --output="${root}/slurm/iris-%A-%a.out" \
    --export="ALL,SWEEP_BUILD_ROOT=${build_root},SWEEP_RESULT_ROOT=${result_root}" \
    "${script_root}/run-focused-tests.sbatch"
