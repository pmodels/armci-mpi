/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

/* These mutexes are built using only MPI-2 atomic accumulate.  The only
 * drawback is that they are vulnerable to livelock.  Here's how the lock
 * algorithm works:
 *
 * Let mutex be an integer that is initially 0.  I hold the mutex when after
 * adding my rank to it, it is equal to my rank.
 *
 * function lock(mutex, p):
 *
 *   acc(mutex, p, me)      // mutex = mutex + me
 *
 *   while (get(mutex, p) != me) {
 *     acc(mutex, p, -1*me) // -1*me is the value to be accumulated
 *     sleep(random)        // Try to avoid livelock/do some backoff
 *     acc(mutex, p, me)
 *   }
 *
 * function unlock(mutex, p)
 *   acc(mutex, p, -1*me)
 */

// TODO: Should each mutex be in a different window?

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#include <debug.h>
#include <armci.h>
#include <armcix.h>

#define MAX_TIMEOUT 1000
#define TIMEOUT_MUL 2
#define MIN(A,B) (((A) < (B)) ? (A) : (B))


/** This is the handle for the "default" group of mutexes used by the
  * standard ARMCI mutex API
  */
static armcix_mutex_grp_t armci_mutex_grp = NULL;


/** Create ARMCI mutexes.  Collective.
  *
  * @param[in] count Number of mutexes to create on the calling process
  */
int ARMCI_Create_mutexes(int count) {
  if (armci_mutex_grp != NULL)
    ARMCII_Error("attempted to create ARMCI mutexes multiple times");

  armci_mutex_grp = ARMCIX_Create_mutexes_grp(count);

  if (armci_mutex_grp != NULL)
    return 0;
  else
    return 1;
}


/** Destroy/free ARMCI mutexes.  Collective.
  */
int ARMCI_Destroy_mutexes(void) {
  int err;

  if (armci_mutex_grp == NULL)
    ARMCII_Error("attempted to free unallocated ARMCI mutexes");
  
  err = ARMCIX_Destroy_mutexes_grp(armci_mutex_grp);
  armci_mutex_grp = NULL;

  return err;
}


/** Lock a mutex.
  *
  * @param[in] mutex Number of the mutex to lock
  * @param[in] proc  Target process for the lock operation
  */
void ARMCI_Lock(int mutex, int proc) {
  if (armci_mutex_grp == NULL)
    ARMCII_Error("attempted to lock on unallocated ARMCI mutexes");
  
  ARMCIX_Lock_grp(armci_mutex_grp, mutex, proc);
}

/** Unlock a mutex.
  *
  * @param[in] mutex Number of the mutex to unlock
  * @param[in] proc  Target process for the unlock operation
  */
void ARMCI_Unlock(int mutex, int proc) {
  if (armci_mutex_grp == NULL)
    ARMCII_Error("attempted to unlock on unallocated ARMCI mutexes");
  
  ARMCIX_Unlock_grp(armci_mutex_grp, mutex, proc);
}


/******************** Mutex Groups: ********************/


/** Create a mutex group.  Collective.
  *
  * @param[in] count Number of mutexes to create on the calling process
  * @return          Handle to the mutex group
  */
armcix_mutex_grp_t ARMCIX_Create_mutexes_grp(int count) {
  int         ierr, i;
  armcix_mutex_grp_t grp;

  grp = malloc(sizeof(struct mutex_grp_s));
  ARMCII_Assert(grp != NULL);

  ierr = MPI_Alloc_mem(count*sizeof(long), MPI_INFO_NULL, &grp->base);
  ARMCII_Assert(ierr == MPI_SUCCESS);

  grp->count = count;

  // Initialize mutexes to 0
  for (i = 0; i < count; i++)
    grp->base[i] = 0;

  // MPI_Barrier?

  ierr = MPI_Win_create(grp->base, count*sizeof(long), sizeof(long) /* displacement size */,
                        MPI_INFO_NULL, ARMCI_GROUP_WORLD.comm, &grp->window);
  ARMCII_Assert(ierr == MPI_SUCCESS);

  return grp;
}


/** Destroy/free a mutex group.  Collective.
  * 
  * @param[in] grp Group to destroy
  */
int ARMCIX_Destroy_mutexes_grp(armcix_mutex_grp_t grp) {
  MPI_Win_free(&grp->window);
  MPI_Free_mem(grp->base);
  free(grp);
  return 0;
}


/** Lock a mutex.
  * 
  * @param[in] grp   Mutex group that the mutex belongs to.
  * @param[in] mutex Desired mutex number [0..count-1]
  * @param[in] proc  Process where the mutex lives
  */
void ARMCIX_Lock_grp(armcix_mutex_grp_t grp, int mutex, int proc) {
  MPI_Group mpi_grp;
  int       rank, nproc;
  long      lock_val, unlock_val, lock_out;
  int       timeout = 1;

  MPI_Win_get_group(grp->window, &mpi_grp);
  MPI_Group_rank(mpi_grp, &rank);
  MPI_Group_size(mpi_grp, &nproc);
  MPI_Group_free(&mpi_grp);

  lock_val   = rank+1;    // Map into range 1..nproc
  unlock_val = -1 * (rank+1);

  /* mutex <- mutex + rank */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
  MPI_Accumulate(&lock_val, 1, MPI_LONG, proc, mutex, 1, MPI_LONG, MPI_SUM, grp->window);
  MPI_Win_unlock(proc, grp->window);

  for (;;) {
    /* read mutex value */
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
    MPI_Get(&lock_out, 1, MPI_LONG, proc, mutex, 1, MPI_LONG, grp->window);
    MPI_Win_unlock(proc, grp->window);

    ARMCII_Assert(lock_out > 0);
    ARMCII_Assert(lock_out <= nproc*(nproc+1)/2); // Must be < sum of all ranks

    /* We are holding the mutex */
    if (lock_out == rank+1)
      break;

    /* mutex <- mutex - rank */
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
    MPI_Accumulate(&unlock_val, 1, MPI_LONG, proc, mutex, 1, MPI_LONG, MPI_SUM, grp->window);
    MPI_Win_unlock(proc, grp->window);

    /* Exponential backoff */
    usleep(timeout + rand()%timeout);
    timeout = MIN(timeout*TIMEOUT_MUL, MAX_TIMEOUT);
    if (rand() % nproc == 0) // Chance to reset timeout
      timeout = 1;

    /* mutex <- mutex + rank */
    MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
    MPI_Accumulate(&lock_val, 1, MPI_LONG, proc, mutex, 1, MPI_LONG, MPI_SUM, grp->window);
    MPI_Win_unlock(proc, grp->window);
  }
}


/** Attempt to lock a mutex (non-blocking).
  * 
  * @param[in] grp   Mutex group that the mutex belongs to.
  * @param[in] mutex Desired mutex number [0..count-1]
  * @param[in] proc  Process where the mutex lives
  * @return          0 on success, non-zero on failure
  */
int ARMCIX_Trylock_grp(armcix_mutex_grp_t grp, int mutex, int proc) {
  MPI_Group mpi_grp;
  int       rank, nproc;
  long      lock_val, unlock_val, lock_out;

  MPI_Win_get_group(grp->window, &mpi_grp);
  MPI_Group_rank(mpi_grp, &rank);
  MPI_Group_size(mpi_grp, &nproc);
  MPI_Group_free(&mpi_grp);

  lock_val   = rank+1;
  unlock_val = -1 * (rank+1);

  /* mutex <- mutex + rank */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
  MPI_Accumulate(&lock_val, 1, MPI_LONG, proc, mutex, 1, MPI_LONG, MPI_SUM, grp->window);
  MPI_Win_unlock(proc, grp->window);

  /* read mutex value */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
  MPI_Get(&lock_out, 1, MPI_LONG, proc, mutex, 1, MPI_LONG, grp->window);
  MPI_Win_unlock(proc, grp->window);

  ARMCII_Assert(lock_out > 0);
  ARMCII_Assert(lock_out <= nproc*(nproc+1)/2); // Must be < sum of all ranks

  /* We are holding the mutex */
  if (lock_out == rank+1)
    return 0;

  /* mutex <- mutex - rank */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
  MPI_Accumulate(&unlock_val, 1, MPI_LONG, proc, mutex, 1, MPI_LONG, MPI_SUM, grp->window);
  MPI_Win_unlock(proc, grp->window);

  return 1;
}


/** Unlock a mutex.
  * 
  * @param[in] grp   Mutex group that the mutex belongs to.
  * @param[in] mutex Desired mutex number [0..count-1]
  * @param[in] proc  Process where the mutex lives
  */
void ARMCIX_Unlock_grp(armcix_mutex_grp_t grp, int mutex, int proc) {
  MPI_Group mpi_grp;
  int       rank;
  long      unlock_val;

  MPI_Win_get_group(grp->window, &mpi_grp);
  MPI_Group_rank(mpi_grp, &rank);
  MPI_Group_free(&mpi_grp);

  unlock_val = -1 * (rank+1);

  /* mutex <- mutex - rank */
  MPI_Win_lock(MPI_LOCK_EXCLUSIVE, proc, 0, grp->window);
  MPI_Accumulate(&unlock_val, 1, MPI_LONG, proc, mutex, 1, MPI_LONG, MPI_SUM, grp->window);
  MPI_Win_unlock(proc, grp->window);
}

