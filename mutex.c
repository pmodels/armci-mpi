#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <mpi.h>

#include "debug.h"
#include "armci.h"

#define MAX_TIMEOUT 1000
#define TIMEOUT_MUL 2
#define MIN(A,B) (((A) < (B)) ? (A) : (B))


/** This is the handle for the "default" group of mutexes used by the
  * standard ARMCI mutex API
  */
static mutex_grp_t armci_mutex_grp = NULL;


/** Create ARMCI mutexes.  Collective.
  *
  * @param[in] count Number of mutexes to create on the calling process
  */
int ARMCI_Create_mutexes(int count) {
  if (armci_mutex_grp != NULL)
    ARMCI_Error("attempted to create ARMCI mutexes multiple times", 10);

  armci_mutex_grp = ARMCI_Create_mutexes_grp(count);

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
    ARMCI_Error("attempted to free unallocated ARMCI mutexes", 10);
  
  err = ARMCI_Destroy_mutexes_grp(armci_mutex_grp);
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
    ARMCI_Error("attempted to lock on unallocated ARMCI mutexes", 10);
  
  ARMCI_Lock_grp(armci_mutex_grp, mutex, proc);
}

/** Unlock a mutex.
  *
  * @param[in] mutex Number of the mutex to unlock
  * @param[in] proc  Target process for the unlock operation
  */
void ARMCI_Unlock(int mutex, int proc) {
  if (armci_mutex_grp == NULL)
    ARMCI_Error("attempted to unlock on unallocated ARMCI mutexes", 10);
  
  ARMCI_Unlock_grp(armci_mutex_grp, mutex, proc);
}
