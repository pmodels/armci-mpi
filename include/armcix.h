/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_ARMCIX_H
#define HAVE_ARMCIX_H

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

armcix_mutex_grp_t ARMCIX_Create_mutexes_grp(int count);
int  ARMCIX_Destroy_mutexes_grp(armcix_mutex_grp_t grp);
void ARMCIX_Lock_grp(armcix_mutex_grp_t grp, int mutex, int proc);
int  ARMCIX_Trylock_grp(armcix_mutex_grp_t grp, int mutex, int proc);
void ARMCIX_Unlock_grp(armcix_mutex_grp_t grp, int mutex, int proc);

#endif /* HAVE_ARMCIX_H */
