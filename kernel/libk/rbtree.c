#include <wheel.h>

#define RB_RED      0
#define RB_BLACK    1

#define RB_COLOR(node)  ((usize)      ((node)->parent_color &  3))
#define RB_PARENT(node) ((rbnode_t *) ((node)->parent_color & ~3))

static void rb_set_parent(rbnode_t * node, rbnode_t * parent) {
    node->parent_color = (node->parent_color & 3) | ((usize) parent & ~3);
}

static void rb_set_color(rbnode_t * node, usize color) {
    node->parent_color = (color & 3) | (node->parent_color & ~3);
}

/*
     X                                Y
    / \     --- left rotate --->     / \
   a   Y                            X   c
      / \   <-- right rotate ---   / \
     b   c                        a   b
*/

static void rb_rotate_left(rbnode_t * node, rbtree_t * tree) {
    rbnode_t * right  = node->right;            // Y
    rbnode_t * parent = RB_PARENT(node);        // P

    if (NULL != (node->right = right->left)) {  // X->right  = b
        rb_set_parent(right->left, node);       // b->parent = X
    }
    right->left = node;                         // Y->left   = X
    rb_set_parent(right, parent);               // Y->parent = P

    if      (NULL == parent)       { tree->root    = right; }
    else if (node == parent->left) { parent->left  = right; }
    else                           { parent->right = right; }
    rb_set_parent(node, right);
}

static void rb_rotate_right(rbnode_t * node, rbtree_t * tree) {
    rbnode_t * left   = node->left;             // X
    rbnode_t * parent = RB_PARENT(node);        // P

    if (NULL != (node->left = left->right)) {   // Y->left   = b
        rb_set_parent(left->right, node);       // b->parent = Y
    }
    left->right = node;                         // X->right  = Y
    rb_set_parent(left, parent);                // X->parent = P

    if      (NULL == parent)       { tree->root    = left; }
    else if (node == parent->left) { parent->left  = left; }
    else                           { parent->right = left; }
    rb_set_parent(node, left);
}

// insert new node into the red-black tree
void rb_link_node(rbnode_t * node, rbnode_t * parent, rbnode_t ** link) {
    node->parent_color = (usize) parent;
    node->left = node->right = NULL;
    *link = node;
}

// keep red-black properties after inserting a new node
void rb_insert_fixup(rbnode_t * node, rbtree_t * tree) {
    rbnode_t * parent, * gparent, * uncle;
    while ((parent = RB_PARENT(node)) && (RB_RED == RB_COLOR(parent))) {
        gparent = RB_PARENT(parent);
        if (parent == gparent->left) {
            uncle = gparent->right;
            if ((NULL != uncle) && (RB_RED == RB_COLOR(uncle))) {
                rb_set_color(uncle,   RB_BLACK);
                rb_set_color(parent,  RB_BLACK);
                rb_set_color(gparent, RB_RED);
                node = gparent;
                continue;
            }
            if (node == parent->right) {
                rb_rotate_left(parent, tree);
                rbnode_t * tmp = node;
                node = parent;
                parent = tmp;
            }
            rb_set_color(parent,  RB_BLACK);
            rb_set_color(gparent, RB_RED);
            rb_rotate_right(gparent, tree);
        } else {
            uncle = gparent->left;
            if ((NULL != uncle) && (RB_RED == RB_COLOR(uncle))) {
                rb_set_color(uncle,   RB_BLACK);
                rb_set_color(parent,  RB_BLACK);
                rb_set_color(gparent, RB_RED);
                node = gparent;
                continue;
            }
            if (node == parent->left) {
                rb_rotate_right(parent, tree);
                rbnode_t * tmp = node;
                node = parent;
                parent = tmp;
            }
            rb_set_color(parent,  RB_BLACK);
            rb_set_color(gparent, RB_RED);
            rb_rotate_left(gparent, tree);
        }
    }
    rb_set_color(tree->root, RB_BLACK);
}

// keep red-black properties after erasing a node
static void rb_erase_fixup(rbnode_t * node, rbnode_t * parent, rbtree_t * tree) {
    rbnode_t * other;
    while ((!node || (RB_BLACK == RB_COLOR(node))) && (node != tree->root)) {
        if (node == parent->left) {
            other = parent->right;
            if (RB_RED == RB_COLOR(other)) {
                rb_set_color(other,  RB_BLACK);
                rb_set_color(parent, RB_RED);
                rb_rotate_left(parent, tree);
                other = parent->right;
            }
            if ((!other->left  || (RB_BLACK == RB_COLOR(other->left))) &&
                (!other->right || (RB_BLACK == RB_COLOR(other->right)))) {
                rb_set_color(other, RB_RED);
                node = parent;
                parent = RB_PARENT(node);
            } else {
                if (!other->right || (RB_BLACK == RB_COLOR(other->right))) {
                    rb_set_color(other->left, RB_BLACK);
                    rb_set_color(other,       RB_RED);
                    rb_rotate_right(other, tree);
                    other = parent->right;
                }
                rb_set_color(other, RB_COLOR(parent));
                rb_set_color(parent, RB_BLACK);
                rb_set_color(other->right, RB_BLACK);
                rb_rotate_left(parent, tree);
                node = tree->root;
                break;
            }
        } else {
            other = parent->left;
            if (RB_RED == RB_COLOR(other)) {
                rb_set_color(other,  RB_BLACK);
                rb_set_color(parent, RB_RED);
                rb_rotate_right(parent, tree);
                other = parent->left;
            }
            if ((!other->left  || (RB_BLACK == RB_COLOR(other->left))) &&
                (!other->right || (RB_BLACK == RB_COLOR(other->right)))) {
                rb_set_color(other, RB_RED);
                node = parent;
                parent = RB_PARENT(node);
            } else {
                if (!other->left || (RB_BLACK == RB_COLOR(other->left))) {
                    rb_set_color(other->right, RB_BLACK);
                    rb_set_color(other, RB_RED);
                    rb_rotate_left(other, tree);
                    other = parent->left;
                }
                rb_set_color(other, RB_COLOR(parent));
                rb_set_color(parent, RB_BLACK);
                rb_set_color(other->left, RB_BLACK);
                rb_rotate_right(parent, tree);
                node = tree->root;
                break;
            }
        }
    }
    if (NULL != node) { rb_set_color(node, RB_BLACK); }
}

void rb_erase(rbnode_t * node, rbtree_t * tree) {
    rbnode_t * child = NULL;
    rbnode_t * parent;
    usize      color;

    if (NULL == node->left) {
        child = node->right;
    } else if (NULL == node->right) {
        child = node->left;
    } else {
        rbnode_t * old = node;
        rbnode_t * left;

        node = node->right;
        while (NULL != (left = node->left)) { node = left; }

        parent = RB_PARENT(old);
        if (NULL != parent) {
            if (parent->left == old) { parent->left  = node; }
            else                     { parent->right = node; }
        } else {
            tree->root = node;
        }

        child  = node->right;
        parent = RB_PARENT(node);
        color  = RB_COLOR(node);

        if (parent == old) {
            parent = node;
        } else {
            if (child) { rb_set_parent(child, parent); }
            parent->left = child;
            node->right = old->right;
            rb_set_parent(old->right, node);
        }

        node->parent_color = old->parent_color;
        node->left = old->left;
        rb_set_parent(old->left, node);
        goto color;
    }

    parent = RB_PARENT(node);
    color  = RB_COLOR(node);

    if (child) { rb_set_parent(child, parent); }
    if (parent) {
        if (parent->left == node) { parent->left  = child; }
        else                      { parent->right = child; }
    } else {
        tree->root = child;
    }

color:
    if (RB_BLACK == color) { rb_erase_fixup(child, parent, tree); }
}
