/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <debug.h>
#include <armci.h>
#include <armci_internals.h>

/** General ARMCI global operation (reduction).  Collective on group.
  *
  * @param[in]    scope Scope in which to perform the GOP (only SCOPE_ALL is supported)
  * @param[inout] x     Vector of n doubles, contains input and will contain output.
  * @param[in]    n     Length of x
  * @param[in]    op    One of '+', '*', 'max', 'min', 'absmax', 'absmin'
  * @param[in]    type  Data type of x
  * @param[in]    group Group on which to perform the GOP
  */
void armci_msg_group_gop_scope(int scope, void *x, int n, char *op, int type, ARMCI_Group *group) {
  void        *out;
  MPI_Op       mpi_op;
  MPI_Datatype mpi_type;
  MPI_Comm     comm;
  int          mpi_type_size;

  if (scope == SCOPE_ALL || scope == SCOPE_MASTERS)
    comm = group->comm;
  else
    comm = MPI_COMM_SELF;

  if (op[0] == '+') {
    mpi_op = MPI_SUM;
  } else if (op[0] == '*') {
    mpi_op = MPI_PROD;
  } else if (strncmp(op, "max", 3) == 0) {
    mpi_op = MPI_MAX;
  } else if (strncmp(op, "min", 3) == 0) {
    mpi_op = MPI_MIN;
  } else if (strncmp(op, "absmax", 6) == 0) {
    ARMCII_Error("absmax operation not supported", 10); // FIXME: Not supported
    return;
  } else if (strncmp(op, "absmin", 6) == 0) {
    ARMCII_Error("absmin operation not supported", 10); // FIXME: Not supported
    return;
  } else {
    ARMCII_Error("unknown operation", 100); // FIXME: Not supported
    return;
  }

  switch(type) {
    case ARMCI_INT:
      mpi_type = MPI_INT;
      break;
    case ARMCI_LONG:
      mpi_type = MPI_LONG;
      break;
    case ARMCI_LONG_LONG:
      mpi_type = MPI_LONG_LONG;
      break;
    case ARMCI_FLOAT:
      mpi_type = MPI_FLOAT;
      break;
    case ARMCI_DOUBLE:
      mpi_type = MPI_DOUBLE;
      break;
    default:
      ARMCII_Error("unknown type", 100); // FIXME: Not supported
      return;
  }

  MPI_Type_size(mpi_type, &mpi_type_size);

  out = malloc(n*mpi_type_size);
  assert(out != NULL);

  MPI_Allreduce(x, out, n, mpi_type, mpi_op, group->comm);

  memcpy(x, out, n*mpi_type_size);
  free(out);
}

void armci_msg_group_igop(int *x, int n, char *op, ARMCI_Group *group) {
  armci_msg_group_gop_scope(SCOPE_ALL, x, n, op, ARMCI_INT, group);
}

void armci_msg_group_lgop(long *x, int n, char *op, ARMCI_Group *group) {
  armci_msg_group_gop_scope(SCOPE_ALL, x, n, op, ARMCI_LONG, group);
}

void armci_msg_group_llgop(long long *x, int n, char *op, ARMCI_Group *group) {
  armci_msg_group_gop_scope(SCOPE_ALL, x, n, op, ARMCI_LONG_LONG, group);
}

void armci_msg_group_fgop(float *x, int n, char *op, ARMCI_Group *group) {
  armci_msg_group_gop_scope(SCOPE_ALL, x, n, op, ARMCI_FLOAT, group);
}

void armci_msg_group_dgop(double *x, int n, char *op, ARMCI_Group *group) {
  armci_msg_group_gop_scope(SCOPE_ALL, x, n, op, ARMCI_DOUBLE, group);
}

void armci_msg_gop_scope(int scope, void *x, int n, char *op, int type) {
  armci_msg_group_gop_scope(scope, x, n, op, type, &ARMCI_GROUP_WORLD);
}

void armci_msg_igop(int *x, int n, char *op) {
  armci_msg_gop_scope(SCOPE_ALL, x, n, op, ARMCI_INT);
}

void armci_msg_lgop(long *x, int n, char *op) {
  armci_msg_gop_scope(SCOPE_ALL, x, n, op, ARMCI_LONG);
}

void armci_msg_llgop(long long *x, int n, char *op) {
  armci_msg_gop_scope(SCOPE_ALL, x, n, op, ARMCI_LONG_LONG);
}

void armci_msg_fgop(float *x, int n, char *op) {
  armci_msg_gop_scope(SCOPE_ALL, x, n, op, ARMCI_FLOAT);
}

void armci_msg_dgop(double *x, int n, char *op) {
  armci_msg_gop_scope(SCOPE_ALL, x, n, op, ARMCI_DOUBLE);
}

