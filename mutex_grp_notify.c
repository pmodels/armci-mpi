#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#include "armci.h"
#include "debug.h"

// FIXME: Need to use a mutex communicator / MPI_Comm_dup here

#define ARMCI_MUTEX_TAG 100

/** Create a group of ARMCI mutexes.
  *
  * @param[in] count Number of mutexes on the local process.
  * @return          Handle to the mutex group.
  */
mutex_grp_t ARMCI_Create_mutexes_grp(int count) {
  int rank, nproc;
  mutex_grp_t grp;

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  grp = malloc(sizeof(struct mutex_grp_s));
  assert(grp != NULL);

  grp->count = count;

  if (count > 0) {
    grp->base = malloc(nproc*count);
    memset(grp->base, 0, nproc*count);

  } else {
    grp->base = NULL;
  }

  return MPI_Win_create(grp->base, nproc*count, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &grp->window);
}


/** Destroy a group of ARMCI mutexes.
  *
  * @param[in] grp Handle to the group that should be destroyed.
  * @return        Zero on success, non-zero otherwise.
  */
int ARMCI_Destroy_mutexes_grp(mutex_grp_t grp) {
  int ret;

  ret = MPI_Win_free(&grp->window);

  if (grp->base != NULL)
    free(grp->base);

  free(grp);

  return ret;
}


int ARMCI_Lock_grp(mutex_grp_t grp, int mutex, int proc) {
  int       rank, nproc, already_locked;
  u_int8_t *buf;

  MPI_Comm_rank(mutex->comm, &rank);
  MPI_Comm_size(mutex->comm, &nproc);

  buf = malloc(nproc*sizeof(u_int8_t));

  buf[rank] = 1;

  /* Get all data from the lock_buf, except the byte belonging to
   * me. Set the byte belonging to me to 1. */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, grp->window);
  
  MPI_Put(&buf[rank], 1, MPI_BYTE, proc, (mutex*nproc) + rank, 1, MPI_BYTE, grp->window);

  /* Get data to the left of rank */
  if (rank > 0) {
    MPI_Get(buf, rank, MPI_BYTE, proc, mutex*nproc, rank, MPI_BYTE, grp->window);
  }

  /* Get data to the right of rank */
  if (rank < nproc - 1) {
    MPI_Get(&buf[rank+1], nproc-1-rank, MPI_BYTE, proc, mutex*nproc + rank + 1, nproc-1-rank, MPI_BYTE, grp->window);
  }
  
  MPI_Win_unlock(0, grp->window);

  for (i = already_locked = 0; i < nproc; i++)
    if (local_lock_buf[i] && i != rank)
      already_locked = 1;

  /* Wait for notification */
  if (already_locked) {
    MPI_Status status;
    MPI_Recv(NULL, 0, MPI_BYTE, MPI_SOURCE_ANY, ARMCI_MUTEX_TAG+mutex, &status, MPI_COMM_WORLD);
  }

  free(buf);
}


int ARMCI_Trylock_grp(mutex_grp_t grp, int mutex, int proc) {
  return ARMCI_Lock_grp(gtp, mutex, proc);
}


void ARMCI_Unlock_grp(mutex_grp_t grp, int mutex, int proc) {
  int       rank, nproc;
  u_int8_t *buf;

  MPI_Comm_rank(mutex->comm, &rank);
  MPI_Comm_size(mutex->comm, &nproc);

  buf = malloc(nproc*sizeof(u_int8_t));

  buf[rank] = 0;

  /* Get all data from the lock_buf, except the byte belonging to
   * me. Set the byte belonging to me to 0. */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, 0, 0, grp->window);
  
  MPI_Put(&buf[rank], 1, MPI_BYTE, proc, (mutex*nproc) + rank, 1, MPI_BYTE, grp->window);

  /* Get data to the left of rank */
  if (rank > 0) {
    MPI_Get(buf, rank, MPI_BYTE, proc, mutex*nproc, rank, MPI_BYTE, grp->window);
  }

  /* Get data to the right of rank */
  if (rank < nproc - 1) {
    MPI_Get(&buf[rank+1], nproc-1-rank, MPI_BYTE, proc, mutex*nproc + rank + 1, nproc-1-rank, MPI_BYTE, grp->window);
  }
  
  MPI_Win_unlock(0, grp->window);

  /* Notify the next waiting process */
  for (i = 0; i < nproc; i++) {
    if (buf[i] == 1) {
      MPI_Send(NULL, 0, MPI_BYTE, i, ARMCI_MUTEX_TAG+mutex, MPI_COMM_WORLD);
      break;
    }
  }

  free(buf);
}
