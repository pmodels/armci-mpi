/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_ARMCIX_H
#define HAVE_ARMCIX_H

#include <armci.h>

/** Access mode extensions: set the access mode for an ARMCI allocation,
  * enabling runtime layer optimizations.
  */

enum armcix_access_mode_e {
  ARMCIX_MODE_ALL      = 1,  /* All access types permitted   */
  ARMCIX_MODE_RMA_ONLY = 2,  /* Remote read/write/acc only   */
  ARMCIX_MODE_READ_ONLY= 3,  /* Local/remote read only       */
  ARMCIX_MODE_ACC_ONLY = 4   /* Local/remote accumulate only */
};

enum armcix_consistency_mode_e {
  ARMCIX_MODE_ORDERED  = 1024, /* Operations are location consistent  */
  ARMCIX_MODE_UNORDERD = 2048  /* Operations are ordered, consistency 
                                * is achieved through sync and flush  */
};

int ARMCIX_Mode_set(int mode, void *ptr, ARMCI_Group *group);
int ARMCIX_Mode_get(void *ptr);

/** Processor group extensions.
  */

int ARMCIX_Group_split(ARMCI_Group *parent, int color, int key, ARMCI_Group *new_group);

/** Mutex groups: These improve on basic ARMCI mutexes by allowing you to
  * create multiple mutex groups.  This is needed to allow libraries access to
  * mutexes.
  */

struct armcix_mutex_grp_s {
  int      count;
  MPI_Comm comm;
  MPI_Win  window;
  long    *base;
};

typedef struct armcix_mutex_grp_s * armcix_mutex_grp_t;

armcix_mutex_grp_t ARMCIX_Create_mutexes_grp(int count, ARMCI_Group *pgroup);
int  ARMCIX_Destroy_mutexes_grp(armcix_mutex_grp_t grp);
void ARMCIX_Lock_grp(armcix_mutex_grp_t grp, int mutex, int proc);
int  ARMCIX_Trylock_grp(armcix_mutex_grp_t grp, int mutex, int proc);
void ARMCIX_Unlock_grp(armcix_mutex_grp_t grp, int mutex, int proc);

#endif /* HAVE_ARMCIX_H */
