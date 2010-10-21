/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <mem_region.h>
#include <debug.h>


/** Perform atomic read-modify-write on the given integer or long location and
  * return the location's original value.
  *
  * @param[in]  op    Operation to be performed:
  *                     ARMCI_FETCH_AND_ADD (int)
  *                     ARMCI_FETCH_AND_ADD_LONG
  *                     ARMCI_SWAP (int)
  *                     ARMCI_SWAP_LONG
  * @param[out] ploc  Location to store the original value.
  * @param[in]  prem  Location on which to perform atomic operation.
  * @param[in]  value Value to add to remote location (ignored for swap).
  * @param[in]  proc  Process rank for the target buffer.
  */
int ARMCI_Rmw(int op, void *ploc, void *prem, int value, int proc) {
  int           is_long;
  mem_region_t *mreg;

  mreg = mem_region_lookup(prem, proc);
  assert(mreg != NULL);

  if (op == ARMCI_SWAP_LONG || op == ARMCI_FETCH_AND_ADD_LONG)
    is_long = 1;
  else
    is_long = 0;

  if (op == ARMCI_SWAP || op == ARMCI_SWAP_LONG) {
    long swap_val;
    ARMCIX_Lock_grp(mreg->rmw_mutex, 0, proc);
    ARMCI_Get(prem, &swap_val, is_long ? sizeof(long) : sizeof(int), proc);
    ARMCI_Put(ploc, prem,      is_long ? sizeof(long) : sizeof(int), proc);
    ARMCIX_Unlock_grp(mreg->rmw_mutex, 0, proc);

    if (is_long)
      *(long*) ploc = swap_val;
    else {

      /* This is different than just casting to int.  Long is larger than or
       * the same size as an int, so if we cast long to int we might get just
       * the higer address bytes, but what we want actually want are the lower
       * address bytes since we are treating this long buffer as an int buffer.
       */

      *(int*) ploc = * ((int*) (&swap_val));
    }
  }

  else if (op == ARMCI_FETCH_AND_ADD || op == ARMCI_FETCH_AND_ADD_LONG) {
    long fetch_val, new_val;
    
    ARMCIX_Lock_grp(mreg->rmw_mutex, 0, proc);
    ARMCI_Get(prem, &fetch_val, is_long ? sizeof(long) : sizeof(int), proc);
    
    if (is_long)
      new_val = fetch_val + value;
    else {
      /* This is different than just casting to int.  See comment above. */

      *((int*) (&new_val)) = *((int*)(&fetch_val)) + value;
    }

    ARMCI_Put(&new_val, prem, is_long ? sizeof(long) : sizeof(int), proc);
    ARMCIX_Unlock_grp(mreg->rmw_mutex, 0, proc);

    if (is_long)
      *(long*) ploc = fetch_val;
    else {
      /* This is different than just casting to int.  See comment above. */

      *(int*) ploc = * ((int*) (&fetch_val));
    }
  }

  else {
    ARMCI_Error("ARMCI_Rmw: Unsopported operation", 1);
  }

  return 0;
}
