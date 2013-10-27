/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef _ARMCIX_H_
#define _ARMCIX_H_

#include <armci.h>
#include <armciconf.h>

#if   HAVE_STDINT_H
#  include <stdint.h>
#elif HAVE_INTTYPES_H
#  include <inttypes.h>
#endif

/** Processor group extensions.
  */

int ARMCIX_Group_split(ARMCI_Group *parent, int color, int key, ARMCI_Group *new_group);
int ARMCIX_Group_dup(ARMCI_Group *parent, ARMCI_Group *new_group);

/** Mutex handles: These improve on basic ARMCI mutexes by allowing you to
  * create multiple batches of mutexes.  This is needed to allow libraries access to
  * mutexes.
  */

struct armcix_mutex_hdl_s {
  int         my_count;
  int         max_count;
  ARMCI_Group grp;
  MPI_Win    *windows;
  uint8_t   **bases;
};

typedef struct armcix_mutex_hdl_s * armcix_mutex_hdl_t;

armcix_mutex_hdl_t ARMCIX_Create_mutexes_hdl(int count, ARMCI_Group *pgroup);
int  ARMCIX_Destroy_mutexes_hdl(armcix_mutex_hdl_t hdl);
void ARMCIX_Lock_hdl(armcix_mutex_hdl_t hdl, int mutex, int proc);
int  ARMCIX_Trylock_hdl(armcix_mutex_hdl_t hdl, int mutex, int proc);
void ARMCIX_Unlock_hdl(armcix_mutex_hdl_t hdl, int mutex, int proc);

#endif /* _ARMCIX_H_ */
