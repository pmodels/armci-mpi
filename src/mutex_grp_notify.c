/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include <armci.h>
#include <armcix.h>
#include <debug.h>

#define ARMCI_MUTEX_TAG 100

/** Create a group of ARMCI mutexes.  Collective.
  *
  * @param[in] count Number of mutexes on the local process.
  * @return          Handle to the mutex group.
  */
armcix_mutex_grp_t ARMCIX_Create_mutexes_grp(int count) {
  int rank, nproc;
  armcix_mutex_grp_t grp;

  grp = malloc(sizeof(struct armcix_mutex_grp_s));
  assert(grp != NULL);

  MPI_Comm_dup(ARMCI_GROUP_WORLD.comm, &grp->comm);

  MPI_Comm_rank(grp->comm, &rank);
  MPI_Comm_size(grp->comm, &nproc);

  grp->count = count;

  if (count > 0) {
    grp->base = malloc(nproc*count); // FIXME: These can be unaligned
    memset(grp->base, 0, nproc*count);

  } else {
    grp->base = NULL;
  }

  MPI_Win_create(grp->base, nproc*count, 1, MPI_INFO_NULL, grp->comm, &grp->window);

  return grp;
}


/** Destroy a group of ARMCI mutexes.  Collective.
  *
  * @param[in] grp Handle to the group that should be destroyed.
  * @return        Zero on success, non-zero otherwise.
  */
int ARMCIX_Destroy_mutexes_grp(armcix_mutex_grp_t grp) {
  int ret;

  ret = MPI_Win_free(&grp->window);

  if (grp->base != NULL)
    free(grp->base);

  MPI_Comm_free(&grp->comm);

  free(grp);

  return ret;
}


/** Lock a mutex.
  * 
  * @param[in] grp   Mutex group that the mutex belongs to.
  * @param[in] mutex Desired mutex number [0..count-1]
  * @param[in] proc  Process where the mutex lives
  */
void ARMCIX_Lock_grp(armcix_mutex_grp_t grp, int mutex, int proc) {
  int       rank, nproc, already_locked, i;
  u_int8_t *buf;

  MPI_Comm_rank(grp->comm, &rank);
  MPI_Comm_size(grp->comm, &nproc);

  buf = malloc(nproc*sizeof(u_int8_t));
  assert(buf != NULL);

  buf[rank] = 1;

  /* Get all data from the lock_buf, except the byte belonging to
   * me. Set the byte belonging to me to 1. */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
  
  MPI_Put(&buf[rank], 1, MPI_BYTE, proc, (mutex*nproc) + rank, 1, MPI_BYTE, grp->window);

  /* Get data to the left of rank */
  if (rank > 0) {
    MPI_Get(buf, rank, MPI_BYTE, proc, mutex*nproc, rank, MPI_BYTE, grp->window);
  }

  /* Get data to the right of rank */
  if (rank < nproc - 1) {
    MPI_Get(&buf[rank+1], nproc-1-rank, MPI_BYTE, proc, mutex*nproc + rank + 1, nproc-1-rank, MPI_BYTE, grp->window);
  }
  
  MPI_Win_unlock(proc, grp->window);

  assert(buf[rank] == 1);

  for (i = already_locked = 0; i < nproc; i++)
    if (buf[i] && i != rank)
      already_locked = 1;

  /* Wait for notification */
  if (already_locked) {
    MPI_Status status;
    dprint(DEBUG_CAT_MUTEX, __func__, "waiting for notification [proc = %d, mutex = %d]\n", proc, mutex);
    MPI_Recv(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE, ARMCI_MUTEX_TAG+mutex, grp->comm, &status);
  }

  dprint(DEBUG_CAT_MUTEX, __func__, "lock acquired [proc = %d, mutex = %d]\n", proc, mutex);
  free(buf);
}


/** Attempt to lock a mutex (implemented as a blocking call).
  * 
  * @param[in] grp   Mutex group that the mutex belongs to.
  * @param[in] mutex Desired mutex number [0..count-1]
  * @param[in] proc  Process where the mutex lives
  * @return          0 on success, non-zero on failure
  */
int ARMCIX_Trylock_grp(armcix_mutex_grp_t grp, int mutex, int proc) {
  ARMCIX_Lock_grp(grp, mutex, proc);
  return 0;
}


/** Unlock a mutex.
  * 
  * @param[in] grp   Mutex group that the mutex belongs to.
  * @param[in] mutex Desired mutex number [0..count-1]
  * @param[in] proc  Process where the mutex lives
  */
void ARMCIX_Unlock_grp(armcix_mutex_grp_t grp, int mutex, int proc) {
  int       rank, nproc, i;
  u_int8_t *buf;

  MPI_Comm_rank(grp->comm, &rank);
  MPI_Comm_size(grp->comm, &nproc);

  buf = malloc(nproc*sizeof(u_int8_t));

  buf[rank] = 0;

  /* Get all data from the lock_buf, except the byte belonging to
   * me. Set the byte belonging to me to 0. */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
  
  MPI_Put(&buf[rank], 1, MPI_BYTE, proc, (mutex*nproc) + rank, 1, MPI_BYTE, grp->window);

  /* Get data to the left of rank */
  if (rank > 0) {
    MPI_Get(buf, rank, MPI_BYTE, proc, mutex*nproc, rank, MPI_BYTE, grp->window);
  }

  /* Get data to the right of rank */
  if (rank < nproc - 1) {
    MPI_Get(&buf[rank+1], nproc-1-rank, MPI_BYTE, proc, mutex*nproc + rank + 1, nproc-1-rank, MPI_BYTE, grp->window);
  }
  
  MPI_Win_unlock(proc, grp->window);

  assert(buf[rank] == 0);

  /* Notify the next waiting process */
  for (i = 0; i < nproc; i++) {
    if (buf[i] == 1) {
      dprint(DEBUG_CAT_MUTEX, __func__, "notifying %d [proc = %d, mutex = %d]\n", i, proc, mutex);
      MPI_Send(NULL, 0, MPI_BYTE, i, ARMCI_MUTEX_TAG+mutex, grp->comm);
      break;
    }
  }

  dprint(DEBUG_CAT_MUTEX, __func__, "lock released [proc = %d, mutex = %d]\n", proc, mutex);
  free(buf);
}
