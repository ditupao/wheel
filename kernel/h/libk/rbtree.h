#ifndef LIBK_RBTREE_H
#define LIBK_RBTREE_H

#include <base.h>

typedef struct rbnode rbnode_t;
typedef struct rbtree rbtree_t;

struct rbnode {
    usize      parent_color;
    rbnode_t * left;
    rbnode_t * right;
} __ALIGNED(sizeof(usize));

struct rbtree {
    rbnode_t * root;
};

#define RBNODE_INIT ((rbnode_t) { 0, 0, 0 })
#define RBTREE_INIT ((rbtree_t) { 0 })

extern void rb_link_node(rbnode_t * node, rbnode_t * parent, rbnode_t ** rb_link);
extern void rb_insert_fixup(rbtree_t * tree, rbnode_t * node);
extern void rb_erase(rbtree_t * tree, rbnode_t * node);
extern void rb_replace(rbtree_t * tree, rbnode_t * victim, rbnode_t * node);

extern rbnode_t * rb_first(rbtree_t * tree);
extern rbnode_t * rb_last (rbtree_t * tree);
extern rbnode_t * rb_next (rbnode_t * node);
extern rbnode_t * rb_prev (rbnode_t * node);

#endif // LIBK_RBTREE_H
