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


/** Initialize ARMCI.  MPI must be initialized before this can be called.  It
  * invalid to make ARMCI calls before initialization.  Collective on the world
  * group.
  *
  * @return            Zero on success
  */
int ARMCI_Init(void) {
  char *var;

  /* GA/TCGMSG end up calling ARMCI_Init() multiple times. */
  if (ARMCII_GLOBAL_STATE.init_count > 0) {
    ARMCII_GLOBAL_STATE.init_count++;
    return 0;
  }

  /* Check for MPI initialization */
  {
    int mpi_is_init, mpi_is_fin;
    MPI_Initialized(&mpi_is_init);
    MPI_Finalized(&mpi_is_fin);
    if (!mpi_is_init || mpi_is_fin) 
      ARMCII_Error("MPI must be initialized before calling ARMCI_Init");
  }

  /* Setup groups and communicators */

  MPI_Comm_dup(MPI_COMM_WORLD, &ARMCI_GROUP_WORLD.comm);
  MPI_Comm_rank(ARMCI_GROUP_WORLD.comm, &ARMCI_GROUP_WORLD.rank);
  MPI_Comm_size(ARMCI_GROUP_WORLD.comm, &ARMCI_GROUP_WORLD.size);

  ARMCI_GROUP_DEFAULT = ARMCI_GROUP_WORLD;

  /* Check for debugging flags. */

  var = getenv("ARMCI_DEBUG_ALLOC");
  if (var != NULL) ARMCII_GLOBAL_STATE.debug_alloc = 1;
  var = getenv("ARMCI_NO_FLUSH_BARRIERS");
  ARMCII_GLOBAL_STATE.debug_flush_barriers = (var == NULL) ? 1 : 0;
  var = getenv("ARMCI_DISABLE_IOV_CHECKS");
  if (var != NULL) ARMCII_GLOBAL_STATE.iov_checks_disabled = 1;
  var = getenv("ARMCI_NO_MPI_BOTTOM");
  if (var != NULL) ARMCII_GLOBAL_STATE.no_mpi_bottom = 1;
  var = getenv("ARMCI_VERBOSE");
  if (var != NULL) ARMCII_GLOBAL_STATE.verbose = 1;

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

  /* Set the IOV/strided transfer method */

  var = getenv("ARMCI_IOV_ONELOCK_LIMIT");
  if (var) 
    ARMCII_GLOBAL_STATE.iov_onelock_limit = (unsigned int) atoi(var);

  /* Set the IOV/strided transfer method */

  var = getenv("ARMCI_STRIDED_METHOD");

  ARMCII_GLOBAL_STATE.strided_method = ARMCII_STRIDED_IOV;

  if (var != NULL) {
    if (strcmp(var, "IOV") == 0)
      ARMCII_GLOBAL_STATE.strided_method = ARMCII_STRIDED_IOV;
    else if (strcmp(var, "DIRECT") == 0)
      ARMCII_GLOBAL_STATE.strided_method = ARMCII_STRIDED_DIRECT;
    else if (ARMCI_GROUP_WORLD.rank == 0)
      ARMCII_Warning("Ignoring unknown value for ARMCI_STRIDED_METHOD (%s)\n", var);
  }

  /* Shared buffer handling method */

  var = getenv("ARMCI_SHR_BUF_METHOD");

  ARMCII_GLOBAL_STATE.shr_buf_method = ARMCII_SHR_BUF_COPY;

  if (var != NULL) {
    if (strcmp(var, "COPY") == 0)
      ARMCII_GLOBAL_STATE.shr_buf_method = ARMCII_SHR_BUF_COPY;
    else if (strcmp(var, "NOGUARD") == 0)
      ARMCII_GLOBAL_STATE.shr_buf_method = ARMCII_SHR_BUF_NOGUARD;
    else if (ARMCI_GROUP_WORLD.rank == 0)
      ARMCII_Warning("Ignoring unknown value for ARMCI_SHR_BUF_METHOD (%s)\n", var);
  }

  /* NO_SEATBELTS Overrides some of the above options */

#ifdef NO_SEATBELTS
  ARMCII_GLOBAL_STATE.iov_checks_disabled = 1;
#endif

  /* Create GOP operators */

  MPI_Op_create(ARMCII_Absmin_op, 1 /* commute */, &MPI_ABSMIN_OP);
  MPI_Op_create(ARMCII_Absmax_op, 1 /* commute */, &MPI_ABSMAX_OP);

  ARMCII_GLOBAL_STATE.init_count++;

  if (ARMCII_GLOBAL_STATE.verbose) {
    if (ARMCI_GROUP_WORLD.rank == 0) {
      int major, minor;

      MPI_Get_version(&major, &minor);

      printf("ARMCI-MPI initialized with %d processes, MPI v%d.%d\n", ARMCI_GROUP_WORLD.size, major, minor);
#ifdef NO_SEATBELTS
      printf("  NO_SEATBELTS      = EPIC SPEED\n");
#endif
      printf("  STRIDED_METHOD    = %s\n", ARMCII_Strided_methods_str[ARMCII_GLOBAL_STATE.strided_method]);
      printf("  IOV_METHOD        = %s\n", ARMCII_Iov_methods_str[ARMCII_GLOBAL_STATE.iov_method]);

      if (   ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_ONELOCK
          || ARMCII_GLOBAL_STATE.iov_method == ARMCII_IOV_AUTO)
      {
        if (ARMCII_GLOBAL_STATE.iov_onelock_limit > 0)
          printf("  ONELOCK_LIMIT     = %d\n", ARMCII_GLOBAL_STATE.iov_onelock_limit);
        else
          printf("  ONELOCK_LIMIT     = UNLIMITED\n");
      }

      printf("  SHR_BUF_METHOD    = %s\n", ARMCII_Shr_buf_methods_str[ARMCII_GLOBAL_STATE.shr_buf_method]);
      printf("  DEBUG_ALLOC       = %s\n", ARMCII_GLOBAL_STATE.debug_alloc ? "TRUE" : "FALSE");
      printf("  NO_FLUSH_BARRIERS = %s\n", ARMCII_GLOBAL_STATE.debug_flush_barriers ? "FALSE" : "TRUE");
      printf("  IOV_CHECKS        = %s\n", ARMCII_GLOBAL_STATE.iov_checks_disabled ? "FALSE" : "TRUE");
      printf("  USE_MPI_BOTTOM    = %s\n", ARMCII_GLOBAL_STATE.no_mpi_bottom ? "FALSE" : "TRUE");
      printf("\n");
      fflush(NULL);
    }

    MPI_Barrier(ARMCI_GROUP_WORLD.comm);
  }

  return 0;
}


/** Initialize ARMCI.  MPI must be initialized before this can be called.  It
  * is invalid to make ARMCI calls before initialization.  Collective on the
  * world group.
  *
  * @param[inout] argc Command line argument count
  * @param[inout] argv Command line arguments
  * @return            Zero on success
  */
int ARMCI_Init_args(int *argc, char ***argv) {
  return ARMCI_Init();
}


/** Finalize ARMCI.  Must be called before MPI is finalized.  ARMCI calls are
  * not valid after finalization.  Collective on world group.
  *
  * @return            Zero on success
  */
int ARMCI_Finalize(void) {
  int nfreed;

  /* GA/TCGMSG end up calling ARMCI_Finalize() multiple times. */
  if (ARMCII_GLOBAL_STATE.init_count == 0) {
    return 0;
  }

  ARMCII_GLOBAL_STATE.init_count--;

  /* Only finalize on the last matching call */
  if (ARMCII_GLOBAL_STATE.init_count > 0) {
    return 0;
  }

  nfreed = mreg_destroy_all();

  if (nfreed > 0 && ARMCI_GROUP_WORLD.rank == 0)
    ARMCII_Warning("Freed %d leaked allocations\n", nfreed);

  /* Free GOP operators */

  MPI_Op_free(&MPI_ABSMIN_OP);
  MPI_Op_free(&MPI_ABSMAX_OP);

  ARMCI_Cleanup();

  MPI_Comm_free(&ARMCI_GROUP_WORLD.comm);

  return 0;
}


/** Cleaup ARMCI resources.  Call finalize instead.
  */
void ARMCI_Cleanup(void) {
  return;
}

