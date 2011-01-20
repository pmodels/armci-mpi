/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#include <debug.h>
#include <armci.h>
#include <armci_internals.h>
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
    ARMCII_Error("attempted to create ARMCI mutexes multiple times", 100);

  armci_mutex_grp = ARMCIX_Create_mutexes_grp(count, &ARMCI_GROUP_WORLD);

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
    ARMCII_Error("attempted to free unallocated ARMCI mutexes", 100);
  
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
    ARMCII_Error("attempted to lock on unallocated ARMCI mutexes", 100);
  
  ARMCIX_Lock_grp(armci_mutex_grp, mutex, proc);
}

/** Unlock a mutex.
  *
  * @param[in] mutex Number of the mutex to unlock
  * @param[in] proc  Target process for the unlock operation
  */
void ARMCI_Unlock(int mutex, int proc) {
  if (armci_mutex_grp == NULL)
    ARMCII_Error("attempted to unlock on unallocated ARMCI mutexes", 100);
  
  ARMCIX_Unlock_grp(armci_mutex_grp, mutex, proc);
}
