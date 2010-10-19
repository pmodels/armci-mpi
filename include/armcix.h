#ifndef _ARMCIX_H_
#define _ARMCIX_H_

int ARMCIX_Group_split(ARMCI_Group *parent, int color, int key, ARMCI_Group *new_group);

/** Mutex groups: These improve on basic ARMCI mutexes by allowing you to
  * create multiple mutex groups.  This is needed to allow libraries access to
  * mutexes.
  */

struct mutex_grp_s {
  int      count;
  MPI_Comm comm;
  MPI_Win  window;
  long    *base;
};

typedef struct mutex_grp_s * mutex_grp_t;

mutex_grp_t ARMCI_Create_mutexes_grp(int count);
int         ARMCI_Destroy_mutexes_grp(mutex_grp_t grp);
void        ARMCI_Lock_grp(mutex_grp_t grp, int mutex, int proc);
int         ARMCI_Trylock_grp(mutex_grp_t grp, int mutex, int proc);
void        ARMCI_Unlock_grp(mutex_grp_t grp, int mutex, int proc);

#endif
