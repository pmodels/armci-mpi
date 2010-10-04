#ifndef _SEG_HDL_H_
#define _SEG_HDL_H_

#include <mpi.h>

#define MAX_SEGS 100

typedef struct {
  MPI_Win   window;
  u_int8_t *base;
  int       nbytes;
  int       active;
} seg_hdl_t;

extern int       seg_hdls_init;
extern seg_hdl_t seg_hdls[MAX_SEGS];

seg_hdl_t *seg_hdl_alloc();
void       seg_hdl_free(void *ptr);
int        seg_hdl_lookup_idx(void *ptr);
seg_hdl_t *seg_hdl_lookup(void *ptr);

MPI_Win   *seg_hdl_win_lookup(void *ptr);
void      *seg_hdl_base_lookup(void *ptr);

#endif /* _SEG_HDL_H_ */
