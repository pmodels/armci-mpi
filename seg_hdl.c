#include <stdio.h>
#include <stdlib.h>
#include "debug.h"
#include <mpi.h>

#include "armci.h"
#include "seg_hdl.h"

int       seg_hdls_init = 0;
seg_hdl_t seg_hdls[MAX_SEGS];

/** Allocate a segment handle
  *
  * @return Pointer to the segment handle.
  */
seg_hdl_t *seg_hdl_alloc() {
  int i, seg;

  // Initialize the array of segment handles the first time through
  if (!seg_hdls_init) {
    seg_hdls_init = 1;

    for (i = 0; i < MAX_SEGS; i++)
      seg_hdls[i].active = 0;
  }

  // Find the next free segment
  for (i = 0, seg = -1; i < MAX_SEGS; i++) {
    if (!seg_hdls[i].active) {
      seg = i;
      seg_hdls[seg].active = 1;
      break;
    }
  }

  if (seg < 0)
    ARMCI_Error("ran out of segment handles, increase MAX_SEGS", 10);

  return &seg_hdls[seg];
}


/** Free a segment handle
  *
  * @param[in] ptr Pointer within range of the segment (e.g. base pointer).
  */
void seg_hdl_free(void *ptr) {
  int idx;

  idx = seg_hdl_lookup_idx(ptr);
  assert(idx >= 0);

  seg_hdls[idx].active = 0;
}


/** Lookup the index of a segment handle (not intended for public use, call
  * seg_hdl_lookup() instead).
  *
  * @param[in] ptr Pointer within range of the segment (e.g. base pointer).
  * @return        Index of the segment handle.
  */
int seg_hdl_lookup_idx(void *ptr) {
  int i;

  // Scan the list of segments
  for (i = 0; i < MAX_SEGS; i++) {
    if (seg_hdls[i].active && 
        (   ((u_int8_t*) ptr >= seg_hdls[i].base && (u_int8_t*) ptr < seg_hdls[i].base + seg_hdls[i].nbytes)
         || ((u_int8_t*) ptr == seg_hdls[i].base && seg_hdls[i].nbytes == 0) // This is a hack to deal with nodes that 
                                                                             // allocate 0 bytes.  Should be fixed elsewhere.
        ))

      return i;
  }

  // Didn't find it
  return -1;
}


/** Lookup a segment handle.
  *
  * @param[in] ptr Pointer within range of the segment (e.g. base pointer).
  * @return        Pointer to the segment handle.
  */
seg_hdl_t *seg_hdl_lookup(void *ptr) {
  int idx;

  idx = seg_hdl_lookup_idx(ptr);

  dprint(DEBUG_CAT_SEG_HDL, "lookup %p => %d idx\n", ptr, idx);

  if (idx < 0)
    return NULL;
  else
    return &seg_hdls[idx];
}


/** Lookup the window object where an address lives.
  *
  * @param[in] base A "shared" address.
  * @return         The MPI window where that address lives
  */
MPI_Win *seg_hdl_win_lookup(void *base) {
  seg_hdl_t *seg = seg_hdl_lookup(base);
  assert(seg != NULL);
  return &seg->window;
}


/** Lookup the base address of a shared segment.
  *
  * @param[in] base A "shared" address
  * @return         The base address of the corresponding MPI window
  */
void *seg_hdl_base_lookup(void *base) {
  seg_hdl_t *seg = seg_hdl_lookup(base);
  assert(seg != NULL);
  return seg->base;
}
