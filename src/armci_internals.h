/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_ARMCI_INTERNALS_H
#define HAVE_ARMCI_INTERNALS_H

#include <armci.h>

extern ARMCI_Group ARMCI_GROUP_WORLD;
extern ARMCI_Group ARMCI_GROUP_DEFAULT;


void ARMCII_Error(const char *file, const int line, const char *func, const char *msg, int code);

int  ARMCII_Translate_absolute_to_group(MPI_Comm group_comm, int world_rank);

#endif /* HAVE_ARMCI_INTERNALS_H */
