#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include "debug.h"
#include "armci.h"

/** Query process rank from messaging (MPI) layer.
  */
int armci_msg_me() {
  int me;
  MPI_Comm_rank(MPI_COMM_WORLD, &me);
  return me;
}


/** Query number of processes.
  */
int armci_msg_nproc() {
  int nproc;
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  return nproc;
}


/** Broadcast a message.  Collective.
  *
  * @param[in] buffer Source buffer on root, destination elsewhere.
  * @param[in] len    Length of the message in bytes.
  * @param[in] root   Rank of the root process.
  */
void armci_msg_bcast(void* buffer, int len, int root) {
  MPI_Bcast(buffer, len, MPI_BYTE, root, MPI_COMM_WORLD);
}


/** ARMCI double precision global operation.  Collective.
  *
  * @param[inout] x  Vector of n doubles, contains input and will contain output.
  * @param[in]    n  Length of x
  * @param[in]    op One of '+', '*', 'max', 'min', 'absmax', 'absmin'
  */
void armci_msg_dgop(double x[], int n, char *op) {
  double *out;
  MPI_Op  mpi_op;

  out = malloc(n*sizeof(double));
  assert(out != NULL);

  if (op[0] == '+') {
    mpi_op = MPI_SUM;
  } else if (op[0] == '*') {
    mpi_op = MPI_PROD;
  } else if (strcmp(op, "max") == 0) {
    mpi_op = MPI_MAX;
  } else if (strcmp(op, "min") == 0) {
    mpi_op = MPI_MIN;
  } else if (strcmp(op, "absmax") == 0) {
    assert(0); // FIXME
  } else if (strcmp(op, "absmin") == 0) {
    assert(0); // FIXME
  } else {
    assert(0);
  }

  MPI_Allreduce(x, out, n, MPI_DOUBLE, mpi_op, MPI_COMM_WORLD);

  memcpy(x, out, n*sizeof(double));
  free(out);
}
