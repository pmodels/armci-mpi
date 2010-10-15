#ifndef _ARMCIX_H_
#define _ARMCIX_H_

typedef struct {
  ARMCI_Group *groups;  // Array of group handles
  int ngroups;          // Length of the array
  int mygroup;          // Index of my group in the array
} ARMCIX_Group_partition_t;

ARMCIX_Group_partition_t *ARMCIX_Group_partition(int grp_size, ARMCI_Group *parent_grp);
void         ARMCIX_Group_partition_free(ARMCIX_Group_partition_t *g);
ARMCI_Group *ARMCIX_Group_partition_mygroup(ARMCIX_Group_partition_t *g);

int ARMCIX_Group_split(ARMCI_Group *parent, int color, int key, ARMCI_Group *new_group);

#endif
