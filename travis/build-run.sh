#! /bin/sh

# Exit on error
set -ev

# Environment variables
export CFLAGS="-std=c99"
#export MPICH_CC=$CC
export MPICC=mpicc

# Configure and build
./autogen.sh
./configure --enable-g --disable-static

# Run unit tests
make check
