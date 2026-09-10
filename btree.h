// https://github.com/tidwall/btree.h
//
// Copyright 2024 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
//
// B-tree collection generator for C
//
// For a complete list of options visit:
// https://github.com/tidwall/btree.h#options

// The API namespace. 
// This is the prefix for all functions calls, and is also and the name of the
// root node structure.
#ifndef BTREE_NAME
#error BTREE_NAME required
#define BTREE_NAME unnamed_btree /* unused placeholder */
#endif

// macro concatenate
#define BTREE_CC(a, b) a ## b
#define BTREE_C(a, b)  BTREE_CC(a, b)

// API symbols are the calls available to the user.
#define BTREE_API(name)   BTREE_C(BTREE_C(BTREE_NAME, _), name)

// Internal symbols are prefixed with an underscore.
// These should not be directly called by the user.
#define BTREE_SYM(name)   BTREE_C(BTREE_C(BTREE_C(_, BTREE_NAME), _internal_), name)

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// The internal item type. This is used as both the value type and the key
// type, and can be pretty much anything.
#ifndef BTREE_TYPE
#error BTREE_TYPE required
#define BTREE_TYPE int /* unused placeholder */
#endif

// The "fanout" is maximum number of child nodes that a branch node may have.
// For example a fanout of 4 is equivalent to a 2-3-4 tree where a branch may
// have 2, 3, or 4 children and branches and leaves may have 1, 2, or 3 items.
// This implementation clamps the fanout to the range of 4 to 4096, and also
// rounds it down to the nearest even number. Such that a value of 9 becomes 8.
#ifndef BTREE_FANOUT
#define BTREE_FANOUTUSED 16
#elif BTREE_FANOUT < 4 
#define BTREE_FANOUTUSED 4
#elif BTREE_FANOUT > 4096
#define BTREE_FANOUTUSED 4096
#elif BTREE_FANOUT % 2 == 1
#define BTREE_FANOUTUSED (BTREE_FANOUT-1)
#else
#define BTREE_FANOUTUSED BTREE_FANOUT
#endif

// MAXITEMS and MINITEMS are the minimum and maximum number of items allowed in
// each node, respectively.
#define BTREE_MAXITEMS  (BTREE_FANOUTUSED-1)
#define BTREE_MINITEMS  (BTREE_MAXITEMS/2)

// Estimated compile time worst case max height for a 64-bit system.
// In other words, this is the maximum possible height of a tree when it's
// fully loaded with SIZE_MAX items.
#if (BTREE_MINITEMS+1) >= 128
#define BTREE_MAXHEIGHT 9      /* pow(128,9+1) >= 18446744073709551615UL */
#elif (BTREE_MINITEMS+1) >= 64
#define BTREE_MAXHEIGHT 10     /* pow(64,10+1) >= 18446744073709551615UL */
#elif (BTREE_MINITEMS+1) >= 32
#define BTREE_MAXHEIGHT 12     /* pow(32,12+1) >= 18446744073709551615UL */
#elif (BTREE_MINITEMS+1) >= 16
#define BTREE_MAXHEIGHT 15     /* pow(16,15+1) >= 18446744073709551615UL */
#elif (BTREE_MINITEMS+1) >= 8
#define BTREE_MAXHEIGHT 21     /* pow(8,21+1)  >= 18446744073709551615UL */
#elif (BTREE_MINITEMS+1) >= 4
#define BTREE_MAXHEIGHT 31     /* pow(4,31+1)  >= 18446744073709551615UL */
#else
#define BTREE_MAXHEIGHT 63
#endif

#define BTREE_INLINE inline
#ifdef __GNUC__
#define BTREE_NOINLINE __attribute__((noinline))
#else
#define BTREE_NOINLINE
#endif

// Provide a custom allocator using BTREE_MALLOC and BTREE_FREE.
// Such as:
//
//     #define BTREE_MALLOC return my_malloc(size);
//     #define BTREE_FREE   my_free(ptr);
//
// This will ensure that the tree will always use my_malloc/my_free instead of
// the standard malloc/free.
#if !defined(BTREE_MALLOC) || !defined(BTREE_FREE)

#include <stdlib.h>

#ifndef BTREE_MALLOC
#define BTREE_MALLOC return malloc(size);
#endif

#ifndef BTREE_FREE
#define BTREE_FREE free(ptr);
#endif
#endif

#ifndef BTREE_EXTERN
#ifdef BTREE_HEADER
#define BTREE_EXTERN extern
#else
#define BTREE_EXTERN static
#endif
#endif

// A path hint is a search optimization.
// It's most useful when bsearching.
// This implementation uses one thread local path hint per each btree namespace.
// See https://github.com/tidwall/btree/blob/master/PATH_HINT.md
#ifdef BTREE_NOPATHHINT
#undef BTREE_PATHHINT
#endif

// Convenient aliases to common types
#define BTREE_NODE struct BTREE_NAME
#define BTREE_ITEM BTREE_TYPE
#define BTREE_ITER struct BTREE_API(iter)
#define BTREE_SNODE struct BTREE_SYM(snode)

// The following status codes are private to this file only.
// Users should use the prefixed version such as bt_INSERTED as defined in the
// enum below.
#define BTREE_INSERTED    1  // New item was inserted
#define BTREE_REPLACED    2  // Item replaced an existing item
#define BTREE_DELETED     3  // Item was successfully deleted
#define BTREE_FOUND       4  // Item was successfully accessed
#define BTREE_NOTFOUND    5  // Item was not found
#define BTREE_OUTOFORDER  6  // Item is out of order
#define BTREE_FINISHED    7  // Callback iterator returned all items
#define BTREE_STOPPED     8  // Callback iterator was stopped early
#define BTREE_COPIED      9  // Tree was copied: `clone`, `copy`
#define BTREE_NOMEM       10 // Out of memory
#define BTREE_UNSUPPORTED 11 // Operation not supported

#ifndef BTREE_SOURCE

// Definitions

enum BTREE_API(status) {
    BTREE_C(BTREE_NAME, _INSERTED)    = BTREE_INSERTED,
    BTREE_C(BTREE_NAME, _REPLACED)    = BTREE_REPLACED,
    BTREE_C(BTREE_NAME, _DELETED)     = BTREE_DELETED,
    BTREE_C(BTREE_NAME, _FOUND)       = BTREE_FOUND,
    BTREE_C(BTREE_NAME, _NOTFOUND)    = BTREE_NOTFOUND,
    BTREE_C(BTREE_NAME, _OUTOFORDER)  = BTREE_OUTOFORDER,
    BTREE_C(BTREE_NAME, _FINISHED)    = BTREE_FINISHED,
    BTREE_C(BTREE_NAME, _STOPPED)     = BTREE_STOPPED,
    BTREE_C(BTREE_NAME, _COPIED)      = BTREE_COPIED,
    BTREE_C(BTREE_NAME, _NOMEM)       = BTREE_NOMEM,
    BTREE_C(BTREE_NAME, _UNSUPPORTED) = BTREE_UNSUPPORTED,
};

BTREE_NODE;
BTREE_ITER;

BTREE_EXTERN int BTREE_API(get)(BTREE_NODE **root, BTREE_ITEM key,
    BTREE_ITEM *item_out, void *udata);
BTREE_EXTERN int BTREE_API(insert)(BTREE_NODE **root, BTREE_ITEM item,
    BTREE_ITEM *item_out, void *udata);
BTREE_EXTERN int BTREE_API(delete)(BTREE_NODE **root, BTREE_ITEM key, 
    BTREE_ITEM *item_out, void *udata);
BTREE_EXTERN bool BTREE_API(contains)(BTREE_NODE **root, BTREE_ITEM key,
    void *udata);
BTREE_EXTERN void BTREE_API(clear)(BTREE_NODE **root, void *udata);

BTREE_EXTERN int BTREE_API(front)(BTREE_NODE **root, BTREE_ITEM *item_out,
    void *udata);
BTREE_EXTERN int BTREE_API(back)(BTREE_NODE **root, BTREE_ITEM *item_out,
    void *udata);
BTREE_EXTERN int BTREE_API(pop_front)(BTREE_NODE **root, BTREE_ITEM *item_out,
    void *udata);
BTREE_EXTERN int BTREE_API(pop_back)(BTREE_NODE **root, BTREE_ITEM *item_out,
    void *udata);
BTREE_EXTERN int BTREE_API(push_front)(BTREE_NODE **root, BTREE_ITEM item,
    void *udata);
BTREE_EXTERN int BTREE_API(push_back)(BTREE_NODE **root, BTREE_ITEM item,
    void *udata);

BTREE_EXTERN int BTREE_API(copy)(BTREE_NODE **root, BTREE_NODE **newroot,
    void *udata);
BTREE_EXTERN int BTREE_API(clone)(BTREE_NODE **root, BTREE_NODE **newroot,
    void *udata);
BTREE_EXTERN int BTREE_API(compare)(BTREE_ITEM a, BTREE_ITEM b, void *udata);
BTREE_EXTERN bool BTREE_API(less)(BTREE_ITEM a, BTREE_ITEM b, void *udata);

// Optimized for counted B-trees (works with indexes) (rank=index_of,
// select=get_at)
BTREE_EXTERN int BTREE_API(insert_at)(BTREE_NODE **root, size_t index,
    BTREE_ITEM item, void *udata);
BTREE_EXTERN int BTREE_API(delete_at)(BTREE_NODE **root, size_t index,
    BTREE_ITEM *item_out, void *udata);
BTREE_EXTERN int BTREE_API(replace_at)(BTREE_NODE **root, size_t index,
    BTREE_ITEM item, BTREE_ITEM *item_out, void *udata);
BTREE_EXTERN int BTREE_API(get_at)(BTREE_NODE **root, size_t index,
    BTREE_ITEM *item_out, void *udata);
BTREE_EXTERN int BTREE_API(index_of)(BTREE_NODE **root, BTREE_ITEM key,
    size_t *index, void *udata);
BTREE_EXTERN size_t BTREE_API(count)(BTREE_NODE **root, void *udata);

// Cursor Iterators
BTREE_EXTERN void BTREE_API(iter_init)(BTREE_NODE **root, BTREE_ITER **iter,
    void *udata);
BTREE_EXTERN int BTREE_API(iter_status)(BTREE_ITER *iter);
BTREE_EXTERN bool BTREE_API(iter_valid)(BTREE_ITER *iter);
BTREE_EXTERN void BTREE_API(iter_release)(BTREE_ITER *iter);
BTREE_EXTERN void BTREE_API(iter_item)(BTREE_ITER *iter, BTREE_ITEM *item);
BTREE_EXTERN void BTREE_API(iter_next)(BTREE_ITER *iter);

// Curstor iterator seekers
BTREE_EXTERN void BTREE_API(iter_seek)(BTREE_ITER *iter, BTREE_ITEM key);
BTREE_EXTERN void BTREE_API(iter_seek_desc)(BTREE_ITER *iter, BTREE_ITEM key);
BTREE_EXTERN void BTREE_API(iter_scan)(BTREE_ITER *iter);
BTREE_EXTERN void BTREE_API(iter_scan_desc)(BTREE_ITER *iter);
BTREE_EXTERN void BTREE_API(iter_seek_at)(BTREE_ITER *iter, size_t index);
BTREE_EXTERN void BTREE_API(iter_seek_at_desc)(BTREE_ITER *iter, size_t index);

// Callback iterators
BTREE_EXTERN int BTREE_API(scan)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item, 
    void *udata), void *udata);
BTREE_EXTERN int BTREE_API(scan_desc)(BTREE_NODE **root, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);
BTREE_EXTERN int BTREE_API(seek)(BTREE_NODE **root, BTREE_ITEM key, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);
BTREE_EXTERN int BTREE_API(seek_desc)(BTREE_NODE **root, BTREE_ITEM key, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);
BTREE_EXTERN int BTREE_API(seek_at)(BTREE_NODE **root, size_t index,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);
BTREE_EXTERN int BTREE_API(seek_at_desc)(BTREE_NODE **root, size_t index,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);

// General information
BTREE_EXTERN int BTREE_API(feat_maxitems)(void);
BTREE_EXTERN int BTREE_API(feat_minitems)(void);
BTREE_EXTERN int BTREE_API(feat_maxheight)(void);
BTREE_EXTERN int BTREE_API(feat_fanout)(void);
BTREE_EXTERN bool BTREE_API(feat_counted)(void);
BTREE_EXTERN bool BTREE_API(feat_ordered)(void);
BTREE_EXTERN bool BTREE_API(feat_cow)(void);
BTREE_EXTERN bool BTREE_API(feat_atomics)(void);
BTREE_EXTERN bool BTREE_API(feat_bsearch)(void);
BTREE_EXTERN bool BTREE_API(feat_pathhint)(void);
BTREE_EXTERN size_t BTREE_API(height)(BTREE_NODE **root, void *udata);
BTREE_EXTERN bool BTREE_API(sane)(BTREE_NODE **root, void *udata);

// Read functions that return items which are intended to be mutated.
// These perform copy-on-write on internal nodes and copies the items before
// returning them to the user. 
BTREE_EXTERN int BTREE_API(get_mut)(BTREE_NODE **root, BTREE_ITEM key,
    BTREE_ITEM *item_out, void *udata);
BTREE_EXTERN int BTREE_API(get_at_mut)(BTREE_NODE **root, size_t index,
    BTREE_ITEM *item_out, void *udata);
BTREE_EXTERN int BTREE_API(front_mut)(BTREE_NODE **root, BTREE_ITEM *item_out,
    void *udata);
BTREE_EXTERN int BTREE_API(back_mut)(BTREE_NODE **root, BTREE_ITEM *item_out,
    void *udata);
BTREE_EXTERN void BTREE_API(iter_init_mut)(BTREE_NODE **root, BTREE_ITER **iter,
    void *udata);
BTREE_EXTERN int BTREE_API(scan_mut)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item,
    void *udata), void *udata);
BTREE_EXTERN int BTREE_API(scan_desc_mut)(BTREE_NODE **root,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);
BTREE_EXTERN int BTREE_API(seek_mut)(BTREE_NODE **root, BTREE_ITEM key,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);
BTREE_EXTERN int BTREE_API(seek_desc_mut)(BTREE_NODE **root, BTREE_ITEM key,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);
BTREE_EXTERN int BTREE_API(seek_at_mut)(BTREE_NODE **root, size_t index,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);
BTREE_EXTERN int BTREE_API(seek_at_desc_mut)(BTREE_NODE **root, size_t index,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata);

// Access direct mutable references... use with care.
BTREE_EXTERN int BTREE_API(get_mut_ref)(BTREE_NODE **root, BTREE_ITEM key,
    BTREE_ITEM **item, void *udata);

#endif // !BTREE_SOURCE

#ifndef BTREE_HEADER

// IMPLEMENTATION

BTREE_NOINLINE
static void *BTREE_SYM(malloc)(size_t size, void *udata) {
    (void)size, (void)udata;
    BTREE_MALLOC
}

static void BTREE_SYM(free)(void *ptr, size_t size, void *udata) {
    (void)ptr, (void)size, (void)udata;
    BTREE_FREE
}

#ifdef BTREE_LESS
#ifdef BTREE_COMPARE
#error \
BTREE_COMPARE and BTREE_LESS cannot be both defined
#endif
#ifdef BTREE_KEYED
// Using nested compare for keyed collection type
static bool BTREE_SYM(less)(BTREE_ITEM a2, BTREE_ITEM b2, void *udata) {
    BTREE_KEYTYPE a = a2.key, b = b2.key;
    (void)a, (void)b, (void)udata;
    BTREE_LESS
}
#else
static bool BTREE_SYM(less)(BTREE_ITEM a, BTREE_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    BTREE_LESS
}
#endif
static int BTREE_SYM(compare)(BTREE_ITEM a, BTREE_ITEM b, void *udata) {
    return BTREE_SYM(less)(a, b, udata) ? -1 :
           BTREE_SYM(less)(b, a, udata) ? 1 :
           0;
}
#elif defined(BTREE_COMPARE)
#ifdef BTREE_KEYED
// Using nested compare for keyed collection type
static int BTREE_SYM(compare)(BTREE_ITEM a2, BTREE_ITEM b2, void *udata) {
    BTREE_KEYTYPE a = a2.key, b = b2.key;
    (void)a, (void)b, (void)udata;
    BTREE_COMPARE
}
#else
static int BTREE_SYM(compare)(BTREE_ITEM a, BTREE_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    BTREE_COMPARE
}
#endif
static bool BTREE_SYM(less)(BTREE_ITEM a, BTREE_ITEM b, void *udata) {
    return BTREE_SYM(compare)(a, b, udata) < 0;
}
#else
static bool BTREE_SYM(less)(BTREE_ITEM a, BTREE_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    return false;
}
static int BTREE_SYM(compare)(BTREE_ITEM a, BTREE_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    return -1;
}
#if !defined(BTREE_NOORDER)
#error \
Neither BTREE_COMPARE nor BTREE_LESS were defined. \
Alternatively define BTREE_NOORDER if only the "Counted B-tree" API is desired. \
Visit https://github.com/tidwall/btree.h for more information.
#endif
#endif

#if defined(BTREE_NOORDER) && (defined(BTREE_LESS) || defined(BTREE_COMPARE))
#error \
Neither BTREE_COMPARE nor BTREE_LESS are allowed when BTREE_NOORDER is defined. \
Visit https://github.com/tidwall/btree.h for more information.
#endif

#ifdef BTREE_MAYBELESSEQUAL
static bool BTREE_SYM(maybelessequal)(BTREE_ITEM a, BTREE_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    BTREE_MAYBELESSEQUAL
}
#endif

static bool BTREE_SYM(item_copy)(BTREE_ITEM item, BTREE_ITEM *copy, void *udata) {
    (void)item, (void)copy, (void)udata;
#ifdef BTREE_ITEMCOPY
    BTREE_ITEMCOPY
#else
    *copy = item;
    return true;
#endif
}

static void BTREE_SYM(item_free)(BTREE_ITEM item, void *udata) {
    (void)item, (void)udata;
#ifdef BTREE_ITEMFREE
    BTREE_ITEMFREE
#endif
}

#ifdef BTREE_COW

#ifdef BTREE_NOATOMICS

typedef int BTREE_SYM(rc_t);
static void BTREE_SYM(rc_init)(BTREE_SYM(rc_t) *rc) {
    *rc = 0;
}
static void BTREE_SYM(rc_retain)(BTREE_SYM(rc_t) *rc) {
    (*rc)++;
}
static bool BTREE_SYM(rc_release)(BTREE_SYM(rc_t) *rc) {
    return (*rc)-- == 1;
}
static bool BTREE_SYM(rc_shared)(BTREE_SYM(rc_t) *rc) {
    return *rc > 1;
}

#else

#include <stdatomic.h>

typedef atomic_int BTREE_SYM(rc_t);
static void BTREE_SYM(rc_init)(BTREE_SYM(rc_t) *rc) {
    atomic_init(rc, 0);
}
static void BTREE_SYM(rc_retain)(BTREE_SYM(rc_t) *rc) {
    atomic_fetch_add_explicit(rc, 1, __ATOMIC_RELAXED);
}
static bool BTREE_SYM(rc_release)(BTREE_SYM(rc_t) *rc) {
    return atomic_fetch_sub_explicit(rc, 1, __ATOMIC_ACQ_REL) == 1;
}
static bool BTREE_SYM(rc_shared)(BTREE_SYM(rc_t) *rc) {
    return atomic_load_explicit(rc, __ATOMIC_ACQUIRE) > 1;
}

#endif
#endif

BTREE_NODE {
    BTREE_ITEM items[BTREE_MAXITEMS];  // all items in node, ordered
#ifdef BTREE_COW
    BTREE_SYM(rc_t) rc; // reference counter
#endif
    short len; // number of items in this node
    char height; // tree height (one is leaf)
    bool isleaf; // node is a leaf
    
    // leaves omit the following fields
    BTREE_NODE *children[BTREE_MAXITEMS+1]; // child nodes
#ifdef BTREE_COUNTED
    size_t counts[BTREE_MAXITEMS+1]; // counts for child nodes
#endif
};

#ifdef BTREE_ASSERT
#include <assert.h>
#undef BTREE_ASSERT
#define BTREE_ASSERT(cond) assert(cond)
#else
#define BTREE_ASSERT(cond)(void)0
#endif

static int BTREE_SYM(feat_maxitems)(void) {
    return BTREE_MAXITEMS;
}
static int BTREE_SYM(feat_minitems)(void) {
    return BTREE_MINITEMS;
}
static int BTREE_SYM(feat_maxheight)(void) {
    return BTREE_MAXHEIGHT;
}
static int BTREE_SYM(feat_fanout)(void) {
    return BTREE_FANOUTUSED;
}
static bool BTREE_SYM(feat_counted)(void) {
#ifdef BTREE_COUNTED
    return true;
#else
    return false;
#endif
}
static bool BTREE_SYM(feat_ordered)(void) {
#ifdef BTREE_NOORDER
    return false;
#else
    return true;
#endif
}

static bool BTREE_SYM(feat_cow)(void) {
#ifdef BTREE_COW
    return true;
#else
    return false;
#endif
}
static bool BTREE_SYM(feat_bsearch)(void) {
#ifdef BTREE_BSEARCH
    return true;
#else
    return false;
#endif
}
static bool BTREE_SYM(feat_pathhint)(void) {
#ifdef BTREE_PATHHINT
    return true;
#else
    return false;
#endif
}

static bool BTREE_SYM(feat_atomics)(void) {
#ifndef BTREE_NOATOMICS
    return true;
#else
    return false;
#endif
}

#define BTREE_LEAF_SIZE offsetof(BTREE_NODE, children)
#define BTREE_BRANCH_SIZE sizeof(BTREE_NODE)
#define BTREE_NODE_SIZE(node) ((node)->isleaf?BTREE_LEAF_SIZE:BTREE_BRANCH_SIZE)

static BTREE_NODE *BTREE_SYM(alloc_node)(bool isleaf, void *udata) {
    void *ptr = isleaf ? 
        BTREE_SYM(malloc)(BTREE_LEAF_SIZE, udata) :
        BTREE_SYM(malloc)(BTREE_BRANCH_SIZE, udata);
    if (!ptr) {
        return 0;
    }
    BTREE_NODE *node = (BTREE_NODE*)ptr;
#ifdef BTREE_COW
    BTREE_SYM(rc_init)(&node->rc);
    BTREE_SYM(rc_retain)(&node->rc);
#endif
    node->isleaf = isleaf;
    node->height = 0;
    node->len = 0;
    return node;
}

// returns the number of items in a node by counting, recursively
static size_t BTREE_SYM(deepcount)(BTREE_NODE *node) {
    size_t count = (size_t)node->len;
    if (!node->isleaf) {
        for (int i = 0; i < node->len+1; i++) {
            count += BTREE_SYM(deepcount)(node->children[i]);
        }
    }
    return count;
}

// returns the height of the node counting the depth, recursively
static int BTREE_SYM(deepheight)(BTREE_NODE *node) {
    int height = 0;
    while (1) {
        height++;
        if (node->isleaf) {
            return height;
        }
        node = node->children[0];
    }
}

static bool BTREE_SYM(sane0)(BTREE_NODE *node, void *udata, int depth) {
    // check the number of items in node.
    if (depth == 0) {
        // the root is allowed to have one item.
        if (node->len < 1 || node->len > BTREE_MAXITEMS) {
            return false;
        }
    } else {
        if (node->len < BTREE_MINITEMS || node->len > BTREE_MAXITEMS) {
            return false;
        }
    }
    if (node->isleaf && node->height != 1) {
        return false;
    }
    if (!node->isleaf && node->height < 2) {
        return false;
    }
    // Check the height
    if (node->height != BTREE_SYM(deepheight)(node)) {
        return false;
    }
    // check the order of items.
#ifndef BTREE_NOORDER
    for (int i = 1; i < node->len; i++) {
        if (BTREE_SYM(compare)(node->items[i-1], node->items[i], udata) >= 0) {
            return false;
        }
    }
#endif
    if (!node->isleaf) {
        // continue sanity test down the tree.
#ifndef BTREE_NOORDER
        // Check the order of each branch item, comparing to the children to
        // the left and right.
        for (int i = 0; i < node->len; i++) {
            if (node->children[i]->len > 0 && 
                node->children[i]->len <= BTREE_MAXITEMS &&
                node->children[i+1]->len > 0 &&
                node->children[i+1]->len <= BTREE_MAXITEMS)
            {
                if (BTREE_SYM(compare)(
                    node->children[i]->items[node->children[i]->len-1], 
                    node->items[i], udata) >= 0 ||
                    BTREE_SYM(compare)(node->items[i],
                    node->children[i+1]->items[0], udata) >= 0)
                {
                    return false;
                }
            }
        }
#endif
        // check the sanity of child node
        for (int i = 0; i <= node->len; i++) {
#ifdef BTREE_COUNTED
            size_t count = BTREE_SYM(deepcount)(node->children[i]);
            if (count != node->counts[i]) {
                return false;
            }
#endif
            if (!BTREE_SYM(sane0)(node->children[i], udata, depth+1)) {
                return false;
            }
        }
    }
    return true;
}

// sanity checker
static bool BTREE_SYM(sane)(BTREE_NODE **root, void *udata) {
    bool sane = true;
    if (*root) {
        return BTREE_SYM(sane0)(*root, udata, 0);
    }
    return sane;
}

static size_t BTREE_SYM(count0)(BTREE_NODE *node) {
#ifndef BTREE_COUNTED
    return BTREE_SYM(deepcount)(node);
#else
    size_t count = node->len;
    if (!node->isleaf) {
        for (int i = 0; i <= node->len; i++) {
            count += node->counts[i];
        }
    }
    return count;
#endif
}

// returns the number of items in tree
static size_t BTREE_SYM(count)(BTREE_NODE **root, void *udata) {
    (void)udata;
    return *root ? BTREE_SYM(count0)(*root) : 0;
}

// returns the number of items in tree
static size_t BTREE_SYM(height)(BTREE_NODE **root, void *udata) {
    (void)udata;
    return *root ? (size_t)(*root)->height : 0;
}

// Returns the number of items in child node at index.
// This will use the 'count' value if available.
static size_t BTREE_SYM(node_count)(BTREE_NODE *branch, int node_index) {
#ifndef BTREE_COUNTED
    return BTREE_SYM(count0)(branch->children[node_index]);
#else
    return branch->counts[node_index];
#endif
}

static void BTREE_SYM(node_free)(BTREE_NODE *node, void *udata) {
#ifdef BTREE_COW
    if (!BTREE_SYM(rc_release)(&node->rc)) {
        return;
    }
#endif
    if (!node->isleaf) {
        for (int i = 0; i < node->len+1; i++) {
            BTREE_SYM(node_free)(node->children[i], udata);
        }
    }
    for (int i = 0; i < node->len; i++) {
        BTREE_SYM(item_free)(node->items[i], udata);
    }
    BTREE_SYM(free)(node, BTREE_NODE_SIZE(node), udata);
}

/// Free the tree!
static void BTREE_SYM(clear)(BTREE_NODE **root, void *udata) {
    if (*root) {
        BTREE_SYM(node_free)(*root, udata);
        *root = 0;
    }
}

#ifdef BTREE_BSEARCH
BTREE_INLINE
static int BTREE_SYM(search_bsearch)(BTREE_ITEM *items, int nitems,
    BTREE_ITEM key, void *udata, int *found)
{
    // Standard bsearch. Balanced. Relies on branch prediction.
    int i = 0;
    int n = nitems;
    while (i < n) {
        int j = (i + n) / 2;
        int cmp = BTREE_SYM(compare)(key, items[j], udata);
        if (cmp < 0) {
            n = j;
        } else if (cmp > 0) {
            i = j+1;
        } else {
            *found = 1;
            return j;
        }
    }
    *found = 0;
    return i;
}
#else
BTREE_INLINE
static int BTREE_SYM(search_linear)(BTREE_ITEM *items, int nitems, BTREE_ITEM key,
    void *udata, int *found)
{
    int i = 0;
    *found = 0;
#ifdef BTREE_MAYBELESSEQUAL
    while (nitems-i >= 4) {
        if (BTREE_SYM(maybelessequal)(key, items[i], udata)){goto compare;}i++;
        if (BTREE_SYM(maybelessequal)(key, items[i], udata)){goto compare;}i++;
        if (BTREE_SYM(maybelessequal)(key, items[i], udata)){goto compare;}i++;
        if (BTREE_SYM(maybelessequal)(key, items[i], udata)){goto compare;}i++;
    }
    for (; i < nitems; i++) {
        if (BTREE_SYM(maybelessequal)(key, items[i], udata)) {
            goto compare;
        }
    }
#endif
#ifdef BTREE_LESS
    for (; i < nitems; i++) {
#ifdef BTREE_MAYBELESSEQUAL
    compare:
#endif
        if (BTREE_SYM(less)(key, items[i], udata)) {
            break;
        }
        if (!BTREE_SYM(less)(items[i], key, udata)) {
            *found = 1;
            break;
        }
    }
#else
    int cmp;
    for (; i < nitems; i++) {
#ifdef BTREE_MAYBELESSEQUAL
    compare:
#endif
        cmp = BTREE_SYM(compare)(key, items[i], udata);
        if (cmp <= 0) {
            *found = cmp == 0;
            break;
        }
    }
#endif
    return i;
}
#endif


static int BTREE_SYM(search)(BTREE_NODE *node, BTREE_ITEM key, void *udata,
    int *found, int depth)
{
#ifndef BTREE_PATHHINT
    (void)depth; // not used
#ifdef BTREE_BSEARCH
    return BTREE_SYM(search_bsearch)(node->items, node->len, key, udata, found);
#else // BTREE_LINEAR
    return BTREE_SYM(search_linear)(node->items, node->len, key, udata, found);
#endif
#else
    // path hints are activated
    BTREE_ITEM *items = node->items;
    int nitems = node->len;
    int i = 0;
    static __thread uint8_t BTREE_SYM(ghint)[BTREE_MAXHEIGHT] = { 0 };
    int j = BTREE_SYM(ghint)[depth];
    if (j >= node->len)  {
        j = node->len-1;
    }
    int cmp = BTREE_SYM(compare)(key, items[j], udata);
    if (cmp == 0) {
        *found = 1;
        return j;
    } else if (cmp < 0) {
        if (j == 0) {
            *found = 0;
            return 0;
        }
        int cmp = BTREE_SYM(compare)(items[j-1], key, udata);
        if (cmp == 0) {
            *found = 1;
            return j-1;
        } else if (cmp < 0) {
            *found = 0;
            return j;
        } else {
            nitems = j;
        }
    } else if (cmp > 0) {
        if (j == node->len-1) {
            *found = 0;
            i = node->len;
            goto okhint;
        }
        int cmp = BTREE_SYM(compare)(key, items[j+1], udata);
        if (cmp == 0) {
            *found = 1;
            i = j+1;
            goto okhint;
        } else if (cmp < 0) {
            *found = 0;
            i = j+1;
            goto okhint;
        } else {
            nitems -= j;
            i = j;
        }
    }
#ifdef BTREE_BSEARCH
    i += BTREE_SYM(search_bsearch)(items+i, nitems, key, udata, found);
#else // BTREE_LINEAR
    i += BTREE_SYM(search_linear)(items+i, nitems, key, udata, found);
#endif
okhint:
    BTREE_SYM(ghint)[depth] = (uint8_t)i;
    return i;
#endif
}

static void BTREE_SYM(print_spaces)(FILE *file, int depth) {
    for (int i = 0; i < depth; i++) {
        fprintf(file, "    ");
    }
}

static void BTREE_SYM(node_print)(BTREE_NODE *node, FILE *file,
    void(*print_item)(BTREE_ITEM item, FILE *file, void *udata),
    int depth, void *udata)
{
    BTREE_SYM(print_spaces)(file, depth);
    fprintf(file, ".isleaf=%d ", node->isleaf);
#ifdef BTREE_COW
    fprintf(file, ".rc=%d ", node->rc);
#endif
    fprintf(file, ".height=%d .len=%d ", node->height, node->len);
    fprintf(file, ".items=[ ");
    for (int i = 0; i < node->len; i++) {
        if (print_item) {
            print_item(node->items[i], file, udata);
            fprintf(file, " ");
        }
    }
    fprintf(file, "] ");
    if (!node->isleaf) {
#ifdef BTREE_COUNTED
        fprintf(file, ".counts=[ ");
        for (int i = 0; i <= node->len; i++) {
            fprintf(file, "%zu ", node->counts[i]);
        }
        fprintf(file, "] ");
#endif
        fprintf(file, ".children=[\n");
        for (int i = 0; i <= node->len; i++) {
            BTREE_SYM(node_print)(node->children[i], file, print_item, 
                depth+1, udata);
        }
        BTREE_SYM(print_spaces)(file, depth);
        fprintf(file, "] ");
    }
    fprintf(file, "\n");
}

static void BTREE_SYM(print_feats)(FILE *file) {
    fprintf(file, "( .fanout=%d .minitems=%d .maxitems=%d .counted=%d "
        ".bsearch=%d .pathhint=%d .cow=%d .atomics=%d", 
        BTREE_SYM(feat_fanout)(), 
        BTREE_SYM(feat_minitems)(), 
        BTREE_SYM(feat_maxitems)(),
        BTREE_SYM(feat_counted)(),
        BTREE_SYM(feat_bsearch)(),
        BTREE_SYM(feat_pathhint)(),
        BTREE_SYM(feat_cow)(),
        BTREE_SYM(feat_atomics)()
    );
    fprintf(file, " )\n");
}

static void BTREE_SYM(print)(BTREE_NODE **root, FILE *file,
    void(*print_item)(BTREE_ITEM item, FILE *file, void *udata), 
    void *udata)
{
    BTREE_SYM(print_feats)(file);
    if (*root) {
        BTREE_SYM(node_print)(*root, file, print_item, 0, udata);
    }
}

static BTREE_NODE *BTREE_SYM(node_copy)(BTREE_NODE *node, bool deep, void *udata) {
    BTREE_NODE *node2 = BTREE_SYM(alloc_node)(node->isleaf, udata);
    if (!node2) {
        return 0;
    }
    node2->len = node->len;
    node2->height = node->height;
    
    int icopied = 0;
    int ccopied = 0;

    // Copy items
    for (int i = 0; i < node->len; i++) {
        if (!BTREE_SYM(item_copy)(node->items[i], &node2->items[i], udata)) {
            goto fail;
        }
        icopied++;
    }
    if (!node->isleaf) {
        // Copy children
        for (int i = 0; i <= node->len; i++) {
#ifdef BTREE_COW
            if (!deep) {
                node2->children[i] = node->children[i];
                BTREE_SYM(rc_retain)(&node2->children[i]->rc);
            } else {
#else 
            {
#endif
                node2->children[i] = BTREE_SYM(node_copy)(node->children[i], 
                    deep, udata);
                if (!node2->children[i]) {
                    goto fail;
                }
            }
            ccopied++;
        }
#ifdef BTREE_COUNTED
        for (int i = 0; i <= node->len; i++) {
            node2->counts[i] = node->counts[i];
        }
#endif
    }
    return node2;
fail:
    // Somthing failed to copy. Assume NOMEM and revert the allocated node.
    for (int i = 0; i < icopied; i++) {
        BTREE_SYM(item_free)(node2->items[i], udata);
    }
    if (!node->isleaf) {
        for (int i = 0; i < ccopied; i++) {
            BTREE_SYM(node_free)(node2->children[i], udata);
        }
    } 
    BTREE_SYM(free)(node2, BTREE_NODE_SIZE(node2), udata);
    return 0;
}

// Check if node is being shared (referenced) by other clones.
static bool BTREE_SYM(shared)(BTREE_NODE *node) {
#ifndef BTREE_COW
    (void)node;
    return false;
#else
    return BTREE_SYM(rc_shared)(&node->rc);
#endif
}

// Perform copy-on-write operation. 
// Returns true on success or false on failure (NOMEM).
static bool BTREE_SYM(cow)(BTREE_NODE **node, void *udata) {
#ifndef BTREE_COW
    (void)node, (void)udata;
#else
    if (BTREE_SYM(shared)(*node)) {
        BTREE_NODE *node2 = BTREE_SYM(node_copy)(*node, false, udata);
        if (!node2) {
            return false;
        }
        BTREE_SYM(node_free)(*node, udata);
        *node = node2;
    }
#endif
    return true;
}

static bool BTREE_SYM(node_scan)(BTREE_NODE *node, bool(*iter)(BTREE_ITEM item, 
    void *udata), void *udata)
{
    if (node->isleaf) {
        for (int i = 0; i < node->len; i++) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    for (int i = 0; i < node->len; i++) {
        if (!BTREE_SYM(node_scan)(node->children[i], iter, udata)) {
            return false;
        }
        if (!iter(node->items[i], udata)) {
            return false;
        }
    }
    return BTREE_SYM(node_scan)(node->children[node->len], iter, udata);
}

static int BTREE_SYM(scan)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item,
    void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(node_scan)(*root, iter, udata)) {
            status = BTREE_STOPPED;
        }
    }
    return status;
}

static bool BTREE_SYM(node_scan_mut)(BTREE_NODE *node, bool(*iter)(BTREE_ITEM item, 
    void *udata), void *udata, int *status)
{
    if (node->isleaf) {
        for (int i = 0; i < node->len; i++) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    for (int i = 0; i < node->len; i++) {
        if (!BTREE_SYM(cow)(&node->children[i], udata)) {
            *status = BTREE_NOMEM;
            return false;
        }
        if (!BTREE_SYM(node_scan_mut)(node->children[i], iter, udata, status)) {
            return false;
        }
        if (!iter(node->items[i], udata)) {
            return false;
        }
    }
    if (!BTREE_SYM(cow)(&node->children[node->len], udata)) {
        *status = BTREE_NOMEM;
        return false;
    }
    return BTREE_SYM(node_scan_mut)(node->children[node->len], iter, udata,
        status);
}

static int BTREE_SYM(scan_mut)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item,
    void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(cow)(root, udata)) {
            return BTREE_NOMEM;
        }
        if (!BTREE_SYM(node_scan_mut)(*root, iter, udata, &status)) {
            if (status == BTREE_FINISHED) {
                status = BTREE_STOPPED;
            }
        }
    }
    return status;
}

static bool BTREE_SYM(node_scan_desc)(BTREE_NODE *node, bool(*iter)(
    BTREE_ITEM item, void *udata), void *udata)
{
    if (node->isleaf) {
        for (int i = node->len-1; i >= 0; i--) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    if (!BTREE_SYM(node_scan_desc)(node->children[node->len], iter, udata)) {
        return false;
    }
    for (int i = node->len-1; i >= 0; i--) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!BTREE_SYM(node_scan_desc)(node->children[i], iter, udata)) {
            return false;
        }
    }
    return true;
}

static int BTREE_SYM(scan_desc)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item,
    void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(node_scan_desc)(*root, iter, udata)) {
            status = BTREE_STOPPED;
        }
    }
    return status;
}

static bool BTREE_SYM(node_scan_desc_mut)(BTREE_NODE *node, bool(*iter)(
    BTREE_ITEM item, void *udata), void *udata, int *status)
{
    if (node->isleaf) {
        for (int i = node->len-1; i >= 0; i--) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    if (!BTREE_SYM(cow)(&node->children[node->len], udata)) {
        *status = BTREE_NOMEM;
        return false;
    }
    if (!BTREE_SYM(node_scan_desc_mut)(node->children[node->len], iter, udata,
        status))
    {
        return false;
    }
    for (int i = node->len-1; i >= 0; i--) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!BTREE_SYM(cow)(&node->children[i], udata)) {
            *status = BTREE_NOMEM;
            return false;
        }
        if (!BTREE_SYM(node_scan_desc_mut)(node->children[i], iter, udata,
            status))
        {
            return false;
        }
    }
    return true;
}

static int BTREE_SYM(scan_desc_mut)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item,
    void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(cow)(root, udata)) {
            return BTREE_NOMEM;
        }
        if (!BTREE_SYM(node_scan_desc_mut)(*root, iter, udata, &status)) {
            if (status == BTREE_FINISHED) {
                status = BTREE_STOPPED;
            }
        }
    }
    return status;
}

static bool BTREE_SYM(node_seek)(BTREE_NODE *node, BTREE_ITEM key,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata, int depth)
{
    int found;
    int i = BTREE_SYM(search)(node, key, udata, &found, depth);
    if (!found) {
        if (!node->isleaf) {
            if (!BTREE_SYM(node_seek)(node->children[i], key, iter, udata, 
                depth+1))
            {
                return false;
            }
        }
    }
    for (; i < node->len; i++) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!node->isleaf) {
            if (!BTREE_SYM(node_scan)(node->children[i+1], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static int BTREE_SYM(seek)(BTREE_NODE **root, BTREE_ITEM key, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(node_seek)(*root, key, iter, udata, 0)) {
            status = BTREE_STOPPED;
        }
    }
    return status;
}

static bool BTREE_SYM(node_seek_at)(BTREE_NODE *node, size_t index,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    if (node->isleaf) {
        for (size_t i = index; i < (size_t)node->len; i++) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    bool found = false;
    int i = 0;
    for (; i < node->len; i++) {
        size_t count = BTREE_SYM(node_count)(node, i);
        if (index <= count) {
            found = index == count;
            break;
        }
        index -= count + 1;
    }
    if (!found) {
        if (!BTREE_SYM(node_seek_at)(node->children[i], index, iter, udata)) {
            return false;
        }
    }
    for (; i < node->len; i++) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!node->isleaf) {
            if (!BTREE_SYM(node_scan)(node->children[i+1], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static int BTREE_SYM(seek_at)(BTREE_NODE **root, size_t index, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(node_seek_at)(*root, index, iter, udata)) {
            status = BTREE_STOPPED;
        }
    }
    return status;
}

static bool BTREE_SYM(node_seek_at_desc)(BTREE_NODE *node, size_t index,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    if (node->isleaf) {
        int i;
        if (index >= (size_t)node->len) {
            i = node->len-1;
        } else {
            i = (int)index;
        }
        for (; i >= 0; i--) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    bool found = false;
    int i = 0;
    for (; i < node->len; i++) {
        size_t count = BTREE_SYM(node_count)(node, i);
        if (index <= count) {
            found = index == count;
            break;
        }
        index -= count + 1;
    }
    if (!found) {
        if (!BTREE_SYM(node_seek_at_desc)(node->children[i], index, iter, 
            udata))
        {
            return false;
        }
        i--;
    }
    for (; i >= 0; i--) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!node->isleaf) {
            if (!BTREE_SYM(node_scan_desc)(node->children[i], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static int BTREE_SYM(seek_at_desc)(BTREE_NODE **root, size_t index, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(node_seek_at_desc)(*root, index, iter, udata)) {
            status = BTREE_STOPPED;
        }
    }
    return status;
}

static bool BTREE_SYM(node_seek_at_mut)(BTREE_NODE *node, size_t index,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata, int *status)
{
    if (node->isleaf) {
        for (size_t i = index; i < (size_t)node->len; i++) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    bool found = false;
    int i = 0;
    for (; i < node->len; i++) {
        size_t count = BTREE_SYM(node_count)(node, i);
        if (index <= count) {
            found = index == count;
            break;
        }
        index -= count + 1;
    }
    if (!found) {
        if (!BTREE_SYM(cow)(&node->children[i], udata)) {
            *status = BTREE_NOMEM;
            return false;
        }
        if (!BTREE_SYM(node_seek_at_mut)(node->children[i], index, iter, udata,
            status))
        {
            return false;
        }
    }
    for (; i < node->len; i++) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!node->isleaf) {
            if (!BTREE_SYM(cow)(&node->children[i+1], udata)) {
                *status = BTREE_NOMEM;
                return false;
            }
            if (!BTREE_SYM(node_scan_mut)(node->children[i+1], iter, udata,
                status))
            {
                return false;
            }
        }
    }
    return true;
}


static int BTREE_SYM(seek_at_mut)(BTREE_NODE **root, size_t index, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(cow)(root, udata)) {
            return BTREE_NOMEM;
        }
        if (!BTREE_SYM(node_seek_at_mut)(*root, index, iter, udata, &status)) {
            if (status == BTREE_FINISHED) {
                status = BTREE_STOPPED;
            }
        }
    }
    return status;
}


static bool BTREE_SYM(node_seek_at_desc_mut)(BTREE_NODE *node, size_t index,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata, int *status)
{
    if (node->isleaf) {
        int i;
        if (index >= (size_t)node->len) {
            i = node->len-1;
        } else {
            i = (int)index;
        }
        for (; i >= 0; i--) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    bool found = false;
    int i = 0;
    for (; i < node->len; i++) {
        size_t count = BTREE_SYM(node_count)(node, i);
        if (index <= count) {
            found = index == count;
            break;
        }
        index -= count + 1;
    }
    if (!found) {
        if (!BTREE_SYM(cow)(&node->children[i], udata)) {
            *status = BTREE_NOMEM;
            return false;
        }
        if (!BTREE_SYM(node_seek_at_desc_mut)(node->children[i], index, iter, 
            udata, status))
        {
            return false;
        }
        i--;
    }
    for (; i >= 0; i--) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!node->isleaf) {
            if (!BTREE_SYM(cow)(&node->children[i], udata)) {
                *status = BTREE_NOMEM;
                return false;
            }
            if (!BTREE_SYM(node_scan_desc)(node->children[i], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static int BTREE_SYM(seek_at_desc_mut)(BTREE_NODE **root, size_t index, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(cow)(root, udata)) {
            return BTREE_NOMEM;
        }
        if (!BTREE_SYM(node_seek_at_desc_mut)(*root, index, iter, udata,
            &status))
        {
            if (status == BTREE_FINISHED) {
                status = BTREE_STOPPED;
            }
        }
    }
    return status;
}

static bool BTREE_SYM(node_seek_mut)(BTREE_NODE *node, BTREE_ITEM key,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata, int depth, 
    int *status) 
{
    int found;
    int i = BTREE_SYM(search)(node, key, udata, &found, depth);
    if (!found) {
        if (!node->isleaf) {
            if (!BTREE_SYM(cow)(&node->children[i], udata)) {
                *status = BTREE_NOMEM;
                return false;
            }
            if (!BTREE_SYM(node_seek_mut)(node->children[i], key, iter, udata,
                depth+1, status))
            {
                return false;
            }
        }
    }
    for (; i < node->len; i++) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!node->isleaf) {
            if (!BTREE_SYM(cow)(&node->children[i+1], udata)) {
                *status = BTREE_NOMEM;
                return false;
            }
            if (!BTREE_SYM(node_scan_mut)(node->children[i+1], iter, udata,
                status))
            {
                return false;
            }
        }
    }
    return true;
}

static int BTREE_SYM(seek_mut)(BTREE_NODE **root, BTREE_ITEM key, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(cow)(root, udata)) {
            return BTREE_NOMEM;
        }
        if (!BTREE_SYM(node_seek_mut)(*root, key, iter, udata, 0, &status)) {
            if (status == BTREE_FINISHED) {
                status = BTREE_STOPPED;
            }
        }
    }
    return status;
}

static bool BTREE_SYM(node_seek_desc)(BTREE_NODE *node, BTREE_ITEM key,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata, int depth) 
{
    int found;
    int i = BTREE_SYM(search)(node, key, udata, &found, depth);
    if (!found) {
        if (!node->isleaf) {
            if (!BTREE_SYM(node_seek_desc)(node->children[i], key, iter, udata,
                depth+1))
            {
                return false;
            }
        }
        if (i == 0) {
            return true;
        }
        i--;
    }
    while(1) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!node->isleaf) {
            if (!BTREE_SYM(node_scan_desc)(node->children[i], iter, udata)) {
                return false;
            }
        }
        if (i == 0) {
            break;
        }
        i--;
    }
    return true;
}

static int BTREE_SYM(seek_desc)(BTREE_NODE **root, BTREE_ITEM key, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(node_seek_desc)(*root, key, iter, udata, 0)) {
            status = BTREE_STOPPED;
        }
    }
    return status;
}

static bool BTREE_SYM(node_seek_desc_mut)(BTREE_NODE *node, BTREE_ITEM key,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata, int depth, 
    int *status) 
{
    int found;
    int i = BTREE_SYM(search)(node, key, udata, &found, depth);
    while(1) {
        if (found) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        if (!node->isleaf) {
            if (!BTREE_SYM(cow)(&node->children[i], udata)) {
                *status = BTREE_NOMEM;
                return false;
            }
            int ok;
            if (found) {
                ok = BTREE_SYM(node_scan_desc_mut)(node->children[i], iter, 
                    udata, status);
            } else {
                ok = BTREE_SYM(node_seek_desc_mut)(node->children[i], key, iter,
                    udata, depth+1, status);
            }
            if (!ok) {
                return false;
            }
        }
        if (i == 0) {
            return true;
        }
        i--;
        found = true;
    }
}

static int BTREE_SYM(seek_desc_mut)(BTREE_NODE **root, BTREE_ITEM key, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    int status = BTREE_FINISHED;
    if (*root) {
        if (!BTREE_SYM(cow)(root, udata)) {
            return BTREE_NOMEM;
        }
        if (!BTREE_SYM(node_seek_desc_mut)(*root, key, iter, udata, 0, &status)){
            if (status == BTREE_FINISHED) {
                status = BTREE_STOPPED;
            }
        }
    }
    return status;
}

static void BTREE_SYM(shift_right)(BTREE_NODE *node, int i, int n) {
    BTREE_ASSERT(!BTREE_SYM(shared)(node));
    n--;
    for (int j = node->len; j > i; j--) {
        node->items[j+n] = node->items[j-1];
    }
    node->len++;
    if (!node->isleaf) {
        for (int j = node->len; j > i; j--) {
            node->children[j+n] = node->children[j-1];
#ifdef BTREE_COUNTED
            node->counts[j+n] = node->counts[j-1];
#endif
        }
    }
}

static int BTREE_SYM(index_of)(BTREE_NODE **root, BTREE_ITEM key,
    size_t *index_out, void *udata)
{
#ifdef BTREE_NOORDER
    (void)root, (void)key, (void)index_out, (void)udata;
    return BTREE_UNSUPPORTED;
#else
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    int depth = 0;
    size_t index = 0;
    BTREE_NODE *node = *root;
    while (1) {
        int i, found;
        i = BTREE_SYM(search)(node, key, udata, &found, depth);
        index += (size_t)i;
        if (!node->isleaf) {
            for (int j = 0; j < i; j++) {
                index += BTREE_SYM(node_count)(node, j);
            }
            if (found) {
                index += BTREE_SYM(node_count)(node, i);
            }
        }
        if (found) {
            if (index_out) {
                *index_out = index;
            }
            return BTREE_FOUND;
        } else if (node->isleaf) {
            return BTREE_NOTFOUND;
        }
        node = node->children[i];
        depth++;
    }
#endif
}

// returns FOUND or NOTFOUND
static int BTREE_SYM(get)(BTREE_NODE **root, BTREE_ITEM key, BTREE_ITEM *item_out,
    void *udata)
{
#ifdef BTREE_NOORDER
    (void)root, (void)key, (void)item_out, (void)udata;
    return BTREE_UNSUPPORTED;
#else
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    BTREE_NODE *node = *root;
    int depth = 0;
    while (1) {
        int i, found;
        i = BTREE_SYM(search)(node, key, udata, &found, depth);
        if (found) {
            if (item_out) {
                *item_out = node->items[i];
            }
            return BTREE_FOUND;
        } else if (node->isleaf) {
            return BTREE_NOTFOUND;
        }
        node = node->children[i];
        depth++;
    }
#endif
}

static int BTREE_SYM(get_mut_ref)(BTREE_NODE **root, BTREE_ITEM key,
    BTREE_ITEM **item, void *udata)
{
#ifdef BTREE_NOORDER
    (void)root, (void)key, (void)item, (void)udata;
    return BTREE_UNSUPPORTED;
#else
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    int depth = 0;
    BTREE_NODE *node = *root;
    while (1) {
        BTREE_ASSERT(!BTREE_SYM(shared)(node));
        int i, found;
        i = BTREE_SYM(search)(node, key, udata, &found, depth);
        if (found) {
            if (item) {
                *item = &node->items[i];
            }
            return BTREE_FOUND;
        } else if (node->isleaf) {
            return BTREE_NOTFOUND;
        }
        if (!BTREE_SYM(cow)(&node->children[i], udata)) {
            return BTREE_NOMEM;
        }
        node = node->children[i];
        depth++;
    }
#endif
}


// Work like (get) but, if needed, performs copy-on-write operations.
// returns FOUND or NOTFOUND or NOMEM
static int BTREE_SYM(get_mut)(BTREE_NODE **root, BTREE_ITEM key, 
    BTREE_ITEM *item_out, void *udata)
{
    BTREE_ITEM *item;
    int ret = BTREE_SYM(get_mut_ref)(root, key, &item, udata);
    if (ret == BTREE_FOUND && item_out) {
        *item_out = *item;
    }
    return ret;
}

// returns true if key is found
static bool BTREE_SYM(contains)(BTREE_NODE **root, BTREE_ITEM key, void *udata) {
    return BTREE_SYM(get)(root, key, 0, udata) == BTREE_FOUND;
}

static BTREE_NODE *BTREE_SYM(split)(BTREE_NODE *left, BTREE_ITEM *mitem, 
    void *udata)
{
    (void)udata;
    BTREE_NODE *right = BTREE_SYM(alloc_node)(left->isleaf, udata);
    if (!right) {
        return 0;
    }
    int mid = BTREE_MAXITEMS / 2;
    *mitem = left->items[mid];
    right->height = left->height;
    right->len = left->len-mid-1;
    left->len = mid;
    for (int i = 0; i < right->len; i++) {
        right->items[i] = left->items[mid+1+i];
    }
    if (!left->isleaf) {
        for (int i = 0; i <= right->len; i++) {
            right->children[i] = left->children[mid+1+i];
        }
#ifdef BTREE_COUNTED
        for (int i = 0; i <= right->len; i++) {
            right->counts[i] = left->counts[mid+1+i];
        }
#endif
    }
    return right;
}

static bool BTREE_SYM(split_root)(BTREE_NODE **root, void *udata) {
    (void)udata;
    BTREE_ASSERT(!BTREE_SYM(shared)(*root));
    BTREE_NODE *newroot = BTREE_SYM(alloc_node)(0, udata);
    if (!newroot) {
        return false;
    }
    newroot->len = 1;
    newroot->height = (*root)->height+1;
    newroot->children[0] = *root;
    newroot->children[1] = BTREE_SYM(split)(*root, &newroot->items[0], udata);
    if (!newroot->children[1]) {
        BTREE_SYM(free)(newroot, BTREE_NODE_SIZE(newroot), udata);
        return false;
    }
#ifdef BTREE_COUNTED
    newroot->counts[0] = BTREE_SYM(count0)(newroot->children[0]);
    newroot->counts[1] = BTREE_SYM(count0)(newroot->children[1]);
#endif
    *root = newroot;
    return true;
}


static bool BTREE_SYM(split_child_at)(BTREE_NODE *node, int i, void *udata) {
    (void)udata;
    BTREE_ASSERT(!BTREE_SYM(shared)(node));
    BTREE_ITEM mitem;
    BTREE_NODE *right = BTREE_SYM(split)(node->children[i], &mitem, udata);
    if (!right) {
        return false;
    }
    BTREE_SYM(shift_right)(node, i, 1);
    node->items[i] = mitem;
    node->children[i+1] = right;
#ifdef BTREE_COUNTED
    node->counts[i] = BTREE_SYM(count0)(node->children[i]);
    node->counts[i+1] = BTREE_SYM(count0)(node->children[i+1]);
#endif
    return true;
}

static void BTREE_SYM(give_left)(BTREE_NODE *node, int index, bool balance) {
    // This will give items from right to left.
    // node->children[i]) to node->children[i-1].
    // It's expected that the children are leaves and that both are cow'd and
    // the right child index > 0.
    // These checks must be done already, prior to calling this function.

    BTREE_NODE *left = node->children[index-1];
    BTREE_NODE *right = node->children[index];

    BTREE_ASSERT(!BTREE_SYM(shared)(node));
    BTREE_ASSERT(!BTREE_SYM(shared)(left));
    BTREE_ASSERT(!BTREE_SYM(shared)(right));
    
    int n = balance ? (right->len-left->len)/2 : right->len-left->len;    
    left->items[left->len++] = node->items[index-1];
    int i = 0;
    for (; i < n-1; i++) {
        left->items[left->len++] = right->items[i];
        right->items[i] = right->items[n+i];
    }
    node->items[index-1] = right->items[i];
    right->len -= n;
    for (; i < right->len; i++) {
        right->items[i] = right->items[n+i];
    }
#ifdef BTREE_COUNTED
    node->counts[index-1] = left->len;
    node->counts[index] = right->len;
#endif

}

static void BTREE_SYM(give_right)(BTREE_NODE *node, int index, bool balance) {
    // This will give items from left to right. 
    // node->children[i]) to node->children[i+1].
    // It's expected that the children are leaves and that both are cow'd and
    // the left child index < node->len.
    // These checks must be done already, prior to calling this function.
    BTREE_NODE *left = node->children[index];
    BTREE_NODE *right = node->children[index+1];
    
    BTREE_ASSERT(!BTREE_SYM(shared)(node));
    BTREE_ASSERT(!BTREE_SYM(shared)(left));
    BTREE_ASSERT(!BTREE_SYM(shared)(right));
    
    int n = balance ? (left->len-right->len)/2 : left->len-right->len;
    int i = right->len+n-1;
    for (int j = right->len-1; j >= 0; j--) {
        right->items[i--] = right->items[j];
    }
    right->items[i--] = node->items[index];
    for (int j = left->len-1; j > left->len-n; j--) {
        right->items[i--] = left->items[j];
    }
    node->items[index] = left->items[left->len-n];
    left->len -= n;
    right->len += n;

#ifdef BTREE_COUNTED
    node->counts[index] = left->len;
    node->counts[index+1] = right->len;
#endif
}

#define BTREE_MUSTSPLIT 9999
#define BTREE_INSITEM      0
#define BTREE_INSAT        1
#define BTREE_REPAT        2
#define BTREE_PUSHFRONT    3
#define BTREE_PUSHBACK     4

static int BTREE_SYM(insert1)(BTREE_NODE *node, int act, size_t index, 
    BTREE_ITEM item, BTREE_ITEM *olditem, void *udata, int depth)
{
    BTREE_ASSERT(!BTREE_SYM(shared)(node));
    size_t oindex = 0;
    int found = 0;
    int i = 0;
retry:
    switch (act) {
    case BTREE_INSITEM:
        i = BTREE_SYM(search)(node, item, udata, &found, depth);
        break;
    case BTREE_INSAT: 
    case BTREE_REPAT:
        oindex = index;
        found = 0;
        if (node->isleaf) {
            if (index > (size_t)node->len || (index == (size_t)node->len &&
                act == BTREE_REPAT))
            {
                // return NOTFOUND when index is out of bounds
                return BTREE_NOTFOUND;
            }
            i = index;
            found = 1;
        } else {
            i = 0;
            for (; i < node->len; i++) {
                size_t count = BTREE_SYM(node_count)(node, i);
                if (index <= count) {
                    found = index == count;
                    break;
                }
                index -= count + 1;
            }
        }
#ifndef BTREE_NOORDER
        // Check order. 
        if (act == BTREE_REPAT && !node->isleaf && found) {
            // Get the previous and next items
            BTREE_NODE *child;
            child = node->children[i];
            while (1) {
                if (child->isleaf) {
                    if (!BTREE_SYM(less)(child->items[child->len-1], item, 
                        udata))
                    {
                        return BTREE_OUTOFORDER;
                    }
                    break;
                }
                child = child->children[child->len];
            }
            child = node->children[i+1];
            while (1) {
                if (child->isleaf) {
                    if (!BTREE_SYM(less)(item, child->items[0], udata)) {
                       return BTREE_OUTOFORDER;
                    }
                    break;
                }
                child = child->children[0];
            }
        } else {
            int i0 = i-1;
            int i1 = act == BTREE_REPAT && node->isleaf ? i+1 : i;
            if (i0 >= 0) {
                if (!BTREE_SYM(less)(node->items[i0], item, udata)) {
                return BTREE_OUTOFORDER;
                }
            }
            if (i1 < node->len) {
                if (!BTREE_SYM(less)(item, node->items[i1], udata)) {
                return BTREE_OUTOFORDER;
                }
            }
        }
#endif
        found = act == BTREE_INSAT ? 0 : found;
        break;
    case BTREE_PUSHFRONT:
        i = 0;
        found = 0;
        if (node->isleaf) {
#ifndef BTREE_NOORDER
            // check order
            if (!BTREE_SYM(less)(item, node->items[0], udata)) {
                return BTREE_OUTOFORDER;
            }
#endif
            goto isleaf;
        }
        goto isbranch;
    case BTREE_PUSHBACK:
        i = node->len;
        found = 0;
        if (node->isleaf) {
#ifndef BTREE_NOORDER
            // check order
            if (!BTREE_SYM(less)(node->items[node->len-1], item, udata)) {
                return BTREE_OUTOFORDER;
            }
#endif
            goto isleaf;
        }
        goto isbranch;
    }
    while (1) {
        BTREE_ASSERT(i >= 0 && i <= node->len);
        if (found) {
            if (olditem) {
                *olditem = node->items[i];
            }
            node->items[i] = item;
            return BTREE_REPLACED;
        }
        if (node->isleaf) {
        isleaf:
            if (node->len == BTREE_MAXITEMS) {
                return BTREE_MUSTSPLIT;
            }
            BTREE_SYM(shift_right)(node, i, 1);
            node->items[i] = item;
            return BTREE_INSERTED;
        }
    isbranch:
        if (!BTREE_SYM(cow)(&node->children[i], udata)) {
            return BTREE_NOMEM;
        }
        int ret = BTREE_SYM(insert1)(node->children[i], act, index, item,
            olditem, udata, depth+1);
        if (ret != BTREE_MUSTSPLIT || node->len == BTREE_MAXITEMS) {
            if (ret == BTREE_INSERTED) {
#ifdef BTREE_COUNTED
                node->counts[i]++;
#endif
            }
            return ret;
        }
        if (!BTREE_SYM(split_child_at)(node, i, udata)) {
            return BTREE_NOMEM;
        }
        if (act == BTREE_INSITEM) {
            int cmp = BTREE_SYM(compare)(item, node->items[i], udata);
            if (cmp <= 0) {
                found = cmp == 0;
            } else {
                i++;
            }
        } else {
            if (act == BTREE_INSAT) {
                // revert the index
                index = oindex;
            }
            goto retry;
        }
    }
}

static int BTREE_SYM(insert0)(BTREE_NODE **root, int act, size_t index, 
    BTREE_ITEM item, BTREE_ITEM *olditem, void *udata)
{
    if (!*root) {
        if (act == BTREE_REPAT || (act == BTREE_INSAT && index > 0)) {
            return BTREE_NOTFOUND;
        }
        *root = BTREE_SYM(alloc_node)(1, udata);
        if (!*root) {
            return BTREE_NOMEM;
        }
        (*root)->items[0] = item;
        (*root)->len = 1;
        (*root)->height = 1;
        return BTREE_INSERTED;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    while (1) {
        int ret = BTREE_SYM(insert1)(*root, act, index, item, olditem, udata, 0);
        if (ret != BTREE_MUSTSPLIT) {
            return ret;
        }
        if (!BTREE_SYM(split_root)(root, udata)) {
            return BTREE_NOMEM;
        }
    }
}

#ifndef BTREE_NOORDER
static int BTREE_SYM(insert_fastpath)(BTREE_NODE **root, BTREE_ITEM item,
    BTREE_ITEM *olditem, void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    int ret = 0;
    int cidx = 0;
    BTREE_NODE *parent = 0;
    BTREE_NODE *node = *root;
    int depth = 0;
#if defined(BTREE_COUNTED)
    short path[BTREE_MAXHEIGHT];
#endif
    while (1) {
        BTREE_ASSERT(!BTREE_SYM(shared)(node));
        int found;
        int i = BTREE_SYM(search)(node, item, udata, &found, depth);
        if (found) {
            if (olditem) {
                *olditem = node->items[i];
            }
            node->items[i] = item;
            ret = BTREE_REPLACED;
            break;
        }
        if (node->isleaf) {
            if (node->len == BTREE_MAXITEMS) {
                if (!parent || parent->len == BTREE_MAXITEMS) {
                    break;
                }
                // Use the standard splitting algorithm
#ifdef BTREE_COUNTED
                parent->counts[cidx]--;
#endif
                depth--;
                node = parent;
                i = cidx;
                if (i > 0 && node->children[i-1]->len < BTREE_MINITEMS+1) {
                    // In the case the the node to left has lots of room then
                    // just give a bunch of items to it rather than split.
                    if (!BTREE_SYM(cow)(&node->children[i-1], udata)) {
                        ret = BTREE_NOMEM;
                        break;
                    }
                    BTREE_SYM(give_left)(node, i, false);
                    continue;
                }
                if (!BTREE_SYM(split_child_at)(node, i, udata)) {
                    ret = BTREE_NOMEM;
                    break;
                }
                int cmp = BTREE_SYM(compare)(item, node->items[i], udata);
                i += cmp > 0;
            } else {
                BTREE_SYM(shift_right)(node, i, 1);
                node->items[i] = item;
                return BTREE_INSERTED;
            }
        }
#if defined(BTREE_COUNTED)
        path[depth] = i;
        node->counts[i]++;
#endif
        depth++;
        cidx = i;
        parent = node;
        if (!BTREE_SYM(cow)(&node->children[i], udata)) {
            ret = BTREE_NOMEM;
            break;
        }
        node = node->children[i];
    }
#if defined(BTREE_COUNTED)
    node = *root;
    for (int i = 0; i < depth; i++) {
        int j = path[i];
        node->counts[j]--;
        node = node->children[j];
    }
#endif
    return ret;
}
#endif

// returns INSERTED, REPLACED, or NOMEM
static int BTREE_SYM(insert)(BTREE_NODE **root, BTREE_ITEM item,
    BTREE_ITEM *olditem, void *udata)
{
#ifdef BTREE_NOORDER
    (void)root, (void)item, (void)olditem, (void)udata;
    return BTREE_UNSUPPORTED;
#else
    int ret = BTREE_SYM(insert_fastpath)(root, item, olditem, udata);
    if (ret) {
        return ret;
    }
    return BTREE_SYM(insert0)(root, BTREE_INSITEM, 0, item, olditem, udata);
#endif
}

static void BTREE_SYM(shift_left)(BTREE_NODE *node, int i, int n, bool for_merge){
    BTREE_ASSERT(!BTREE_SYM(shared)(node));
    n--;
    for (int j = i; j < node->len-1; j++) {
        node->items[j+n] = node->items[j+1];
    }
    if (!node->isleaf) {
        if (for_merge) {
            i++;
        }
        for (int j = i; j < node->len; j++) {
            node->children[j+n] = node->children[j+1];
        }
#ifdef BTREE_COUNTED
        for (int j = i; j < node->len; j++) {
            node->counts[j+n] = node->counts[j+1];
        }
#endif
    }
    node->len--;
}

static void BTREE_SYM(join)(BTREE_NODE *left, BTREE_NODE *right, void *udata) {
    (void)udata;
    BTREE_ASSERT(!BTREE_SYM(shared)(left));
    BTREE_ASSERT(!BTREE_SYM(shared)(right));
    for (int i = 0; i < right->len; i++) {
        left->items[left->len+i] = right->items[i];
    }
    if (!left->isleaf) {
        for (int i = 0; i <= right->len; i++) {
            left->children[left->len+i] = right->children[i];
        }
#ifdef BTREE_COUNTED
        for (int i = 0; i <= right->len; i++) {
            left->counts[left->len+i] = right->counts[i];
        }
#endif
    }
    left->len += right->len;
}

static void BTREE_SYM(rebalance)(BTREE_NODE *node, int i, void *udata) {
    BTREE_ASSERT(!BTREE_SYM(shared)(node));

    if (i == node->len) {
        i--;
    }

    BTREE_NODE *left = node->children[i];
    BTREE_NODE *right = node->children[i+1];

    BTREE_ASSERT(!BTREE_SYM(shared)(left));
    BTREE_ASSERT(!BTREE_SYM(shared)(right));

    if (left->len + right->len < BTREE_MAXITEMS) {
        // merge (left,item,right)

        // Merges the left and right children nodes together as a single node
        // that includes (left,item,right), and places the contents into the
        // existing left node. Delete the right node altogether and move the
        // following items and child nodes to the left by one slot.
        left->items[left->len] = node->items[i];
        left->len++;
        BTREE_SYM(join)(left, right, udata);
#ifdef BTREE_COUNTED
        size_t count = node->counts[i] + 1 + node->counts[i+1];
#endif
        BTREE_SYM(free)(right, BTREE_NODE_SIZE(right), udata);
        BTREE_SYM(shift_left)(node, i, 1, true);
#ifdef BTREE_COUNTED
        node->counts[i] = count;
#endif
        return;
    }
    if (left->isleaf) {
        // For leaves only.
        // Move as many items from one leaf to another to create balance.
        if (left->len < right->len) {
            BTREE_SYM(give_left)(node, i+1, true);
        } else {
            BTREE_SYM(give_right)(node, i, true);
        }
    } else {
        // For branches only.
        // Shift items and children over by one.
        if (left->len < right->len) {
            // move right to left
            left->items[left->len] = node->items[i];
            left->children[left->len+1] = right->children[0];
    #ifdef BTREE_COUNTED
            left->counts[left->len+1] = right->counts[0];
    #endif
            left->len++;
            node->items[i] = right->items[0];
            BTREE_SYM(shift_left)(right, 0, 1, false);
        } else {
            // move left to right
            BTREE_SYM(shift_right)(right, 0, 1);
            right->items[0] = node->items[i];
            right->children[0] = left->children[left->len];
    #ifdef BTREE_COUNTED
            right->counts[0] = left->counts[left->len];
    #endif
            node->items[i] = left->items[left->len-1];
            left->len--;
        }
    }
#ifdef BTREE_COUNTED
    node->counts[i] = BTREE_SYM(count0)(node->children[i]);
    node->counts[i+1] = BTREE_SYM(count0)(node->children[i+1]);
#endif
}

// deletion actions
#define BTREE_DELKEY   0
#define BTREE_POPMAX   1
#define BTREE_POPFRONT 2
#define BTREE_POPBACK  3
#define BTREE_DELAT    4

static int BTREE_SYM(delete1)(BTREE_NODE *node, int act, BTREE_ITEM key,
    size_t index, void *udata, int depth, BTREE_ITEM *prev)
{
    BTREE_ASSERT(!BTREE_SYM(shared)(node));
    int i = 0;
    int found = 0;
    switch (act) {
    case BTREE_DELKEY:
        i = BTREE_SYM(search)(node, key, udata, &found, depth);
        break;
    case BTREE_POPMAX:
        i = node->isleaf ? node->len-1 : node->len;
        found = 1;
        break;
    case BTREE_POPFRONT:
        i = 0;
        found = node->isleaf;
        break;
    case BTREE_POPBACK:
        i = node->isleaf ? node->len-1 : node->len;
        found = node->isleaf;
        break;
    case BTREE_DELAT:
        if (node->isleaf) {
            if (index < (size_t)node->len) {
                i = index;
                found = 1;
            }
        } else {
            for (; i < node->len; i++) {
                size_t count = BTREE_SYM(node_count)(node, i);
                if (index <= count) {
                    found = index == count;
                    break;
                }
                index -= count + 1;
            }
        }
        break;
    }
    if (node->isleaf) {
        if (found) {
            // Item was found in leaf, copy its contents and delete it.
            // This might cause the number of items to drop below min_items,
            // and it so, the caller will take care of the rebalancing.
            *prev = node->items[i];
            BTREE_SYM(shift_left)(node, i, 1, false);
            return BTREE_DELETED;
        }
        return BTREE_NOTFOUND;
    }

    // Cow the target child node.
    if (!BTREE_SYM(cow)(&node->children[i], udata)) {
        return BTREE_NOMEM;
    }
    if (node->children[i]->len == BTREE_MINITEMS) {
        // There's a good chance that this node will need to be rebalanced at
        // the end of this operation. Make sure that both child nodes that will
        // be used in the rebalancing are cow'd _before_ traversing further.
        if (i > 0 && i == node->len) {
            if (!BTREE_SYM(cow)(&node->children[i-1], udata)) {
                return BTREE_NOMEM;
            }
        } else {
            if (!BTREE_SYM(cow)(&node->children[i+1], udata)) {
                return BTREE_NOMEM;
            }
        }
    }
    if (found) {
        if (act == BTREE_POPMAX) {
            // Popping off the max item into into its parent branch to maintain
            // a balanced tree.
            act = BTREE_POPMAX;
        } else {
            // Item was found in branch, copy its contents, delete it, and 
            // begin popping off the max items in child nodes.
            *prev = node->items[i];
            prev = &node->items[i];
            act = BTREE_POPMAX;
        }
    }
    int ret = BTREE_SYM(delete1)(node->children[i], act, key, index, udata,
        depth+1, prev);
    if (ret != BTREE_DELETED) {
        return ret;
    }
#ifdef BTREE_COUNTED
    node->counts[i]--;
#endif
    if (node->children[i]->len < BTREE_MINITEMS) {
        BTREE_SYM(rebalance)(node, i, udata);
    }
    return BTREE_DELETED;
}

static int BTREE_SYM(delete0)(BTREE_NODE **root, int act, BTREE_ITEM key,
    size_t index, void *udata, BTREE_ITEM *olditem)
{
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    int ret = BTREE_SYM(delete1)(*root, act, key, index, udata, 0, olditem);
    if (ret != BTREE_DELETED) {
        return ret;
    }
    if ((*root)->len == 0) {
        BTREE_NODE *old_root = *root;
        *root = (*root)->isleaf ? 0 : (*root)->children[0];
        BTREE_SYM(free)(old_root, BTREE_NODE_SIZE(old_root), udata);
    }
    return BTREE_DELETED;
}

// Optimized fast-path.
//
// This performs a non-recursive search and delete of the item matching the
// provided key. This works in most cases because it's very likely that
// if the key is found then it can be removed without much rebalancing.
//
// For example, with a fanout of 16 there's a minimum of 7 items in a leaf.
// Thus, on average there'll be ~11 items per leaf. Leaving a one-in-four
// chance for a rebalance. When a rebalance is needed, there's a very good
// chance that the leaf can take from a sibling with only a single item
// shift to the parent, and no changes to the grandparent.
// 
// If the item is found in a branch node, then there's also a good chance
// that the branch be a height just above the leaf level. In that case the
// item can be deleted and it's space can be assigned to an item from a
// nearby leaf child, provided the leaf can spare it.
//
// In all other cases, the standard path will be used.
//
// In the case that optimized path fails due to not meeting the above 
// conditions, then there may be rollback operations, such as reverting
// COUNTS. As long as those are features of the tree.
#if !defined(BTREE_NOORDER)
static int BTREE_SYM(delete_fastpath)(BTREE_NODE **root, BTREE_ITEM key,
    BTREE_ITEM *olditem, void *udata)
{
    if (!*root || (*root)->isleaf) {
        // Continue using the standard path operation for small trees with no
        // child nodes.
        return 0;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    int ret = 0;
    int depth = 0;
    short path[BTREE_MAXHEIGHT];
    BTREE_NODE *parent = 0;
    BTREE_NODE *node = *root;
    while (1) {
        int found = 0;
        int i = BTREE_SYM(search)(node, key, udata, &found, depth);
        if (node->isleaf) {
            if (!found) {
                ret = BTREE_NOTFOUND;
                break;
            }
            bool take = false;
            bool merge = false;
            int from = 0;
            int ci = path[depth-1];
            if (node->len == BTREE_MINITEMS) {
                // Deleting will cause node to have too few items. 
                BTREE_ASSERT(parent);
                if (ci > 0 && parent->children[ci-1]->len > BTREE_MINITEMS) {
                    take = true;
                    from = -1;

                } else if (ci == 0 && parent->children[ci+1]->len > 
                    BTREE_MINITEMS)
                {
                    take = true;
                    from = +1;
                } else {
                    // Both left and right siblings are at min capacity
                    if (parent->len > BTREE_MINITEMS) {
                        // merge (take from zero)
                        take = true;
                        merge = true;
                        if (ci > 0) {
                            from = -1;
                        } else {
                            from = +1;
                        }
                    } else {
                        break;
                    }
                }
                if (take) {
                    if (!BTREE_SYM(cow)(&parent->children[ci+from], udata)) {
                        ret = BTREE_NOMEM;
                        break;
                    }
                }
            }
            if (olditem) {
                *olditem = node->items[i];
            }
            BTREE_SYM(shift_left)(node, i, 1, false);
            if (take) {
                if (!merge) {
                    if (from == -1) {
                        BTREE_SYM(give_right)(parent, ci-1, true);
                    } else {
                        BTREE_SYM(give_left)(parent, ci+1, true);
                    }
                } else {
                    i = ci;
                    if (from == -1) {
                        i--;
                    }
                    BTREE_NODE *left = parent->children[i];
                    BTREE_NODE *right = parent->children[i+1];
                    left->items[left->len] = parent->items[i];
                    left->len++;
                    BTREE_SYM(join)(left, right, udata);
            #ifdef BTREE_COUNTED
                    size_t count = parent->counts[i] + 1 + parent->counts[i+1];
            #endif
                    BTREE_SYM(free)(right, BTREE_NODE_SIZE(right), udata);
                    BTREE_SYM(shift_left)(parent, i, 1, true);
            #ifdef BTREE_COUNTED
                    parent->counts[i] = count;
            #endif
                }
            }
            return BTREE_DELETED;
        }
        // branch
        if (found) {
            if (node->height == 2) {
                if (node->children[i]->len > BTREE_MINITEMS) {
                    if (!BTREE_SYM(cow)(&node->children[i], udata)) {
                        ret = BTREE_NOMEM;
                        break;
                    }
                    BTREE_NODE *child = node->children[i];
                    if (olditem) {
                        *olditem = node->items[i];
                    }
                    node->items[i] = child->items[child->len-1];
                    child->len--;
            #ifdef BTREE_COUNTED
                    node->counts[i]--;
            #endif
                } else if (node->children[i+1]->len > BTREE_MINITEMS) {
                    if (!BTREE_SYM(cow)(&node->children[i+1], udata)) {
                        ret = BTREE_NOMEM;
                        break;
                    }
                    BTREE_NODE *child = node->children[i+1];
                    if (olditem) {
                        *olditem = node->items[i];
                    }
                    node->items[i] = child->items[0];
                    child->len--;
                    for (int j = 0; j < child->len; j++) {
                        child->items[j] = child->items[j+1];
                    }
            #ifdef BTREE_COUNTED
                    node->counts[i+1]--;
            #endif
                } else {
                    break;
                }
                return BTREE_DELETED;
            } else {
                break;
            }
        }
        if (!BTREE_SYM(cow)(&node->children[i], udata)) {
            ret = BTREE_NOMEM;
            break;
        }
        path[depth++] = i;
#ifdef BTREE_COUNTED
        node->counts[i]--;
#endif
        parent = node;
        node = node->children[i];
    }
#ifdef BTREE_COUNTED
    node = *root;
    for (int i = 0; i < depth; i++) {
        int j = path[i];
        node->counts[j]++;
        node = node->children[j];
    }
#endif
    return ret;
}
#endif

// returns DELETED, NOTFOUND, or NOMEM
static int BTREE_SYM(delete)(BTREE_NODE **root, BTREE_ITEM key, BTREE_ITEM *olditem, 
    void *udata)
{
#ifdef BTREE_NOORDER
    (void)root, (void)key, (void)olditem, (void)udata;
    return BTREE_UNSUPPORTED;
#else
    int ret;
    ret = BTREE_SYM(delete_fastpath)(root, key, olditem, udata);
    if (ret) {
        return ret;
    }
    BTREE_ITEM spare;
    ret = BTREE_SYM(delete0)(root, BTREE_DELKEY, key, 0, udata, &spare);
    if (ret != BTREE_DELETED) {
        return ret;
    }
    if (olditem) {
        *olditem = spare;
    }
    return BTREE_DELETED;
#endif
}


// returns FOUND or NOTFOUND
static int BTREE_SYM(front)(BTREE_NODE **root, BTREE_ITEM *item_out, void *udata) {
    (void)udata;
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    BTREE_NODE *node = *root;
    while (1) {
        if (node->isleaf) {
            if (item_out) {
                *item_out = node->items[0];
            }
            return BTREE_FOUND;
        } 
        node = node->children[0];
    }
}

static int BTREE_SYM(front_mut)(BTREE_NODE **root, BTREE_ITEM *item_out,
    void *udata)
{
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    BTREE_NODE *node = *root;
    while (1) {
        BTREE_ASSERT(!BTREE_SYM(shared)(node));
        if (node->isleaf) {
            if (item_out) {
                *item_out = node->items[0];
            }
            return BTREE_FOUND;
        } 
        if (!BTREE_SYM(cow)(&node->children[0], udata)) {
            return BTREE_NOMEM;
        }
        node = node->children[0];
    }
}

// returns FOUND or NOTFOUND
static int BTREE_SYM(back)(BTREE_NODE **root, BTREE_ITEM *item_out, void *udata) {
    (void)udata;
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    BTREE_NODE *node = *root;
    while (1) {
        if (node->isleaf) {
            if (item_out) {
                *item_out = node->items[node->len-1];
            }
            return BTREE_FOUND;
        } 
        node = node->children[node->len];
    }
}

// returns FOUND or NOTFOUND or NOMEM
static int BTREE_SYM(back_mut)(BTREE_NODE **root, BTREE_ITEM *item_out,
    void *udata)
{
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    BTREE_NODE *node = *root;
    while (1) {
        BTREE_ASSERT(!BTREE_SYM(shared)(node));
        if (node->isleaf) {
            if (item_out) {
                *item_out = node->items[node->len-1];
            }
            return BTREE_FOUND;
        } 
        if (!BTREE_SYM(cow)(&node->children[node->len], udata)) {
            return BTREE_NOMEM;
        }
        node = node->children[node->len];
    }
}

// returns FOUND or NOTFOUND
static int BTREE_SYM(get_at)(BTREE_NODE **root, size_t index, BTREE_ITEM *item,
    void *udata)
{
    (void)udata;
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    BTREE_NODE *node = *root;
    while (1) {
        if (node->isleaf) {
            if (index >= (size_t)node->len) {
                return BTREE_NOTFOUND;
            }
            if (item) {
                *item = node->items[index];
            }
            return BTREE_FOUND;
        }
        int i = 0;
        for (; i < node->len; i++) {
            size_t count = BTREE_SYM(node_count)(node, i);
            if (index < count) {
                break;
            }
            if (index == count) {
                if (item) {
                    *item = node->items[i];
                }
                return BTREE_FOUND;
            }
            index -= count + 1;
        }
        node = node->children[i];
    }
}

static int BTREE_SYM(get_at_mut)(BTREE_NODE **root, size_t index, BTREE_ITEM *item,
    void *udata)
{
    if (!*root) {
        return BTREE_NOTFOUND;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    BTREE_NODE *node = *root;
    while (1) {
        BTREE_ASSERT(!BTREE_SYM(shared)(node));
        if (node->isleaf) {
            if (index >= (size_t)node->len) {
                return BTREE_NOTFOUND;
            }
            if (item) {
                *item = node->items[index];
            }
            return BTREE_FOUND;
        }
        int i = 0;
        for (; i < node->len; i++) {
            size_t count = BTREE_SYM(node_count)(node, i);
            if (index < count) {
                break;
            }
            if (index == count) {
                if (item) {
                    *item = node->items[i];
                }
                return BTREE_FOUND;
            }
            index -= count + 1;
        }
        if (!BTREE_SYM(cow)(&node->children[i], udata)) {
            return BTREE_NOMEM;
        }
        node = node->children[i];
    }
}

static int BTREE_SYM(delete_at)(BTREE_NODE **root, size_t index, 
    BTREE_ITEM *olditem, void *udata)
{
    BTREE_ITEM spare = { 0 };
    int ret = BTREE_SYM(delete0)(root, BTREE_DELAT, spare, index, udata, &spare);
    if (ret != BTREE_DELETED) {
        return ret;
    }
    if (olditem) {
        *olditem = spare;
    }
    return BTREE_DELETED;
}

static int BTREE_SYM(replace_at)(BTREE_NODE **root, size_t index, BTREE_ITEM item,
    BTREE_ITEM *olditem, void *udata)
{
    return BTREE_SYM(insert0)(root, BTREE_REPAT, index, item, olditem, udata);
}

static int BTREE_SYM(pop_front_fastpath)(BTREE_NODE **root, BTREE_ITEM *olditem,
    void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    BTREE_NODE *node = *root;
    BTREE_NODE *parent = 0;
#ifdef BTREE_COUNTED
    int depth = 0;
#endif
    int ret = 0;
    while (1) {
        BTREE_ASSERT(!BTREE_SYM(shared)(node));
        if (node->isleaf) {
            if (node->len <= BTREE_MINITEMS) {
                if (parent && parent->children[1]->len > BTREE_MINITEMS+1) {
                    if (!BTREE_SYM(cow)(&parent->children[1], udata)) {
                        ret = BTREE_NOMEM;
                        break;
                    }
                    BTREE_SYM(give_left)(parent, 1, true);
#ifdef BTREE_COUNTED
                    parent->counts[0]--;
#endif
                } else {
                    break;
                }
            }
            if (olditem) {
                *olditem = node->items[0];
            }
            for (int i = 1; i < node->len; i++) {
                node->items[i-1] = node->items[i];
            }
            node->len--;
            return BTREE_DELETED;
        }
#ifdef BTREE_COUNTED
        node->counts[0]--;
        depth++;
#endif
        parent = node;
        if (!BTREE_SYM(cow)(&node->children[0], udata)) {
            ret = BTREE_NOMEM;
            break;
        }
        node = node->children[0];
    }
#ifdef BTREE_COUNTED
    node = *root;
    for (int i = 0; i < depth; i++) {
        node->counts[0]++;
        node = node->children[0];
    }
#endif
    return ret;
}

static int BTREE_SYM(pop_front)(BTREE_NODE **root, BTREE_ITEM *olditem,
    void *udata)
{
    int ret;
    ret = BTREE_SYM(pop_front_fastpath)(root, olditem, udata);
    if (ret) {
        return ret;
    }
    BTREE_ITEM spare = { 0 };
    ret = BTREE_SYM(delete0)(root, BTREE_POPFRONT, spare, 0, udata, 
        &spare);
    if (ret == BTREE_DELETED) {
        if (olditem) {
            *olditem = spare;
        }
    }
    return ret;
}

static int BTREE_SYM(pop_back_fastpath)(BTREE_NODE **root, BTREE_ITEM *olditem,
    void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    BTREE_NODE *parent = 0;
    BTREE_NODE *node = *root;
#ifdef BTREE_COUNTED
    int depth = 0;
#endif
    int ret = 0;
    while (1) {
        BTREE_ASSERT(!BTREE_SYM(shared)(node));
        if (node->isleaf) {
            if (node->len <= BTREE_MINITEMS) {
                if (parent && 
                    parent->children[parent->len-1]->len > BTREE_MINITEMS+1)
                {
                    if (!BTREE_SYM(cow)(&parent->children[parent->len-1],
                        udata))
                    {
                        ret = BTREE_NOMEM;
                        break;
                    }
                    BTREE_SYM(give_right)(parent, parent->len-1, true);
#ifdef BTREE_COUNTED
                    parent->counts[parent->len]--;
#endif
                } else {
                    break;
                }
            }
            if (olditem) {
                *olditem = node->items[node->len-1];
            }
            node->len--;
            return BTREE_DELETED;
        }
#ifdef BTREE_COUNTED
        node->counts[node->len]--;
        depth++;
#endif
        parent = node;
        if (!BTREE_SYM(cow)(&node->children[node->len], udata)) {
            ret = BTREE_NOMEM;
            break;
        }
        node = node->children[node->len];
    }
#ifdef BTREE_COUNTED
    node = *root;
    for (int i = 0; i < depth; i++) {
        node->counts[node->len]++;
        node = node->children[node->len];
    }
#endif
    return ret;
}

static int BTREE_SYM(pop_back)(BTREE_NODE **root, BTREE_ITEM *olditem, void *udata)
{
    int ret;
    ret = BTREE_SYM(pop_back_fastpath)(root, olditem, udata);
    if (ret) {
        return ret;
    }
    BTREE_ITEM spare = { 0 };
    ret = BTREE_SYM(delete0)(root, BTREE_POPBACK, spare, 0, udata,
        &spare);
    if (ret == BTREE_DELETED) {
        if (olditem) {
            *olditem = spare;
        }
    }
    return ret;
}

static int BTREE_SYM(push_front_fastpath)(BTREE_NODE **root, BTREE_ITEM item,
    void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    BTREE_NODE *node = *root;
#ifdef BTREE_COUNTED
    int depth = 0;
#endif
    int ret = 0;
    BTREE_NODE *parent = 0;
    while (1) {
        BTREE_ASSERT(!BTREE_SYM(shared)(node));
        if (node->isleaf) {
            if (node->len == BTREE_MAXITEMS) {
                if (!parent || parent->len == BTREE_MAXITEMS) {
                    break;
                }
                if (parent->children[1]->len < BTREE_MAXITEMS) {
                    // Instead of splitting just give the leaf to the right
                    // as many items as possible.
                    if (!BTREE_SYM(cow)(&parent->children[1], udata)) {
                        ret = BTREE_NOMEM;
                        break;
                    }
                    BTREE_SYM(give_right)(parent, 0, false);
                } else {
                    // Use the standard splitting algorithm
                    if (!BTREE_SYM(split_child_at)(parent, 0, udata)) {
                        ret = BTREE_NOMEM;
                        break;
                    }
                }
#ifdef BTREE_COUNTED
                parent->counts[0]++;
#endif
                node = parent->children[0];
            }
#ifndef BTREE_NOORDER
            if (!BTREE_SYM(less)(item, node->items[0], udata)) {
                ret = BTREE_OUTOFORDER;
                break;
            }
#endif
            BTREE_SYM(shift_right)(node, 0, 1);
            node->items[0] = item;
            return BTREE_INSERTED;
        }
#ifdef BTREE_COUNTED
        node->counts[0]++;
        depth++;
#endif
        parent = node;
        if (!BTREE_SYM(cow)(&node->children[0], udata)) {
            ret = BTREE_NOMEM;
            break;
        }
        node = node->children[0];
    }
#ifdef BTREE_COUNTED
    node = *root;
    for (int i = 0; i < depth; i++) {
        node->counts[0]--;
        node = node->children[0];
    }
#endif
    return ret;
}

static int BTREE_SYM(push_front)(BTREE_NODE **root, BTREE_ITEM item, void *udata) {
    int ret = BTREE_SYM(push_front_fastpath)(root, item, udata);
    if (ret) {
        return ret;
    }
    return BTREE_SYM(insert0)(root, BTREE_PUSHFRONT, 0, item, 0, udata);
}

static int BTREE_SYM(push_back_fastpath)(BTREE_NODE **root, BTREE_ITEM item,
    void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BTREE_SYM(cow)(root, udata)) {
        return BTREE_NOMEM;
    }
    BTREE_NODE *node = *root;
#ifdef BTREE_COUNTED
    int depth = 0;
#endif
    int ret = 0;
    BTREE_NODE *parent = 0;
    while (1) {
        BTREE_ASSERT(!BTREE_SYM(shared)(node));
        if (node->isleaf) {
            if (node->len == BTREE_MAXITEMS) {
                if (!parent || parent->len == BTREE_MAXITEMS) {
                    break;
                }
                if (parent->children[parent->len-1]->len < BTREE_MAXITEMS) {
                    if (!BTREE_SYM(cow)(&parent->children[parent->len-1],
                        udata))
                    {
                        ret = BTREE_NOMEM;
                        break;
                    }
                    // Instead of splitting just give the leaf to the left
                    // as many items as possible.
                    BTREE_SYM(give_left)(parent, parent->len, false);
                } else {
                    // Use the standard splitting algorithm
                    if (!BTREE_SYM(split_child_at)(parent, parent->len, 
                        udata))
                    {
                        ret = BTREE_NOMEM;
                        break;
                    }
                }
#ifdef BTREE_COUNTED
                parent->counts[parent->len]++;
#endif
                node = parent->children[parent->len];
            }
#ifndef BTREE_NOORDER
            if (!BTREE_SYM(less)(node->items[node->len-1], item, udata)) {
                ret = BTREE_OUTOFORDER;
                break;
            }
#endif
            node->items[node->len++] = item;
            return BTREE_INSERTED;
        }
#ifdef BTREE_COUNTED
        node->counts[node->len]++;
        depth++;
#endif
        parent = node;
        if (!BTREE_SYM(cow)(&node->children[node->len], udata)) {
            ret = BTREE_NOMEM;
            break;
        }
        node = node->children[node->len];
    }
#ifdef BTREE_COUNTED
    node = *root;
    for (int i = 0; i < depth; i++) {
        node->counts[node->len]--;
        node = node->children[node->len];
    }
#endif
    return ret;
}

static int BTREE_SYM(push_back)(BTREE_NODE **root, BTREE_ITEM item, void *udata) {
    int ret = BTREE_SYM(push_back_fastpath)(root, item, udata);
    if (ret) {
        return ret;
    }
    return BTREE_SYM(insert0)(root, BTREE_PUSHBACK, 0, item, 0, udata);
}

// Returns OUTOFORDER: The item cannot be inserted at index due to the item not
// being in order relative to the items at indexes to the right and left.
// Returns NOMEM: System is out of memory.
// Returns NOTFOUND: The item cannot be inserted because the index is out of 
// bounds, thus the index was > tree count.
static int BTREE_SYM(insert_at)(BTREE_NODE **root, size_t index, BTREE_ITEM item,
    void *udata)
{
    return BTREE_SYM(insert0)(root, BTREE_INSAT, index, item, 0, udata);
}

static int BTREE_SYM(copy)(BTREE_NODE **root, BTREE_NODE **newroot, void *udata) {
    if (!*root) {
        if (newroot) {
            *newroot = 0;
        }
        return BTREE_COPIED;
    }
    BTREE_NODE *node2 = BTREE_SYM(node_copy)(*root, true, udata);
    if (!node2) {
        return BTREE_NOMEM;
    }
    if (newroot) {
        *newroot = node2;
    }
    return BTREE_COPIED;
}

static int BTREE_SYM(clone)(BTREE_NODE **root, BTREE_NODE **newroot, void *udata) {
#ifndef BTREE_COW
    return BTREE_SYM(copy)(root, newroot, udata);
#else
    (void)udata;
    if (newroot) {
        *newroot = *root;
    }
    if (*root) {
        BTREE_SYM(rc_retain)(&(*root)->rc);
    }
#endif
    return BTREE_COPIED;
}

// stack node used by an iterator
BTREE_SNODE {
    BTREE_NODE *node; // the node
    int index;       // index of current item in node, used by iter_item()
};

#define BTREE_SCAN       0
#define BTREE_SCANDESC   1

BTREE_ITER {
    BTREE_NODE **root;         // root node
    void *udata;              // user data
    int kind;                 // kind of iterator
    bool mut;                 // this is a mutable iterator
    bool valid;               // iterator is valid
    short status;             // last status code. Zero for no errors
    union {
        struct  {
            short nstack; // number of path nodes (depth)
            BTREE_SNODE stack[BTREE_MAXHEIGHT]; // traversed path nodes
        } s;
    } u;
};

static void BTREE_SYM(iter_init)(BTREE_NODE **root, BTREE_ITER **iter, void *udata)
{
    *iter = BTREE_SYM(malloc)(sizeof(BTREE_ITER), udata);
    if (*iter) {
        (*iter)->root = root;
        (*iter)->udata = udata;
        (*iter)->mut = false;
        (*iter)->valid = false;
        (*iter)->kind = 0;
    }
}

static void BTREE_SYM(iter_init_mut)(BTREE_NODE **root, BTREE_ITER **iter, 
    void *udata)
{
    BTREE_SYM(iter_init)(root, iter, udata);
    if (*iter) {
        (*iter)->mut = 1;
    }
}

static void BTREE_SYM(iter_reset)(BTREE_ITER *iter, int kind) {
    iter->valid = true;
    iter->status = 0;
    iter->u.s.nstack = 0;
    iter->kind = kind;
}

static void BTREE_SYM(iter_release)(BTREE_ITER *iter) {
    if (iter) {
        BTREE_SYM(free)(iter, sizeof(BTREE_ITER), iter->udata);
    }
}

static bool BTREE_SYM(iter_valid)(BTREE_ITER *iter) {
    return iter && iter->valid;
}

static int BTREE_SYM(iter_status)(BTREE_ITER *iter) {
    return !iter ? BTREE_NOMEM : iter->status;
}

BTREE_NOINLINE
static void BTREE_SYM(iter_next_asc)(BTREE_ITER *iter) {
    BTREE_SNODE *snode = &iter->u.s.stack[iter->u.s.nstack-1];
    while (1) {
        snode = &iter->u.s.stack[iter->u.s.nstack-1];
        snode->index++;
        if (snode->node->isleaf && snode->index < snode->node->len) {
        next_item:
            // Iterator now points to the next item
            return;
        }
        if (snode->node->isleaf || snode->index == snode->node->len+1) {
            // pop the stack
            while (iter->u.s.nstack > 1) {
                iter->u.s.nstack--;
                snode = &iter->u.s.stack[iter->u.s.nstack-1];
                if (snode->index < snode->node->len) {
                    goto next_item;
                }
            }
            // end of iterator
            iter->valid = false;
            return;
        }
        if (iter->mut && !BTREE_SYM(cow)(&snode->node->children[snode->index], 
            iter->udata))
        {
            iter->status = BTREE_NOMEM;
            iter->valid = false;
            return;
        }
        iter->u.s.stack[iter->u.s.nstack++] = (BTREE_SNODE){ 
            snode->node->children[snode->index], -1 };
    }
}

// Move iterator cursor to the previous item.
BTREE_NOINLINE
static void BTREE_SYM(iter_next_desc)(BTREE_ITER *iter) {
    BTREE_SNODE *snode;
    while (1) {
        snode = &iter->u.s.stack[iter->u.s.nstack-1];
        snode->index--;
        if (snode->node->isleaf && snode->index > -1) {
            // Iterator now points to the next item
            return;
        }
        if (snode->node->isleaf) {
            // pop stack
            while (iter->u.s.nstack > 1) {
                iter->u.s.nstack--;
                snode = &iter->u.s.stack[iter->u.s.nstack-1];
                snode->index--;
                if (snode->index > -1) {
                    // Iterator now points to the next item
                    return;
                }
            }
            // end of iterator
            iter->valid = false;
            return;
        }
        snode->index++;
        if (iter->mut && !BTREE_SYM(cow)(&snode->node->children[snode->index], 
            iter->udata))
        {
            iter->status = BTREE_NOMEM;
            iter->valid = false;
            return;
        }
        BTREE_NODE *node = snode->node->children[snode->index];
        iter->u.s.stack[iter->u.s.nstack++] = (BTREE_SNODE){ node, node->len };
        snode = &iter->u.s.stack[iter->u.s.nstack-1];
    }
}

// Move iterator cursor to the next item.
// REQUIRED: iter_valid()
BTREE_INLINE
static void BTREE_SYM(iter_next)(BTREE_ITER *iter) {
    BTREE_ASSERT(BTREE_SYM(iter_valid)(iter));
    if (iter->kind == BTREE_SCAN) {
        // Fastpath for forward scanning iterators iter_seek and iter_scan,
        // where the next item is in a leaf. Fallback to the function call.
        BTREE_SNODE *snode = &iter->u.s.stack[iter->u.s.nstack-1];
        if (snode->node->isleaf && snode->index+1 < snode->node->len) {
            snode->index++;
        } else {
            BTREE_SYM(iter_next_asc)(iter);
        }
    } else if (iter->kind == BTREE_SCANDESC) {
        BTREE_SYM(iter_next_desc)(iter);
    }
}

// Moves iterator to first item and resets the status
static void BTREE_SYM(iter_scan)(BTREE_ITER *iter) {
    if (!iter) {
        return;
    }
    BTREE_SYM(iter_reset)(iter, BTREE_SCAN);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BTREE_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BTREE_NOMEM;
        iter->valid = false;
        return;
    }
    BTREE_NODE *node = *iter->root;
    while (1) {
        iter->u.s.stack[iter->u.s.nstack++] = (BTREE_SNODE){ node, 0 };
        if (node->isleaf) {
            return;
        }
        if (iter->mut && !BTREE_SYM(cow)(&node->children[0], iter->udata)) {
            iter->status = BTREE_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[0];
    }
}

// Moves iterator to last item and resets the status
static void BTREE_SYM(iter_scan_desc)(BTREE_ITER *iter) {
    if (!iter) {
        return;
    }
    BTREE_SYM(iter_reset)(iter, BTREE_SCANDESC);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BTREE_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BTREE_NOMEM;
        iter->valid = false;
        return;
    }
    BTREE_NODE *node = *iter->root;
    while (1) {
        iter->u.s.stack[iter->u.s.nstack++] = (BTREE_SNODE){ node, node->len };
        if (node->isleaf) {
            iter->u.s.stack[iter->u.s.nstack-1].index--;
            return;
        }
        if (iter->mut && !BTREE_SYM(cow)(&node->children[node->len],
            iter->udata))
        {
            iter->status = BTREE_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[node->len];
    }
}

static void BTREE_SYM(iter_seek)(BTREE_ITER *iter, BTREE_ITEM key) {
    if (!iter) {
        return;
    }
#ifdef BTREE_NOORDER
    (void)iter, (void)key;
    iter->valid = false;
    iter->status = BTREE_UNSUPPORTED;
#else
    BTREE_SYM(iter_reset)(iter, BTREE_SCAN);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BTREE_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BTREE_NOMEM;
        iter->valid = false;
        return;
    }
    int depth = 0;
    BTREE_NODE *node = *iter->root;
    while (1) {
        int found;
        int i = BTREE_SYM(search)(node, key, iter->udata, &found, depth);
        iter->u.s.stack[iter->u.s.nstack++] = (BTREE_SNODE){ node, i };
        if (found) {
            return;
        }
        if (node->isleaf) {
            iter->u.s.stack[iter->u.s.nstack-1].index--;
            BTREE_SYM(iter_next)(iter);
            return;
        }
        if (iter->mut && !BTREE_SYM(cow)(&node->children[i], iter->udata)) {
            iter->status = BTREE_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[i];
        depth++;
    }
#endif
}

static void BTREE_SYM(iter_seek_at)(BTREE_ITER *iter, size_t index) {
    if (!iter) {
        return;
    }
    BTREE_SYM(iter_reset)(iter, BTREE_SCAN);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BTREE_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BTREE_NOMEM;
        iter->valid = false;
        return;
    }
    BTREE_NODE *node = *iter->root;
    while (1) {
        iter->u.s.stack[iter->u.s.nstack++] = (BTREE_SNODE){ node, 0 };
        if (node->isleaf) {
            if (index >= (size_t)node->len) {
                iter->u.s.stack[iter->u.s.nstack-1].index = node->len;
            } else {
                iter->u.s.stack[iter->u.s.nstack-1].index = index;
            }
            iter->u.s.stack[iter->u.s.nstack-1].index--;
            BTREE_SYM(iter_next)(iter);
            return;
        }
        int i = 0;
        bool found = false;
        for (; i < node->len; i++) {
            size_t count = BTREE_SYM(node_count)(node, i);
            if (index <= count) {
                found = index == count;
                break;
            }
            index -= count + 1;
        }
        iter->u.s.stack[iter->u.s.nstack-1].index = i;
        if (found) {
            return;
        }
        if (iter->mut && !BTREE_SYM(cow)(&node->children[i], iter->udata)) {
            iter->status = BTREE_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[i];
    }
}

static void BTREE_SYM(iter_seek_at_desc)(BTREE_ITER *iter, size_t index) {
    if (!iter) {
        return;
    }
    BTREE_SYM(iter_reset)(iter, BTREE_SCANDESC);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BTREE_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BTREE_NOMEM;
        iter->valid = false;
        return;
    }
    BTREE_NODE *node = *iter->root;
    while (1) {
        iter->u.s.stack[iter->u.s.nstack++] = (BTREE_SNODE){ node, 0 };
        if (node->isleaf) {
            if (index >= (size_t)node->len) {
                iter->u.s.stack[iter->u.s.nstack-1].index = node->len-1;
            } else {
                iter->u.s.stack[iter->u.s.nstack-1].index = index;
            }
            return;
        }
        int i = 0;
        bool found = false;
        for (; i < node->len; i++) {
            size_t count = BTREE_SYM(node_count)(node, i);
            if (index <= count) {
                found = index == count;
                break;
            }
            index -= count + 1;
        }
        iter->u.s.stack[iter->u.s.nstack-1].index = i;
        if (found) {
            return;
        }
        if (iter->mut && !BTREE_SYM(cow)(&node->children[i], iter->udata)) {
            iter->status = BTREE_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[i];
    }
}

// Get the current iterator item.
// REQUIRES: iter_valid() and item != NULL
static void BTREE_SYM(iter_item)(BTREE_ITER *iter, BTREE_ITEM *item) {
    BTREE_SNODE *snode = &iter->u.s.stack[iter->u.s.nstack-1];
    *item = snode->node->items[snode->index];
}

static void BTREE_SYM(iter_seek_desc)(BTREE_ITER *iter, BTREE_ITEM key) {
    if (!iter) {
        return;
    }
#ifdef BTREE_NOORDER
    (void)iter, (void)key;
    iter->valid = false;
    iter->status = BTREE_UNSUPPORTED;
#else
    BTREE_SYM(iter_seek)(iter, key);
    if (!BTREE_SYM(iter_valid)(iter)) {
        if (BTREE_SYM(iter_status)(iter) == 0) {
            BTREE_SYM(iter_scan_desc)(iter);
        }
    } else {
        BTREE_ITEM item;
        BTREE_SYM(iter_item)(iter, &item);
        if (BTREE_SYM(compare)(item, key, iter->udata) > 0) {
            BTREE_SYM(iter_next_desc)(iter);
        }
    }
    iter->kind = BTREE_SCANDESC;
#endif
}

static inline void BTREE_SYM(all_sym_calls)(void) {
    // All internal symbols
    (void)BTREE_SYM(all_sym_calls);
    (void)BTREE_SYM(feat_maxitems);
    (void)BTREE_SYM(feat_minitems);
    (void)BTREE_SYM(feat_maxheight);
    (void)BTREE_SYM(feat_fanout);
    (void)BTREE_SYM(feat_counted);
    (void)BTREE_SYM(feat_ordered);
    (void)BTREE_SYM(feat_bsearch);
    (void)BTREE_SYM(feat_pathhint);
    (void)BTREE_SYM(feat_cow);
    (void)BTREE_SYM(feat_atomics);
    (void)BTREE_SYM(print);
    (void)BTREE_SYM(get_mut);
    (void)BTREE_SYM(get_mut_ref);
    (void)BTREE_SYM(get_at_mut);
    (void)BTREE_SYM(insert);
    (void)BTREE_SYM(get);
    (void)BTREE_SYM(index_of);
    (void)BTREE_SYM(contains);
    (void)BTREE_SYM(delete);
    (void)BTREE_SYM(get_at);
    (void)BTREE_SYM(insert_at);
    (void)BTREE_SYM(delete_at);
    (void)BTREE_SYM(replace_at);
    (void)BTREE_SYM(count);
    (void)BTREE_SYM(height);
    (void)BTREE_SYM(clear);
    (void)BTREE_SYM(sane);
    (void)BTREE_SYM(front);
    (void)BTREE_SYM(front_mut);
    (void)BTREE_SYM(back);
    (void)BTREE_SYM(back_mut);
    (void)BTREE_SYM(pop_front);
    (void)BTREE_SYM(pop_back);
    (void)BTREE_SYM(push_front);
    (void)BTREE_SYM(push_back);
    (void)BTREE_SYM(copy);
    (void)BTREE_SYM(clone);
    (void)BTREE_SYM(compare);
    (void)BTREE_SYM(less);
    (void)BTREE_SYM(iter_init);
    (void)BTREE_SYM(iter_init_mut);
    (void)BTREE_SYM(iter_release);
    (void)BTREE_SYM(iter_valid);
    (void)BTREE_SYM(iter_status);
    (void)BTREE_SYM(iter_seek);
    (void)BTREE_SYM(iter_seek_desc);
    (void)BTREE_SYM(iter_scan);
    (void)BTREE_SYM(iter_scan_desc);
    (void)BTREE_SYM(iter_seek_at);
    (void)BTREE_SYM(iter_seek_at_desc);
    (void)BTREE_SYM(iter_next);
    (void)BTREE_SYM(iter_item);
    (void)BTREE_SYM(scan);
    (void)BTREE_SYM(scan_desc);
    (void)BTREE_SYM(seek);
    (void)BTREE_SYM(seek_at);
    (void)BTREE_SYM(seek_at_desc);
    (void)BTREE_SYM(seek_desc);
    (void)BTREE_SYM(scan_mut);
    (void)BTREE_SYM(scan_desc_mut);
    (void)BTREE_SYM(seek_mut);
    (void)BTREE_SYM(seek_desc_mut);
    (void)BTREE_SYM(seek_at_mut);
    (void)BTREE_SYM(seek_at_desc_mut);
    (void)BTREE_SYM(shared);
}

static inline void BTREE_SYM(all_api_calls)(void) {
    // All external symbols
    (void)BTREE_SYM(all_api_calls);
    (void)BTREE_API(feat_maxitems);
    (void)BTREE_API(feat_minitems);
    (void)BTREE_API(feat_maxheight);
    (void)BTREE_API(feat_fanout);
    (void)BTREE_API(feat_counted);
    (void)BTREE_API(feat_ordered);
    (void)BTREE_API(feat_bsearch);
    (void)BTREE_API(feat_pathhint);
    (void)BTREE_API(feat_cow);
    (void)BTREE_API(feat_atomics);
    (void)BTREE_API(get_mut);
    (void)BTREE_API(get_mut_ref);
    (void)BTREE_API(get_at_mut);
    (void)BTREE_API(insert);
    (void)BTREE_API(get);
    (void)BTREE_API(index_of);    
    (void)BTREE_API(contains);
    (void)BTREE_API(delete);
    (void)BTREE_API(get_at);
    (void)BTREE_API(insert_at);
    (void)BTREE_API(delete_at);
    (void)BTREE_API(replace_at);
    (void)BTREE_API(count);
    (void)BTREE_API(height);
    (void)BTREE_API(clear);
    (void)BTREE_API(sane);
    (void)BTREE_API(front);
    (void)BTREE_API(front_mut);
    (void)BTREE_API(back);
    (void)BTREE_API(back_mut);
    (void)BTREE_API(pop_front);
    (void)BTREE_API(pop_back);
    (void)BTREE_API(push_front);
    (void)BTREE_API(push_back);
    (void)BTREE_API(copy);
    (void)BTREE_API(clone);
    (void)BTREE_API(compare);
    (void)BTREE_API(less);
    (void)BTREE_API(iter_init);
    (void)BTREE_API(iter_init_mut);
    (void)BTREE_API(iter_release);
    (void)BTREE_API(iter_valid);
    (void)BTREE_API(iter_status);
    (void)BTREE_API(iter_seek);
    (void)BTREE_API(iter_seek_desc);
    (void)BTREE_API(iter_scan);
    (void)BTREE_API(iter_scan_desc);
    (void)BTREE_API(iter_seek_at);
    (void)BTREE_API(iter_seek_at_desc);
    (void)BTREE_API(iter_next);
    (void)BTREE_API(iter_item);
    (void)BTREE_API(scan);
    (void)BTREE_API(scan_desc);
    (void)BTREE_API(seek);
    (void)BTREE_API(seek_at);
    (void)BTREE_API(seek_at_desc);
    (void)BTREE_API(seek_desc);
    (void)BTREE_API(scan_mut);
    (void)BTREE_API(scan_desc_mut);
    (void)BTREE_API(seek_mut);
    (void)BTREE_API(seek_desc_mut);
    (void)BTREE_API(seek_at_mut);
    (void)BTREE_API(seek_at_desc_mut);
}

///////////////////////////////////////////////////////////////////////////////
// Exposed API
///////////////////////////////////////////////////////////////////////////////
int BTREE_API(feat_maxitems)(void) {
    return BTREE_SYM(feat_maxitems)();
}

int BTREE_API(feat_minitems)(void) {
    return BTREE_SYM(feat_minitems)();
}

int BTREE_API(feat_maxheight)(void) {
    return BTREE_SYM(feat_maxheight)();
}

int BTREE_API(feat_fanout)(void) {
    return BTREE_SYM(feat_fanout)();
}

bool BTREE_API(feat_counted)(void) {
    return BTREE_SYM(feat_counted)();
}

bool BTREE_API(feat_ordered)(void) {
    return BTREE_SYM(feat_ordered)();
}

bool BTREE_API(feat_bsearch)(void) {
    return BTREE_SYM(feat_bsearch)();
}

bool BTREE_API(feat_pathhint)(void) {
    return BTREE_SYM(feat_pathhint)();
}

bool BTREE_API(feat_cow)(void) {
    return BTREE_SYM(feat_cow)();
}

bool BTREE_API(feat_atomics)(void) {
    return BTREE_SYM(feat_atomics)();
}

void BTREE_API(clear)(BTREE_NODE **root, void *udata) {
    BTREE_SYM(clear)(root, udata);
}

bool BTREE_API(sane)(BTREE_NODE **root, void *udata) {
    return BTREE_SYM(sane)(root, udata);
}

size_t BTREE_API(count)(BTREE_NODE **root, void *udata) {
    return BTREE_SYM(count)(root, udata);
}

size_t BTREE_API(height)(BTREE_NODE **root, void *udata) {
    return BTREE_SYM(height)(root, udata);
}

int BTREE_API(index_of)(BTREE_NODE **root, BTREE_ITEM key, size_t *index,
    void *udata)
{
    return BTREE_SYM(index_of)(root, key, index, udata);
}

int BTREE_API(get)(BTREE_NODE **root, BTREE_ITEM key, BTREE_ITEM *item_out,
    void *udata)
{
    return BTREE_SYM(get)(root, key, item_out, udata);
}

int BTREE_API(get_mut)(BTREE_NODE **root, BTREE_ITEM key, BTREE_ITEM *item_out,
    void *udata)
{
    return BTREE_SYM(get_mut)(root, key, item_out, udata);
}

int BTREE_API(get_mut_ref)(BTREE_NODE **root, BTREE_ITEM key, BTREE_ITEM **item,
    void *udata)
{
    return BTREE_SYM(get_mut_ref)(root, key, item, udata);
}

bool BTREE_API(contains)(BTREE_NODE **root, BTREE_ITEM key, void *udata) {
    return BTREE_SYM(contains)(root, key, udata);
}

int BTREE_API(insert)(BTREE_NODE **root, BTREE_ITEM item, BTREE_ITEM *olditem,
    void *udata)
{
    return BTREE_SYM(insert)(root, item, olditem, udata);
}

int BTREE_API(delete)(BTREE_NODE **root, BTREE_ITEM key, BTREE_ITEM *olditem, 
    void *udata)
{
    return BTREE_SYM(delete)(root, key, olditem, udata);
}

int BTREE_API(front)(BTREE_NODE **root, BTREE_ITEM *item_out, void *udata) {
    return BTREE_SYM(front)(root, item_out, udata);
}

int BTREE_API(front_mut)(BTREE_NODE **root, BTREE_ITEM *item_out, void *udata) {
    return BTREE_SYM(front_mut)(root, item_out, udata);
}

int BTREE_API(back)(BTREE_NODE **root, BTREE_ITEM *item_out, void *udata) {
    return BTREE_SYM(back)(root, item_out, udata);
}

int BTREE_API(back_mut)(BTREE_NODE **root, BTREE_ITEM *item_out, void *udata) {
    return BTREE_SYM(back_mut)(root, item_out, udata);
}

int BTREE_API(delete_at)(BTREE_NODE **root, size_t index, BTREE_ITEM *olditem,
    void *udata)
{
    return BTREE_SYM(delete_at)(root, index, olditem, udata);
}

int BTREE_API(replace_at)(BTREE_NODE **root, size_t index, BTREE_ITEM item,
    BTREE_ITEM *olditem, void *udata)
{
    return BTREE_SYM(replace_at)(root, index, item, olditem, udata);
}

int BTREE_API(pop_front)(BTREE_NODE **root, BTREE_ITEM *olditem, void *udata) {
    return BTREE_SYM(pop_front)(root, olditem, udata);
}

int BTREE_API(pop_back)(BTREE_NODE **root, BTREE_ITEM *olditem, void *udata) {
    return BTREE_SYM(pop_back)(root, olditem, udata);
}

int BTREE_API(get_at)(BTREE_NODE **root, size_t index, BTREE_ITEM *item,
    void *udata)
{
    return BTREE_SYM(get_at)(root, index, item, udata);
}

int BTREE_API(get_at_mut)(BTREE_NODE **root, size_t index, BTREE_ITEM *item,
    void *udata)
{
    return BTREE_SYM(get_at_mut)(root, index, item, udata);
}

int BTREE_API(push_front)(BTREE_NODE **root, BTREE_ITEM item, void *udata) {
    return BTREE_SYM(push_front)(root, item, udata);
}

int BTREE_API(push_back)(BTREE_NODE **root, BTREE_ITEM item, void *udata) {
    return BTREE_SYM(push_back)(root, item, udata);
}

int BTREE_API(insert_at)(BTREE_NODE **root, size_t index, BTREE_ITEM item,
    void *udata)
{
    return BTREE_SYM(insert_at)(root, index, item, udata);
}

int BTREE_API(copy)(BTREE_NODE **root, BTREE_NODE **newroot, void *udata) {
    return BTREE_SYM(copy)(root, newroot, udata);
}

int BTREE_API(clone)(BTREE_NODE **root, BTREE_NODE **newroot, void *udata) {
    return BTREE_SYM(clone)(root, newroot, udata);
}

int BTREE_API(compare)(BTREE_ITEM a, BTREE_ITEM b, void *udata) {
    return BTREE_SYM(compare)(a, b, udata);
}

bool BTREE_API(less)(BTREE_ITEM a, BTREE_ITEM b, void *udata) {
    return BTREE_SYM(less)(a, b, udata);
}

void BTREE_API(iter_init)(BTREE_NODE **root, BTREE_ITER **iter, void *udata) {
    BTREE_SYM(iter_init)(root, iter, udata);
}

void BTREE_API(iter_init_mut)(BTREE_NODE **root, BTREE_ITER **iter, void *udata) {
    BTREE_SYM(iter_init_mut)(root, iter, udata);
}

int BTREE_API(iter_status)(BTREE_ITER *iter) {
    return BTREE_SYM(iter_status)(iter);
}

bool BTREE_API(iter_valid)(BTREE_ITER *iter) {
    return BTREE_SYM(iter_valid)(iter);
}

void BTREE_API(iter_release)(BTREE_ITER *iter) {
    BTREE_SYM(iter_release)(iter);
}

void BTREE_API(iter_seek)(BTREE_ITER *iter, BTREE_ITEM key) {
    BTREE_SYM(iter_seek)(iter, key);
}

void BTREE_API(iter_seek_at)(BTREE_ITER *iter, size_t index) {
    BTREE_SYM(iter_seek_at)(iter, index);
}

void BTREE_API(iter_seek_at_desc)(BTREE_ITER *iter, size_t index) {
    BTREE_SYM(iter_seek_at_desc)(iter, index);
}

void BTREE_API(iter_seek_desc)(BTREE_ITER *iter, BTREE_ITEM key) {
    BTREE_SYM(iter_seek_desc)(iter, key);
}

void BTREE_API(iter_scan)(BTREE_ITER *iter) {
    BTREE_SYM(iter_scan)(iter);
}

void BTREE_API(iter_scan_desc)(BTREE_ITER *iter) {
    BTREE_SYM(iter_scan_desc)(iter);
}

void BTREE_API(iter_next)(BTREE_ITER *iter) {
    BTREE_SYM(iter_next)(iter);
}

void BTREE_API(iter_item)(BTREE_ITER *iter, BTREE_ITEM *item) {
    BTREE_SYM(iter_item)(iter, item);
}

int BTREE_API(scan)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item, void *udata),
    void *udata)
{
    return BTREE_SYM(scan)(root, iter, udata);
}

int BTREE_API(scan_desc)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item, 
    void *udata), void *udata)
{
    return BTREE_SYM(scan_desc)(root, iter, udata);
}

int BTREE_API(seek)(BTREE_NODE **root, BTREE_ITEM key, bool(*iter)(BTREE_ITEM item,
    void *udata), void *udata)
{
    return BTREE_SYM(seek)(root, key, iter, udata);
}

int BTREE_API(seek_desc)(BTREE_NODE **root, BTREE_ITEM key, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    return BTREE_SYM(seek_desc)(root, key, iter, udata);
}

int BTREE_API(seek_at)(BTREE_NODE **root, size_t index, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    return BTREE_SYM(seek_at)(root, index, iter, udata);
}

int BTREE_API(seek_at_mut)(BTREE_NODE **root, size_t index, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    return BTREE_SYM(seek_at_mut)(root, index, iter, udata);
}

int BTREE_API(seek_at_desc)(BTREE_NODE **root, size_t index, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    return BTREE_SYM(seek_at_desc)(root, index, iter, udata);
}

int BTREE_API(seek_at_desc_mut)(BTREE_NODE **root, size_t index, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    return BTREE_SYM(seek_at_desc_mut)(root, index, iter, udata);
}

int BTREE_API(scan_mut)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item,
    void *udata), void *udata)
{
    return BTREE_SYM(scan_mut)(root, iter, udata);
}

int BTREE_API(scan_desc_mut)(BTREE_NODE **root, bool(*iter)(BTREE_ITEM item, 
    void *udata), void *udata)
{
    return BTREE_SYM(scan_desc_mut)(root, iter, udata);
}

int BTREE_API(seek_mut)(BTREE_NODE **root, BTREE_ITEM key,
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    return BTREE_SYM(seek_mut)(root, key, iter, udata);
}

int BTREE_API(seek_desc_mut)(BTREE_NODE **root, BTREE_ITEM key, 
    bool(*iter)(BTREE_ITEM item, void *udata), void *udata)
{
    return BTREE_SYM(seek_desc_mut)(root, key, iter, udata);
}

#endif // BTREE_HEADER

// undefine everything
// use `gcc -dM -E <source>` to help find leftover BTREE_* defines
#undef BTREE_COW
#undef BTREE_KEY
#undef BTREE_KEYTYPE
#undef BTREE_NOTFOUND
#undef BTREE_NOPATHHINT
#undef BTREE_NODE
#undef BTREE_ASSERT
#undef BTREE_MINITEMS
#undef BTREE_NOINLINE
#undef BTREE_FREE
#undef BTREE_ITEM
#undef BTREE_REPLACED
#undef BTREE_SCANDESC
#undef BTREE_CC
#undef BTREE_C
#undef BTREE_PUSHFRONT
#undef BTREE_OUTOFORDER
#undef BTREE_NOATOMICS
#undef BTREE_BSEARCH
#undef BTREE_EXTERN
#undef BTREE_MAP
#undef BTREE_SYM
#undef BTREE_UNSUPPORTED
#undef BTREE_POPBACK
#undef BTREE_INSERTED
#undef BTREE_ITEMCOPY
#undef BTREE_MAXHEIGHT
#undef BTREE_MUSTSPLIT
#undef BTREE_ITEMFREE
#undef BTREE_MAXITEMS
#undef BTREE_KEYED
#undef BTREE_NOMEM
#undef BTREE_PITEM
#undef BTREE_REPAT
#undef BTREE_COPIED
#undef BTREE_DELKEY
#undef BTREE_SCAN
#undef BTREE_TYPE
#undef BTREE_API
#undef BTREE_POPFRONT
#undef BTREE_PUSHBACK
#undef BTREE_LINEAR
#undef BTREE_MALLOC
#undef BTREE_PQUEUE
#undef BTREE_BTREE
#undef BTREE_DELAT
#undef BTREE_DSIZE
#undef BTREE_REALLOC
#undef BTREE_STOPPED
#undef BTREE_VALUETYPE
#undef BTREE_FINISHED
#undef BTREE_FANOUTUSED
#undef BTREE_POPMAX
#undef BTREE_VALUE
#undef BTREE_HEADER
#undef BTREE_NOORDER
#undef BTREE_SNODE
#undef BTREE_PATHHINT
#undef BTREE_DELETED
#undef BTREE_INSITEM
#undef BTREE_FANOUT
#undef BTREE_INLINE
#undef BTREE_ITER
#undef BTREE_LESS
#undef BTREE_NAME
#undef BTREE_COMPARE
#undef BTREE_COUNTED
#undef BTREE_FOUND
#undef BTREE_INSAT
#undef BTREE_MAYBELESSEQUAL
#undef BTREE_SOURCE
#undef BTREE_LEAF_SIZE
#undef BTREE_BRANCH_SIZE
#undef BTREE_NODE_SIZE
