#!/bin/bash

set -euo pipefail

if [[ $# -ne 3 ]]; then
    echo "usage: $0 MASTER_SOURCE PR_SOURCE OUTPUT_DIRECTORY" >&2
    exit 2
fi

master_source=$(cd "$1" && pwd -P)
pr_source=$(cd "$2" && pwd -P)
output_root=$3
mpi_releases=${MPI_RELEASES:-$HOME/MPI/external/mpi-releases}
hpcx_root=${HPCX_ROOT:-/global/software/rocky-9.x86_64/modules/gcc/11/hpcx/2.50}

mkdir -p "$output_root"
output_root=$(cd "$output_root" && pwd -P)

build_one()
{
    local tree=$1 stack=$2 source_dir=$3 build_dir=$4

    if [[ -e "$build_dir" ]]; then
        echo "refusing to overwrite existing build directory: $build_dir" >&2
        return 1
    fi

    mkdir -p "$build_dir"
    git -C "$source_dir" archive HEAD | tar -x -C "$build_dir"

    (
        case "$stack" in
            mpich5-ucx)
                source "$mpi_releases/env/activate-mpich-5.0.1-ucx.sh" >/dev/null
                ;;
            mpich5-ofi)
                source "$mpi_releases/env/activate-mpich-5.0.1-ofi.sh" >/dev/null
                ;;
            ompi5-ucx)
                source "$mpi_releases/env/activate-openmpi-5.0.10-ucx.sh" >/dev/null
                ;;
            ompi5-ofi)
                source "$mpi_releases/env/activate-openmpi-5.0.10-ofi.sh" >/dev/null
                ;;
            ompi4-ucx)
                export HPCX_ENABLE_OMPI4=1
                source "$hpcx_root/hpcx-init-ompi.sh"
                hpcx_load
                ;;
            *)
                echo "unknown stack: $stack" >&2
                return 2
                ;;
        esac

        cd "$build_dir"
        ./autogen.sh >matrix-autogen.log 2>&1
        ./configure MPICC=mpicc --with-progress=no >matrix-configure.log 2>&1
        make -j "${BUILD_JOBS:-8}" >matrix-build.log 2>&1
        make -j "${BUILD_JOBS:-8}" checkprogs >>matrix-build.log 2>&1
        git -C "$source_dir" rev-parse HEAD >matrix-source-commit.txt
        mpicc -show >matrix-mpicc-show.txt 2>&1 || mpicc --showme >matrix-mpicc-show.txt 2>&1
    )

    echo "built $tree/$stack in $build_dir"
}

stacks=(mpich5-ucx mpich5-ofi ompi5-ucx ompi5-ofi ompi4-ucx)
for tree in master pr; do
    if [[ $tree == master ]]; then
        source_dir=$master_source
    else
        source_dir=$pr_source
    fi

    for stack in "${stacks[@]}"; do
        build_one "$tree" "$stack" "$source_dir" \
            "$output_root/$tree-$stack"
    done
done
