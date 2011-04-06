/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#include <armci.h>
#include <armci_internals.h>
#include <armcix.h>
#include <debug.h>

#define ARMCI_MUTEX_TAG 100

/** Create a group of ARMCI mutexes.  Collective onthe ARMCI group.
  *
  * @param[in] count  Number of mutexes on the local process.
  * @param[in] pgroup ARMCI group on which to create mutexes
  * @return           Handle to the mutex group.
  */
armcix_mutex_hdl_t ARMCIX_Create_mutexes_hdl(int count, ARMCI_Group *pgroup) {
  int rank, nproc;
  armcix_mutex_hdl_t hdl;

  hdl = malloc(sizeof(struct armcix_mutex_hdl_s));
  ARMCII_Assert(hdl != NULL);

  MPI_Comm_dup(pgroup->comm, &hdl->comm);

  MPI_Comm_rank(hdl->comm, &rank);
  MPI_Comm_size(hdl->comm, &nproc);

  hdl->count = count;

  if (count > 0) {
    MPI_Alloc_mem(nproc*count, MPI_INFO_NULL, &hdl->base);
    ARMCII_Assert(hdl->base != NULL);
    ARMCII_Bzero(hdl->base, nproc*count);

  } else {
    hdl->base = NULL;
  }

  /* FIXME: We need multiple windows here: one for each mutex.  Otherwise performance will suffer. */
  MPI_Win_create(hdl->base, nproc*count, 1, MPI_INFO_NULL, hdl->comm, &hdl->window);

  return hdl;
}


/** Destroy a group of ARMCI mutexes.  Collective.
  *
  * @param[in] hdl Handle to the group that should be destroyed.
  * @return        Zero on success, non-zero otherwise.
  */
int ARMCIX_Destroy_mutexes_hdl(armcix_mutex_hdl_t hdl) {
  int ret;

  ret = MPI_Win_free(&hdl->window);

  if (hdl->base != NULL)
    MPI_Free_mem(hdl->base);

  MPI_Comm_free(&hdl->comm);

  free(hdl);

  return ret;
}


/** Lock a mutex.
  * 
  * @param[in] hdl        Mutex group that the mutex belongs to.
  * @param[in] mutex      Desired mutex number [0..count-1]
  * @param[in] world_proc Absolute ID of process where the mutex lives
  */
void ARMCIX_Lock_hdl(armcix_mutex_hdl_t hdl, int mutex, int world_proc) {
  int       rank, nproc, already_locked, i, proc;
  uint8_t *buf;

  ARMCII_Assert(mutex >= 0);

  MPI_Comm_rank(hdl->comm, &rank);
  MPI_Comm_size(hdl->comm, &nproc);

  /* User gives us the absolute ID.  Translate to the rank in the mutex's group. */
  proc = ARMCII_Translate_absolute_to_group(hdl->comm, world_proc);
  ARMCII_Assert(proc >= 0);

  buf = malloc(nproc*sizeof(uint8_t));
  ARMCII_Assert(buf != NULL);

  buf[rank] = 1;

  /* Get all data from the lock_buf, except the byte belonging to
   * me. Set the byte belonging to me to 1. */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, hdl->window);
  
  MPI_Put(&buf[rank], 1, MPI_BYTE, proc, (mutex*nproc) + rank, 1, MPI_BYTE, hdl->window);

  /* Get data to the left of rank */
  if (rank > 0) {
    MPI_Get(buf, rank, MPI_BYTE, proc, mutex*nproc, rank, MPI_BYTE, hdl->window);
  }

  /* Get data to the right of rank */
  if (rank < nproc - 1) {
    MPI_Get(&buf[rank+1], nproc-1-rank, MPI_BYTE, proc, mutex*nproc + rank + 1, nproc-1-rank, MPI_BYTE, hdl->window);
  }
  
  MPI_Win_unlock(proc, hdl->window);

  ARMCII_Assert(buf[rank] == 1);

  for (i = already_locked = 0; i < nproc; i++)
    if (buf[i] && i != rank)
      already_locked = 1;

  /* Wait for notification */
  if (already_locked) {
    MPI_Status status;
    ARMCII_Dbg_print(DEBUG_CAT_MUTEX, "waiting for notification [proc = %d, mutex = %d]\n", proc, mutex);
    MPI_Recv(NULL, 0, MPI_BYTE, MPI_ANY_SOURCE, ARMCI_MUTEX_TAG+mutex, hdl->comm, &status);
  }

  ARMCII_Dbg_print(DEBUG_CAT_MUTEX, "lock acquired [proc = %d, mutex = %d]\n", proc, mutex);
  free(buf);
}


/** Attempt to lock a mutex (implemented as a blocking call).
  * 
  * @param[in] hdl   Mutex group that the mutex belongs to.
  * @param[in] mutex Desired mutex number [0..count-1]
  * @param[in] world_proc Absolute ID of process where the mutex lives
  * @return          0 on success, non-zero on failure
  */
int ARMCIX_Trylock_hdl(armcix_mutex_hdl_t hdl, int mutex, int world_proc) {
  ARMCII_Assert(mutex >= 0 && mutex < hdl->count);

  ARMCIX_Lock_hdl(hdl, mutex, world_proc);
  return 0;
}


/** Unlock a mutex.
  * 
  * @param[in] hdl   Mutex group that the mutex belongs to.
  * @param[in] mutex Desired mutex number [0..count-1]
  * @param[in] world_proc Absolute ID of process where the mutex lives
  */
void ARMCIX_Unlock_hdl(armcix_mutex_hdl_t hdl, int mutex, int world_proc) {
  int      rank, nproc, i, proc;
  uint8_t *buf;

  ARMCII_Assert(mutex >= 0);

  MPI_Comm_rank(hdl->comm, &rank);
  MPI_Comm_size(hdl->comm, &nproc);

  proc = ARMCII_Translate_absolute_to_group(hdl->comm, world_proc);
  ARMCII_Assert(proc >= 0);

  buf = malloc(nproc*sizeof(uint8_t));

  buf[rank] = 0;

  /* Get all data from the lock_buf, except the byte belonging to
   * me. Set the byte belonging to me to 0. */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, hdl->window);
  
  MPI_Put(&buf[rank], 1, MPI_BYTE, proc, (mutex*nproc) + rank, 1, MPI_BYTE, hdl->window);

  /* Get data to the left of rank */
  if (rank > 0) {
    MPI_Get(buf, rank, MPI_BYTE, proc, mutex*nproc, rank, MPI_BYTE, hdl->window);
  }

  /* Get data to the right of rank */
  if (rank < nproc - 1) {
    MPI_Get(&buf[rank+1], nproc-1-rank, MPI_BYTE, proc, mutex*nproc + rank + 1, nproc-1-rank, MPI_BYTE, hdl->window);
  }
  
  MPI_Win_unlock(proc, hdl->window);

  ARMCII_Assert(buf[rank] == 0);

  /* Notify the next waiting process, starting to my right for fairness */
  for (i = 1; i < nproc; i++) {
    int p = (rank + i) % nproc;
    if (buf[p] == 1) {
      ARMCII_Dbg_print(DEBUG_CAT_MUTEX, "notifying %d [proc = %d, mutex = %d]\n", p, proc, mutex);
      MPI_Send(NULL, 0, MPI_BYTE, p, ARMCI_MUTEX_TAG+mutex, hdl->comm);
      break;
    }
  }

  ARMCII_Dbg_print(DEBUG_CAT_MUTEX, "lock released [proc = %d, mutex = %d]\n", proc, mutex);
  free(buf);
}
