#ifndef _MEM_REGION_H_
#define _MEM_REGION_H_

#include <mpi.h>

typedef struct {
  void *base;
  int   size;
} mem_region_slice_t;

// TODO: how to handle groups here?
typedef struct mem_region_s {
  MPI_Win             window;
  int                 nslices;
  struct mem_region_s *prev;
  struct mem_region_s *next;
  mem_region_slice_t *slices;
} mem_region_t;

extern mem_region_t *mreg_list;

mem_region_t *mem_region_create(int local_size);
void          mem_region_destroy(mem_region_t *mreg);
mem_region_t *mem_region_lookup(void *ptr, int proc);

#endif /* _MEM_REGION_H_ */
