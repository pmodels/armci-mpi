#!/usr/bin/env bash

set -euo pipefail

source_dir=${1:-/global/home/users/jehammond/Claude/armci-mpi}
output_root=${2:?output build root is required}
mpi_releases=${MPI_RELEASES:-/global/home/users/jehammond/MPI/external/mpi-releases}
hpcx_root=${HPCX_ROOT:-/global/software/rocky-9.x86_64/modules/gcc/11/hpcx/2.50}
vibempi_root=${VIBEMPI_ROOT:-/global/home/users/jehammond/Claude/VibeMPI}
source_commit=$(git -C "${source_dir}" rev-parse HEAD)

mkdir -p "${output_root}"

activate_stack()
{
    local stack=$1

    case "${stack}" in
        mpich5-ucx)
            source "${mpi_releases}/env/activate-mpich-5.0.1-ucx.sh" >/dev/null
            ;;
        mpich5-ofi)
            source "${mpi_releases}/env/activate-mpich-5.0.1-ofi.sh" >/dev/null
            ;;
        ompi4-ucx)
            export HPCX_ENABLE_OMPI4=1
            source "${hpcx_root}/hpcx-init-ompi.sh" >/dev/null
            hpcx_load >/dev/null
            ;;
        ompi5-ucx)
            source "${mpi_releases}/env/activate-openmpi-5.0.10-ucx.sh" >/dev/null
            ;;
        ompi5-ofi)
            source "${mpi_releases}/env/activate-openmpi-5.0.10-ofi.sh" >/dev/null
            ;;
        ompi6-ucx)
            source "${mpi_releases}/env/activate-openmpi-6.0.x-ucx.sh" >/dev/null
            ;;
        ompi6-ofi)
            source "${mpi_releases}/env/activate-openmpi-6.0.x-ofi.sh" >/dev/null
            ;;
        mvapich4-ucx)
            source "${mpi_releases}/env/activate-mvapich-4.1-ucx.sh" >/dev/null
            ;;
        mvapich4-ofi)
            source "${mpi_releases}/env/activate-mvapich-4.1-ofi.sh" >/dev/null
            ;;
        vibempi)
            export PATH="${vibempi_root}/install/bin:${PATH}"
            export LD_LIBRARY_PATH="${vibempi_root}/build:${vibempi_root}/build/_deps/libfabric-install/lib:${LD_LIBRARY_PATH:-}"
            export LD_PRELOAD="${vibempi_root}/build/libmpi.so"
            ;;
        *)
            echo "unknown stack: ${stack}" >&2
            return 2
            ;;
    esac
}

build_stack()
{
    local stack=$1
    local build_dir="${output_root}/${stack}"

    if [[ -e ${build_dir} ]]; then
        echo "refusing to overwrite existing build: ${build_dir}" >&2
        return 1
    fi

    mkdir -p "${build_dir}"
    git -C "${source_dir}" archive HEAD | tar -x -C "${build_dir}"

    (
        activate_stack "${stack}"
        cd "${build_dir}"
        printf '%s\n' "${source_commit}" > source-commit.txt
        if ! mpicc -show > mpicc-show.txt 2>&1; then
            mpicc --showme > mpicc-show.txt 2>&1 || true
        fi
        mpiexec --version > mpiexec-version.txt 2>&1 || true
        ./autogen.sh > autogen.log 2>&1
        ./configure MPICC=mpicc --with-progress=no > configure.log 2>&1
        make -j "${BUILD_JOBS:-8}" > build.log 2>&1
        make -j "${BUILD_JOBS:-8}" \
            tests/contrib/armci-test \
            tests/test_acc_overlap \
            tests/test_location_consistency >> build.log 2>&1
    )

    echo "built ${stack} at ${source_commit}"
}

stacks=${FOCUSED_STACKS:-"mpich5-ucx mpich5-ofi ompi4-ucx ompi5-ucx ompi5-ofi ompi6-ucx ompi6-ofi mvapich4-ucx mvapich4-ofi vibempi"}
for stack in ${stacks}
do
    build_stack "${stack}"
done
