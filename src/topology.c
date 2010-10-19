/*
 * Copyright (C) 2010. See COPYRIGHT in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>

#include <armci.h>
#include <armci_internals.h>

/** WARNING: Domains are not implemented.  These dummy wrappers assume that all
  * domains are of size 1. */

int armci_domain_nprocs(armci_domain_t domain, int id) {
  return 1;
}

int armci_domain_id(armci_domain_t domain, int glob_proc_id) {
  return glob_proc_id;
}

int armci_domain_glob_proc_id(armci_domain_t domain, int id, int loc_proc_id) {
  return id;
}

int armci_domain_my_id(armci_domain_t domain) {
  return 0;
}

int armci_domain_count(armci_domain_t domain) {
  return ARMCI_GROUP_WORLD.size;
}

int armci_domain_same_id(armci_domain_t domain, int proc) {
  return proc == ARMCI_GROUP_WORLD.rank;
}
