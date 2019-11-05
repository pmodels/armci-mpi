#! /bin/sh

# Exit on error
set -ev

os=`uname`
TRAVIS_ROOT="$1"
MPI_IMPL="$2"

# Environment variables
export CFLAGS="-std=c99 -Wall -Wextra"
#export MPICH_CC=$CC
export MPICC=mpicc

# PROGRESS is disabled for Linux testing
case "$os" in
    Darwin)
       PROGRESS=yes
        ;;
    Linux)
       PROGRESS=no
       export PATH=$TRAVIS_ROOT/mpich/bin:$PATH
       export PATH=$TRAVIS_ROOT/open-mpi/bin:$PATH
       ;;
esac

# Capture details of build
case "$MPI_IMPL" in
    mpich)
        mpichversion
        mpicc -show
        export HWLOC_COMPONENTS=no_os
        export OVERSUBSCRIBE=""
        ;;
    openmpi)
        # this is missing with Mac build it seems
        #ompi_info --arch --config
        mpicc --showme:command
        # see https://github.com/open-mpi/ompi/issues/2956
        # fixes issues e.g. https://travis-ci.org/jeffhammond/armci-mpi/jobs/211165004
        export TMPDIR=/tmp
        # see https://github.com/open-mpi/ompi/issues/6275
        # workaround Open-MPI 4.0.0 RMA bug
        export OMPI_MCA_osc=sm,pt2pt
        export OVERSUBSCRIBE="--oversubscribe"
        ;;
esac

# Configure and build
./autogen.sh
./configure --with-progress=$PROGRESS
make V=1

# Run unit tests
export ARMCI_VERBOSE=1
make check MPIEXEC="mpirun ${OVERSUBSCRIBE} -n 2"
make check MPIEXEC="mpirun ${OVERSUBSCRIBE} -n 4"
