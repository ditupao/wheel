#ifndef LIBK_LIST_H
#define LIBK_LIST_H

#include <base.h>

typedef struct dlnode dlnode_t;
typedef struct dllist dllist_t;

struct dlnode {
    dlnode_t * prev;
    dlnode_t * next;
};

struct dllist {
    dlnode_t * head;
    dlnode_t * tail;
};

#define DLNODE_INIT ((dlnode_t) { NULL, NULL })
#define DLLIST_INIT ((dllist_t) { NULL, NULL })

static inline void dl_push_head(dllist_t * list, dlnode_t * node) {
    dlnode_t * head = list->head;
    list->head = node;
    node->prev = NULL;
    node->next = head;
    if (NULL == head) {
        list->tail = node;
    } else {
        head->prev = node;
    }
}

static inline void dl_push_tail(dllist_t * list, dlnode_t * node) {
    dlnode_t * tail = list->tail;
    list->tail = node;
    node->prev = tail;
    node->next = NULL;
    if (NULL == tail) {
        list->head = node;
    } else {
        tail->next = node;
    }
}

static inline dlnode_t * dl_pop_head(dllist_t * list) {
    dlnode_t * head = list->head;
    if (NULL != head) {
        list->head = head->next;
        if (NULL == head->next) {
            list->tail = NULL;
        } else {
            head->next->prev = NULL;
        }
    }
    return head;
}

static inline dlnode_t * dl_pop_tail(dllist_t * list) {
    dlnode_t * tail = list->tail;
    if (NULL != tail) {
        list->tail = tail->prev;
        if (NULL == tail->prev) {
            list->head = NULL;
        } else {
            tail->prev->next = NULL;
        }
    }
    return tail;
}

// insert node `x` before `y`
static inline void dl_insert_before(dllist_t * list, dlnode_t * x, dlnode_t * y) {
    if (NULL == y) {
        dl_push_tail(list, x);
    } else {
        dlnode_t * p = y->prev;
        x->prev = p;
        x->next = y;
        y->prev = x;
        if (NULL == p) {
            list->head = x;
        } else {
            p->next = x;
        }
    }
}

// insert node `x` after `y`
static inline void dl_insert_after(dllist_t * list, dlnode_t * x, dlnode_t * y) {
    if (NULL == y) {
        dl_push_head(list, x);
    } else {
        dlnode_t * n = y->next;
        x->prev = y;
        x->next = n;
        y->next = x;
        if (NULL == n) {
            list->tail = x;
        } else {
            n->prev = x;
        }
    }
}

static inline void dl_remove(dllist_t * list, dlnode_t * node) {
    dlnode_t * prev = node->prev;
    dlnode_t * next = node->next;
    if (NULL == prev) {
        list->head = next;
    } else {
        prev->next = next;
    }
    if (NULL == next) {
        list->tail = prev;
    } else {
        next->prev = prev;
    }
}

#endif // LIBK_LIST_H
