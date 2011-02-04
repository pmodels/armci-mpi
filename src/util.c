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
#include <mem_region.h>


int ARMCI_Init(void) {
  char *var;

  ARMCII_Assert_msg(!(ARMCII_GLOBAL_STATE.initialized), "ARMCI is already initialized");

  /* Check for MPI initialization */
  {
    int mpi_is_init;
    MPI_Initialized(&mpi_is_init);
    if (!mpi_is_init)
      ARMCII_Error("MPI must be initialized before calling ARMCI_Init");
  }

  /* Setup groups and communicators */

  MPI_Comm_dup(MPI_COMM_WORLD, &ARMCI_GROUP_WORLD.comm);
  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &ARMCI_GROUP_WORLD.rank);
  MPI_Comm_size(ARMCI_GROUP_WORLD.comm, &ARMCI_GROUP_WORLD.size);

  ARMCI_GROUP_DEFAULT = ARMCI_GROUP_WORLD;

  /* Set the IOV/strided transfer method */

  var = getenv("ARMCI_IOV_METHOD");

  ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_AUTO;

  if (var != NULL) {
    if (strcmp(var, "AUTO") == 0)
      ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_AUTO;
    else if (strcmp(var, "SAFE") == 0)
      ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_SAFE;
    else if (strcmp(var, "ONELOCK") == 0)
      ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_ONELOCK;
    else if (strcmp(var, "DTYPE") == 0)
      ARMCII_GLOBAL_STATE.iov_method = ARMCII_IOV_DTYPE;
    else if (ARMCI_GROUP_WORLD.rank == 0)
      ARMCII_Warning("Ignoring unknown value for ARMCI_IOV_METHOD (%s)\n", var);
  }

  /* Check for debugging flags. */
  var = getenv("ARMCI_DEBUG_ALLOC");
  if (var != NULL) ARMCII_GLOBAL_STATE.debug_alloc = 1;

  /* Initialize the Direct Local Access (DLA) state */

  ARMCII_GLOBAL_STATE.dla_state = ARMCII_DLA_CLOSED;
  ARMCII_GLOBAL_STATE.dla_mreg  = NULL;

  /* Create GOP operators */

  MPI_Op_create(ARMCII_Absmin_op, 1 /* commute */, &MPI_ABSMIN_OP);
  MPI_Op_create(ARMCII_Absmax_op, 1 /* commute */, &MPI_ABSMAX_OP);


  ARMCII_GLOBAL_STATE.initialized = 1;

  return 0;
}

int ARMCI_Init_args(int *argc, char ***argv) {
  return ARMCI_Init();
}

int ARMCI_Finalize(void) {
  int nfreed;

  ARMCII_Assert_msg(ARMCII_GLOBAL_STATE.initialized, "ARMCI has not been initialized");

  /* Free all remaining mem regions */

  nfreed = mreg_destroy_all();

  if (nfreed > 0 && ARMCI_GROUP_WORLD.rank == 0)
    ARMCII_Warning("Freed %d leaked allocations\n", nfreed);

  /* Free GOP operators */

  MPI_Op_free(&MPI_ABSMIN_OP);
  MPI_Op_free(&MPI_ABSMAX_OP);

  ARMCI_Cleanup();

  MPI_Comm_free(&ARMCI_GROUP_WORLD.comm);

  ARMCII_GLOBAL_STATE.initialized = 0;

  return 0;
}

void ARMCI_Cleanup(void) {
  return;
}

void ARMCI_Error(char *msg, int code) {
  fprintf(stderr, "[%d] ARMCI Error: %s\n", ARMCI_GROUP_WORLD.rank, msg);
  fflush(NULL);
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
