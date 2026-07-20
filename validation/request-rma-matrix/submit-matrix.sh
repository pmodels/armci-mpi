#!/bin/bash

set -euo pipefail

if [[ $# -ne 2 ]]; then
    echo "usage: $0 BUILD_DIRECTORY RESULT_DIRECTORY" >&2
    exit 2
fi

: "${SBATCH_ACCOUNT:?set SBATCH_ACCOUNT to the Slurm account to charge}"

build_root=$(cd "$1" && pwd -P)
mkdir -p "$2" "$2/slurm"
result_root=$(cd "$2" && pwd -P)
script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd -P)
partitions=${MATRIX_PARTITIONS:-"iris thor rome"}

for partition in $partitions; do
    sbatch --account="$SBATCH_ACCOUNT" --partition="$partition" \
        --array=0-19 --output="$result_root/slurm/$partition-%A-%a.out" \
        --export="ALL,MATRIX_BUILD_ROOT=$build_root,MATRIX_RESULTS_ROOT=$result_root" \
        "$script_dir/run-matrix.sbatch"
done
