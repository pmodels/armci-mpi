/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#ifndef HAVE_ARMCI_INTERNALS_H
#define HAVE_ARMCI_INTERNALS_H

#include <armci.h>

extern ARMCI_Group ARMCI_GROUP_WORLD;

void ARMCI_Error_internal(const char *file, const int line, const char *func, char *msg, int code);

#endif /* HAVE_ARMCI_INTERNALS_H */
