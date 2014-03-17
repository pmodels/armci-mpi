/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <mpi.h>

int main(int argc, char ** argv) {
  int      rank, nproc;
  void    *ptr;
  MPI_Win  win;
  int      wsize = 1024*1024;
  int attribute_val, attr_flag;

  MPI_Init(&argc, &argv);

  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);

  if (rank == 0) printf("Starting MPI window attribute test with %d processes\n", nproc);

  /* WIN_CREATE */

  MPI_Alloc_mem(wsize, MPI_INFO_NULL, &ptr);
  MPI_Win_create(ptr, wsize, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &win);

  /* this function will always return flag=false in MPI-2 */
  MPI_Win_get_attr(win, MPI_WIN_MODEL, (void *)&attribute_val, &attr_flag);
  if (!attr_flag && rank==0)
      printf("MPI_WIN_MODEL flag missing! \n");
  if (attr_flag && attribute_val!=MPI_WIN_UNIFIED && rank==0)
      printf("MPI_WIN_MODEL = MPI_WIN_SEPARATE \n" );
 
  MPI_Win_free(&win);
  MPI_Free_mem(ptr);

  /* WIN_ALLOCATE */

  MPI_Win_allocate(wsize, 1, MPI_INFO_NULL, MPI_COMM_WORLD, &ptr, &win);

  /* this function will always return flag=false in MPI-2 */
  MPI_Win_get_attr(win, MPI_WIN_MODEL, (void *)&attribute_val, &attr_flag);
  if (!attr_flag && rank==0)
      printf("MPI_WIN_MODEL flag missing! \n");
  if (attr_flag && attribute_val!=MPI_WIN_UNIFIED && rank==0)
      printf("MPI_WIN_MODEL = MPI_WIN_SEPARATE \n" );
 
  MPI_Win_free(&win);

  /* WIN_ALLOCATE */
  MPI_Finalize();

  return 0;
}
