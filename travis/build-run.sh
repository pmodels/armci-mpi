#! /bin/sh

# Exit on error
set -ev

os=`uname`
TRAVIS_ROOT="$1"
MPI_IMPL="$2"

# Environment variables
export CFLAGS="-std=c99"
#export MPICH_CC=$CC
export MPICC=mpicc

case "$os" in
    Darwin)
        ;;
    Linux)
       export PATH=$TRAVIS_ROOT/mpich/bin:$PATH
       export PATH=$TRAVIS_ROOT/open-mpi/bin:$PATH
       ;;
esac

# Capture details of build
case "$MPI_IMPL" in
    mpich)
        mpichversion
        mpicc -show
        ;;
    openmpi)
        # this is missing with Mac build it seems
        #ompi_info --arch --config
        mpicc --showme:command
        # see https://github.com/open-mpi/ompi/issues/2956
        # fixes issues e.g. https://travis-ci.org/jeffhammond/armci-mpi/jobs/211165004
        export TMPDIR=/tmp
        ;;
esac

# Configure and build
./autogen.sh
./configure --disable-static --enable-win-allocate

# Run unit tests
export HWLOC_COMPONENTS=no_os
export ARMCI_VERBOSE=1
make check
