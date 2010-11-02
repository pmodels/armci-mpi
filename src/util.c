/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <armci.h>
#include <armci_internals.h>
#include <debug.h>

int ARMCI_Init(void) {
  MPI_Comm_dup(MPI_COMM_WORLD, &ARMCI_GROUP_WORLD.comm);
  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &ARMCI_GROUP_WORLD.rank);
  MPI_Comm_size(ARMCI_GROUP_WORLD.comm, &ARMCI_GROUP_WORLD.size);
  return 0;
}

int ARMCI_Init_args(int *argc, char ***argv) {
  return ARMCI_Init();
}

int ARMCI_Finalize(void) {
  ARMCI_Cleanup();
  return 0;
}

void ARMCI_Cleanup(void) {
  return;
}

void ARMCI_Error(char *msg, int code) {
  fprintf(stderr, "ARMCI_Error: %s\n", msg);
  MPI_Abort(ARMCI_GROUP_WORLD.comm, code);
}

void ARMCI_Barrier(void) {
  ARMCI_AllFence();
  MPI_Barrier(ARMCI_GROUP_WORLD.comm);
}

/** \brief Wait for remote completion on one-sided operations targeting
  * process proc.
  *
  * In MPI-2, this is a no-op since get/put/acc already guarantee remote
  * completion.
  *
  * @param[in] proc Process to target
  */
void ARMCI_Fence(int proc) {
  return;
}

/** \brief Wait for remote completion on all one-sided operations.
  *
  * In MPI-2, this is a no-op since get/put/acc already guarantee remote
  * completion.
  */
void ARMCI_AllFence(void) {
  return;
}

int ARMCI_Uses_shm(void) {
  return 0;
}

void ARMCI_Set_shm_limit(unsigned long shmemlimit) {
  return;
}

int ARMCI_Uses_shm_grp(ARMCI_Group *group) {
  return 0;
}

/** Copy local data.
  *
  * @param[in]  src  Source buffer
  * @param[out] dst  Destination buffer
  * @param[in]  size Number of bytes to copy
  */
void ARMCI_Copy(void *src, void *dst, int size) {
  memcpy(dst, src, size);
}
