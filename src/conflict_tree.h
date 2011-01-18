#ifndef _CONFLICT_TREE_H
#define _CONFLICT_TREE_H

#include <stdint.h>

struct ctree_node_s {
  uint8_t *lo;
  uint8_t *hi;

  struct ctree_node_s *left;
  struct ctree_node_s *right;
};

typedef struct ctree_node_s * ctree_t;

extern const ctree_t CTREE_EMPTY;

int  ctree_insert(ctree_t *root, uint8_t *lo, uint8_t *hi);
void ctree_destroy(ctree_t *root);
void ctree_print(ctree_t root);


#endif /* _CONFLICT_TREE_H */
