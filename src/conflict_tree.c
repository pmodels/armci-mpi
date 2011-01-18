#include <stdio.h>
#include <stdlib.h>

#include "conflict_tree.h"

const ctree_t CTREE_EMPTY = NULL;

/** Insert an address range into the conflict detection tree.
  *
  * @param[inout] root The root of the tree
  * @param[in]    lo   Lower bound of the address range
  * @param[in]    hi   Upper bound of the address range
  * @return            Zero on success, nonzero when a conflict is detected.
  *                    When a conflict exists, the range is not added.
  */
int ctree_insert(ctree_t *root, uint8_t *lo, uint8_t *hi) {
  ctree_t cur;
  ctree_t new_node = (ctree_t) malloc(sizeof(struct ctree_node_s));

  new_node->lo    = lo;
  new_node->hi    = hi;
  new_node->left  = NULL;
  new_node->right = NULL;

  cur = *root;

  if (cur == NULL) {
    *root = new_node;
    return 0;
  }

  for (;;) {

    // Check for conflicts
    if (   (lo >= cur->lo && lo <= cur->hi)
        || (hi >= cur->lo && hi <= cur->hi)
        || (lo <  cur->lo && hi >  cur->hi)) {
      free(new_node);
      return 1;
    }

    // Place in left subtree
    else if (lo < cur->lo) {
      if (cur->left == NULL) {
        cur->left = new_node;
        return 0;
      } else {
        cur = cur->left;
      }
    }
   
    // Place in right subtree
    else /* hi > cur->hi */ {
      if (cur->right == NULL) {
        cur->right = new_node;
        return 0;
      } else {
        cur = cur->right;
      }
    }
  }
}


static void ctree_destroy_rec(ctree_t root) {
  if (root == NULL) return;
  
  ctree_destroy_rec(root->left);
  ctree_destroy_rec(root->right);

  free(root);
}


/** Destroy a conflict tree
  */
void ctree_destroy(ctree_t *root) {
  ctree_destroy_rec(*root);

  *root = NULL;
}


/** Print out the contents of the conflict detection tree.
  */
void ctree_print(ctree_t root) {
  if (root != NULL) {
    ctree_print(root->left);
    printf("[%p, %p]\n", root->lo, root->hi);
    ctree_print(root->right);
  }
}
