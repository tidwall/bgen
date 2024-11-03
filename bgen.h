// https://github.com/tidwall/bgen
//
// Copyright 2024 Joshua J Baker. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.
//
// Bgen - B-tree collection generator for C
//
// For a complete list of options visit:
// https://github.com/tidwall/bgen#options

// The API namespace. 
// This is the prefix for all functions calls, and is also and the name of the
// root node structure.
#ifndef BGEN_NAME
#error BGEN_NAME required
#define BGEN_NAME unnamed_bgen /* unused placeholder */
#endif

// macro concatenate
#define BGEN_CC(a, b) a ## b
#define BGEN_C(a, b)  BGEN_CC(a, b)

// API symbols are the calls available to the user.
#define BGEN_API(name)   BGEN_C(BGEN_C(BGEN_NAME, _), name)

// Internal symbols are prefixed with an underscore.
// These should not be directly called by the user.
#define BGEN_SYM(name)   BGEN_C(BGEN_C(BGEN_C(_, BGEN_NAME), _internal_), name)

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

// The internal item type. This is used as both the value type and the key
// type, and can be pretty much anything.
#ifndef BGEN_TYPE
#error BGEN_TYPE required
#define BGEN_TYPE int /* unused placeholder */
#endif

// The "fanout" is maximum number of child nodes that a branch node may have.
// For example a fanout of 4 is equivalent to a 2-3-4 tree where a branch may
// have 2, 3, or 4 children and branches and leaves may have 1, 2, or 3 items.
// This implementation clamps the fanout to the range of 4 to 4096, and also
// rounds it down to the nearest even number. Such that a value of 9 becomes 8.
#ifndef BGEN_FANOUT
#define BGEN_FANOUTUSED 16
#elif BGEN_FANOUT < 4 
#define BGEN_FANOUTUSED 4
#elif BGEN_FANOUT > 4096
#define BGEN_FANOUTUSED 4096
#elif BGEN_FANOUT % 2 == 1
#define BGEN_FANOUTUSED (BGEN_FANOUT-1)
#else
#define BGEN_FANOUTUSED BGEN_FANOUT
#endif

// MAXITEMS and MINITEMS are the minimum and maximum number of items allowed in
// each node, respectively.
#define BGEN_MAXITEMS  (BGEN_FANOUTUSED-1)
#define BGEN_MINITEMS  (BGEN_MAXITEMS/2)

// Estimated compile time worst case max height for a 64-bit system.
// In other words, this is the maximum possible height of a tree when it's
// fully loaded with SIZE_MAX items.
#if (BGEN_MINITEMS+1) >= 128
#define BGEN_MAXHEIGHT 9      /* pow(128,9+1) >= 18446744073709551615UL */
#elif (BGEN_MINITEMS+1) >= 64
#define BGEN_MAXHEIGHT 10     /* pow(64,10+1) >= 18446744073709551615UL */
#elif (BGEN_MINITEMS+1) >= 32
#define BGEN_MAXHEIGHT 12     /* pow(32,12+1) >= 18446744073709551615UL */
#elif (BGEN_MINITEMS+1) >= 16
#define BGEN_MAXHEIGHT 15     /* pow(16,15+1) >= 18446744073709551615UL */
#elif (BGEN_MINITEMS+1) >= 8
#define BGEN_MAXHEIGHT 21     /* pow(8,21+1)  >= 18446744073709551615UL */
#elif (BGEN_MINITEMS+1) >= 4
#define BGEN_MAXHEIGHT 31     /* pow(4,31+1)  >= 18446744073709551615UL */
#else
#define BGEN_MAXHEIGHT 63
#endif

#define BGEN_INLINE inline
#ifdef __GNUC__
#define BGEN_NOINLINE __attribute__((noinline))
#else
#define BGEN_NOINLINE
#endif

// Provide a custom allocator using BGEN_MALLOC and BGEN_FREE.
// Such as:
//
//     #define BGEN_MALLOC my_malloc
//     #define BGEN_FREE   my_free
//
// This will ensure that the tree will always use my_malloc/my_free instead of
// the standard malloc/free.
#if !defined(BGEN_MALLOC) || !defined(BGEN_FREE)

#include <stdlib.h>

#ifndef BGEN_MALLOC
#define BGEN_MALLOC malloc
#endif

#ifndef BGEN_FREE
#define BGEN_FREE free
#endif
#endif

#ifndef BGEN_EXTERN
#define BGEN_EXTERN static
#endif

// Enable Spatial B-tree support
#ifdef BGEN_SPATIAL
#ifndef BGEN_ITEMRECT
#error \
BGEN_ITEMRECT is required when BGEN_SPATIAL is defined. \
Visit https://github.com/tidwall/bgen for more information.
#endif
#else
#ifdef BGEN_ITEMRECT
#error \
BGEN_ITEMRECT must not be defined withou BGEN_SPATIAL. \
Visit https://github.com/tidwall/bgen for more information.
#endif
#endif

// Number of dimensions for Spatial B-tree
#ifndef BGEN_DIMS
#define BGEN_DIMS 2
#elif BGEN_DIMS < 1 || BGEN_DIMS > 4096
#error \
BGEN_DIMS must be between 1 and 4096 \
Visit https://github.com/tidwall/bgen for more information.
#endif

// Rectangle coordinate type for Spatial B-tree
#ifndef BGEN_RTYPE
#define BGEN_RTYPE double
#endif

// A path hint is a search optimization.
// It's most useful when bsearching, and is turned on by default when
// BGEN_BSEARCH is provided.
// This implementation uses one thread local path hint per each btree namespace.
// See https://github.com/tidwall/btree/blob/master/PATH_HINT.md
#if defined(BGEN_BSEARCH) && BGEN_FANOUT < 256
#ifndef BGEN_PATHHINT
#define BGEN_PATHHINT
#endif
#endif
#ifdef BGEN_NOPATHHINT
#undef BGEN_PATHHINT
#endif

// Convenient aliases to common types
#define BGEN_NODE struct BGEN_NAME
#define BGEN_ITEM BGEN_TYPE
#define BGEN_ITER struct BGEN_API(iter)
#define BGEN_SNODE struct BGEN_SYM(snode)
#define BGEN_RECT struct BGEN_SYM(rect)

// Definitions

// The following status codes are private to this file only.
// Users should use the prefixed version such as bt_INSERTED as defined in the
// enum below.
#define BGEN_INSERTED    1  // New item was inserted
#define BGEN_REPLACED    2  // Item replaced an existing item
#define BGEN_DELETED     3  // Item was successfully deleted
#define BGEN_FOUND       4  // Item was successfully accessed
#define BGEN_NOTFOUND    5  // Item was not found
#define BGEN_OUTOFORDER  6  // Item is out of order
#define BGEN_FINISHED    7  // Callback iterator returned all items
#define BGEN_STOPPED     8  // Callback iterator was stopped early
#define BGEN_COPIED      9  // Tree was copied: `clone`, `copy`
#define BGEN_NOMEM       10 // Out of memory
#define BGEN_UNSUPPORTED 11 // Operation not supported

enum BGEN_API(status) {
    BGEN_C(BGEN_NAME, _INSERTED)    = BGEN_INSERTED,
    BGEN_C(BGEN_NAME, _REPLACED)    = BGEN_REPLACED,
    BGEN_C(BGEN_NAME, _DELETED)     = BGEN_DELETED,
    BGEN_C(BGEN_NAME, _FOUND)       = BGEN_FOUND,
    BGEN_C(BGEN_NAME, _NOTFOUND)    = BGEN_NOTFOUND,
    BGEN_C(BGEN_NAME, _OUTOFORDER)  = BGEN_OUTOFORDER,
    BGEN_C(BGEN_NAME, _FINISHED)    = BGEN_FINISHED,
    BGEN_C(BGEN_NAME, _STOPPED)     = BGEN_STOPPED,
    BGEN_C(BGEN_NAME, _COPIED)      = BGEN_COPIED,
    BGEN_C(BGEN_NAME, _NOMEM)       = BGEN_NOMEM,
    BGEN_C(BGEN_NAME, _UNSUPPORTED) = BGEN_UNSUPPORTED,
};

BGEN_NODE;
BGEN_ITER;

BGEN_EXTERN int BGEN_API(get)(BGEN_NODE **root, BGEN_ITEM key,
    BGEN_ITEM *item_out, void *udata);
BGEN_EXTERN int BGEN_API(insert)(BGEN_NODE **root, BGEN_ITEM item,
    BGEN_ITEM *item_out, void *udata);
BGEN_EXTERN int BGEN_API(delete)(BGEN_NODE **root, BGEN_ITEM key, 
    BGEN_ITEM *item_out, void *udata);
BGEN_EXTERN bool BGEN_API(contains)(BGEN_NODE **root, BGEN_ITEM key,
    void *udata);
BGEN_EXTERN void BGEN_API(clear)(BGEN_NODE **root, void *udata);

BGEN_EXTERN int BGEN_API(front)(BGEN_NODE **root, BGEN_ITEM *item_out,
    void *udata);
BGEN_EXTERN int BGEN_API(back)(BGEN_NODE **root, BGEN_ITEM *item_out,
    void *udata);
BGEN_EXTERN int BGEN_API(pop_front)(BGEN_NODE **root, BGEN_ITEM *item_out,
    void *udata);
BGEN_EXTERN int BGEN_API(pop_back)(BGEN_NODE **root, BGEN_ITEM *item_out,
    void *udata);
BGEN_EXTERN int BGEN_API(push_front)(BGEN_NODE **root, BGEN_ITEM item,
    void *udata);
BGEN_EXTERN int BGEN_API(push_back)(BGEN_NODE **root, BGEN_ITEM item,
    void *udata);

BGEN_EXTERN int BGEN_API(copy)(BGEN_NODE **root, BGEN_NODE **newroot,
    void *udata);
BGEN_EXTERN int BGEN_API(clone)(BGEN_NODE **root, BGEN_NODE **newroot,
    void *udata);
BGEN_EXTERN int BGEN_API(compare)(BGEN_ITEM a, BGEN_ITEM b, void *udata);
BGEN_EXTERN bool BGEN_API(less)(BGEN_ITEM a, BGEN_ITEM b, void *udata);

// Optimized for counted B-trees (works with indexes) (rank=index_of,
// select=get_at)
BGEN_EXTERN int BGEN_API(insert_at)(BGEN_NODE **root, size_t index,
    BGEN_ITEM item, void *udata);
BGEN_EXTERN int BGEN_API(delete_at)(BGEN_NODE **root, size_t index,
    BGEN_ITEM *item_out, void *udata);
BGEN_EXTERN int BGEN_API(replace_at)(BGEN_NODE **root, size_t index,
    BGEN_ITEM item, BGEN_ITEM *item_out, void *udata);
BGEN_EXTERN int BGEN_API(get_at)(BGEN_NODE **root, size_t index,
    BGEN_ITEM *item_out, void *udata);
BGEN_EXTERN int BGEN_API(index_of)(BGEN_NODE **root, BGEN_ITEM key,
    size_t *index, void *udata);
BGEN_EXTERN size_t BGEN_API(count)(BGEN_NODE **root, void *udata);

// Cursor Iterators
BGEN_EXTERN void BGEN_API(iter_init)(BGEN_NODE **root, BGEN_ITER *iter,
    void *udata);
BGEN_EXTERN int BGEN_API(iter_status)(BGEN_ITER *iter);
BGEN_EXTERN bool BGEN_API(iter_valid)(BGEN_ITER *iter);
BGEN_EXTERN void BGEN_API(iter_release)(BGEN_ITER *iter);
BGEN_EXTERN void BGEN_API(iter_item)(BGEN_ITER *iter, BGEN_ITEM *item);
BGEN_EXTERN void BGEN_API(iter_next)(BGEN_ITER *iter);

// Curstor iterator seekers
BGEN_EXTERN void BGEN_API(iter_seek)(BGEN_ITER *iter, BGEN_ITEM key);
BGEN_EXTERN void BGEN_API(iter_seek_desc)(BGEN_ITER *iter, BGEN_ITEM key);
BGEN_EXTERN void BGEN_API(iter_scan)(BGEN_ITER *iter);
BGEN_EXTERN void BGEN_API(iter_scan_desc)(BGEN_ITER *iter);
BGEN_EXTERN void BGEN_API(iter_intersects)(BGEN_ITER *iter, BGEN_RTYPE min[],
    BGEN_RTYPE max[]);
BGEN_EXTERN void BGEN_API(iter_nearby)(BGEN_ITER *iter, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS], 
    void *target, void *udata));
BGEN_EXTERN void BGEN_API(iter_seek_at)(BGEN_ITER *iter, size_t index);
BGEN_EXTERN void BGEN_API(iter_seek_at_desc)(BGEN_ITER *iter, size_t index);

// Callback iterators
BGEN_EXTERN int BGEN_API(scan)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item, 
    void *udata), void *udata);
BGEN_EXTERN int BGEN_API(scan_desc)(BGEN_NODE **root, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(seek)(BGEN_NODE **root, BGEN_ITEM key, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(seek_desc)(BGEN_NODE **root, BGEN_ITEM key, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(intersects)(BGEN_NODE **root,
    BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS],
        bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(nearby)(BGEN_NODE **root, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS], 
    void *target, void *udata), bool(*iter)(BGEN_ITEM item, void *udata), 
    void *udata);
BGEN_EXTERN int BGEN_API(seek_at)(BGEN_NODE **root, size_t index,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(seek_at_desc)(BGEN_NODE **root, size_t index,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);

// General information
BGEN_EXTERN int BGEN_API(feat_maxitems)(void);
BGEN_EXTERN int BGEN_API(feat_minitems)(void);
BGEN_EXTERN int BGEN_API(feat_maxheight)(void);
BGEN_EXTERN int BGEN_API(feat_fanout)(void);
BGEN_EXTERN bool BGEN_API(feat_counted)(void);
BGEN_EXTERN bool BGEN_API(feat_spatial)(void);
BGEN_EXTERN bool BGEN_API(feat_ordered)(void);
BGEN_EXTERN bool BGEN_API(feat_cow)(void);
BGEN_EXTERN bool BGEN_API(feat_atomics)(void);
BGEN_EXTERN bool BGEN_API(feat_bsearch)(void);
BGEN_EXTERN bool BGEN_API(feat_pathhint)(void);
BGEN_EXTERN int BGEN_API(feat_dims)(void);

BGEN_EXTERN size_t BGEN_API(height)(BGEN_NODE **root, void *udata);
BGEN_EXTERN bool BGEN_API(sane)(BGEN_NODE **root, void *udata);

BGEN_EXTERN void BGEN_API(rect)(BGEN_NODE **root, BGEN_RTYPE min[BGEN_DIMS], 
    BGEN_RTYPE max[BGEN_DIMS], void *udata);

// Read functions that return items which are intended to be mutated.
// These perform copy-on-write on internal nodes and copies the items before
// returning them to the user. 
BGEN_EXTERN int BGEN_API(get_mut)(BGEN_NODE **root, BGEN_ITEM key,
    BGEN_ITEM *item_out, void *udata);
BGEN_EXTERN int BGEN_API(get_at_mut)(BGEN_NODE **root, size_t index,
    BGEN_ITEM *item_out, void *udata);
BGEN_EXTERN int BGEN_API(front_mut)(BGEN_NODE **root, BGEN_ITEM *item_out,
    void *udata);
BGEN_EXTERN int BGEN_API(back_mut)(BGEN_NODE **root, BGEN_ITEM *item_out,
    void *udata);
BGEN_EXTERN void BGEN_API(iter_init_mut)(BGEN_NODE **root, BGEN_ITER *iter,
    void *udata);
BGEN_EXTERN int BGEN_API(scan_mut)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item,
    void *udata), void *udata);
BGEN_EXTERN int BGEN_API(scan_desc_mut)(BGEN_NODE **root,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(seek_mut)(BGEN_NODE **root, BGEN_ITEM key,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(seek_desc_mut)(BGEN_NODE **root, BGEN_ITEM key,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(intersects_mut)(BGEN_NODE **root,
    BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS],
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(nearby_mut)(BGEN_NODE **root, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS],
    void *target, void *udata), bool(*iter)(BGEN_ITEM item, void *udata),
    void *udata);
BGEN_EXTERN int BGEN_API(seek_at_mut)(BGEN_NODE **root, size_t index,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);
BGEN_EXTERN int BGEN_API(seek_at_desc_mut)(BGEN_NODE **root, size_t index,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata);

#ifndef BGEN_DEFS

// IMPLEMENTATION

#ifdef BGEN_LESS
#ifdef BGEN_COMPARE
#error \
BGEN_COMPARE and BGEN_LESS cannot be both defined
#endif
#ifdef BGEN_KEYED
// Using nested compare for keyed collection type
static bool BGEN_SYM(less)(BGEN_ITEM a2, BGEN_ITEM b2, void *udata) {
    BGEN_KEYTYPE a = a2.key, b = b2.key;
    (void)a, (void)b, (void)udata;
    BGEN_LESS
}
#else
static bool BGEN_SYM(less)(BGEN_ITEM a, BGEN_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    BGEN_LESS
}
#endif
static int BGEN_SYM(compare)(BGEN_ITEM a, BGEN_ITEM b, void *udata) {
    return BGEN_SYM(less)(a, b, udata) ? -1 :
           BGEN_SYM(less)(b, a, udata) ? 1 :
           0;
}
#elif defined(BGEN_COMPARE)
#ifdef BGEN_KEYED
// Using nested compare for keyed collection type
static int BGEN_SYM(compare)(BGEN_ITEM a2, BGEN_ITEM b2, void *udata) {
    BGEN_KEYTYPE a = a2.key, b = b2.key;
    (void)a, (void)b, (void)udata;
    BGEN_COMPARE
}
#else
static int BGEN_SYM(compare)(BGEN_ITEM a, BGEN_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    BGEN_COMPARE
}
#endif
static bool BGEN_SYM(less)(BGEN_ITEM a, BGEN_ITEM b, void *udata) {
    return BGEN_SYM(compare)(a, b, udata) < 0;
}
#else
static bool BGEN_SYM(less)(BGEN_ITEM a, BGEN_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    return false;
}
static int BGEN_SYM(compare)(BGEN_ITEM a, BGEN_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    return -1;
}
#if !defined(BGEN_NOORDER)
#error \
Neither BGEN_COMPARE nor BGEN_LESS were defined. \
Alternatively define BGEN_NOORDER if only the "Counted B-tree" API is desired. \
Visit https://github.com/tidwall/bgen for more information.
#endif
#endif

#if defined(BGEN_NOORDER) && (defined(BGEN_LESS) || defined(BGEN_COMPARE))
#error \
Neither BGEN_COMPARE nor BGEN_LESS are allowed when BGEN_NOORDER is defined. \
Visit https://github.com/tidwall/bgen for more information.
#endif

#ifdef BGEN_MAYBELESSEQUAL
static bool BGEN_SYM(maybelessequal)(BGEN_ITEM a, BGEN_ITEM b, void *udata) {
    (void)a, (void)b, (void)udata;
    BGEN_MAYBELESSEQUAL
}
#endif

#ifdef BGEN_SPATIAL
BGEN_RECT {
    BGEN_RTYPE min[BGEN_DIMS];
    BGEN_RTYPE max[BGEN_DIMS];
};

static BGEN_RECT BGEN_SYM(item_rect)(BGEN_ITEM item, void *udata) {
    (void)item, (void)udata;
    BGEN_RTYPE min[BGEN_DIMS] = { 0 };
    BGEN_RTYPE max[BGEN_DIMS] = { 0 };
#ifdef BGEN_ITEMRECT
    BGEN_ITEMRECT
#endif
    BGEN_RECT rect;
    for (int i = 0; i < BGEN_DIMS; i++) {
        rect.min[i] = min[i];
    }
    for (int i = 0; i < BGEN_DIMS; i++) {
        rect.max[i] = max[i];
    }
    return rect;
}
#endif

static bool BGEN_SYM(item_copy)(BGEN_ITEM item, BGEN_ITEM *copy, void *udata) {
    (void)item, (void)copy, (void)udata;
#ifdef BGEN_ITEMCOPY
    BGEN_ITEMCOPY
#else
    *copy = item;
    return true;
#endif
}

static void BGEN_SYM(item_free)(BGEN_ITEM item, void *udata) {
    (void)item, (void)udata;
#ifdef BGEN_ITEMFREE
    BGEN_ITEMFREE
#endif
}

#ifdef BGEN_COW
#ifdef BGEN_NOATOMICS

typedef int BGEN_SYM(rc_t);
static int BGEN_SYM(rc_load)(BGEN_SYM(rc_t) *ptr) {
    return *ptr;
}
static int BGEN_SYM(rc_fetch_sub)(BGEN_SYM(rc_t) *ptr, int delta) {
    int rc = *ptr;
    *ptr -= delta;
    return rc;
}
static int BGEN_SYM(rc_fetch_add)(BGEN_SYM(rc_t) *ptr, int delta) {
    int rc = *ptr;
    *ptr += delta;
    return rc;
}

#else

#include <stdatomic.h>

typedef atomic_int BGEN_SYM(rc_t);
static int BGEN_SYM(rc_load)(BGEN_SYM(rc_t) *ptr) {
    return atomic_load(ptr);
}
static int BGEN_SYM(rc_fetch_sub)(BGEN_SYM(rc_t) *ptr, int delta) {
    return atomic_fetch_sub(ptr, delta);
}
static int BGEN_SYM(rc_fetch_add)(BGEN_SYM(rc_t) *ptr, int delta) {
    return atomic_fetch_add(ptr, delta);
}

#endif
#endif

BGEN_NODE {
    BGEN_ITEM items[BGEN_MAXITEMS];  // all items in node, ordered
#ifdef BGEN_COW
    BGEN_SYM(rc_t) rc; // reference counter
#endif
    short len; // number of items in this node
    short height; // tree height (one is leaf)
    bool isleaf; // node is a leaf
    
    // leaves omit the following fields
    BGEN_NODE *children[BGEN_MAXITEMS+1]; // child nodes
#ifdef BGEN_COUNTED
    size_t counts[BGEN_MAXITEMS+1]; // counts for child nodes
#endif
#ifdef BGEN_SPATIAL
    BGEN_RECT rects[BGEN_MAXITEMS+1];
#endif
};

#ifdef BGEN_ASSERT
#include <assert.h>
#undef BGEN_ASSERT
#define BGEN_ASSERT(cond) assert(cond)
#else
#define BGEN_ASSERT(cond)(void)0
#endif

static int BGEN_SYM(feat_maxitems)(void) {
    return BGEN_MAXITEMS;
}
static int BGEN_SYM(feat_minitems)(void) {
    return BGEN_MINITEMS;
}
static int BGEN_SYM(feat_maxheight)(void) {
    return BGEN_MAXHEIGHT;
}
static int BGEN_SYM(feat_fanout)(void) {
    return BGEN_FANOUTUSED;
}
static bool BGEN_SYM(feat_counted)(void) {
#ifdef BGEN_COUNTED
    return true;
#else
    return false;
#endif
}
static bool BGEN_SYM(feat_spatial)(void) {
#ifdef BGEN_SPATIAL
    return true;
#else
    return false;
#endif
}
static bool BGEN_SYM(feat_ordered)(void) {
#ifdef BGEN_NOORDER
    return false;
#else
    return true;
#endif
}

static bool BGEN_SYM(feat_cow)(void) {
#ifdef BGEN_COW
    return true;
#else
    return false;
#endif
}
static bool BGEN_SYM(feat_bsearch)(void) {
#ifdef BGEN_BSEARCH
    return true;
#else
    return false;
#endif
}
static bool BGEN_SYM(feat_pathhint)(void) {
#ifdef BGEN_PATHHINT
    return true;
#else
    return false;
#endif
}

static bool BGEN_SYM(feat_atomics)(void) {
#ifndef BGEN_NOATOMICS
    return true;
#else
    return false;
#endif
}

static int BGEN_SYM(feat_dims)(void) {
#ifdef BGEN_SPATIAL
    return BGEN_DIMS;
#else
    return 0;
#endif
}

static BGEN_NODE *BGEN_SYM(alloc_node)(bool isleaf) {
    void *ptr = isleaf ? BGEN_MALLOC(offsetof(BGEN_NODE, children)) :
        BGEN_MALLOC(sizeof(BGEN_NODE));
    if (!ptr) {
        return 0;
    }
    BGEN_NODE *node = (BGEN_NODE*)ptr;
#ifdef BGEN_COW
    node->rc = 0;
#endif
    node->isleaf = isleaf;
    node->height = 0;
    node->len = 0;
    return node;
}

// returns the number of items in a node by counting, recursively
static size_t BGEN_SYM(deepcount)(BGEN_NODE *node) {
    size_t count = (size_t)node->len;
    if (!node->isleaf) {
        for (int i = 0; i < node->len+1; i++) {
            count += BGEN_SYM(deepcount)(node->children[i]);
        }
    }
    return count;
}

// returns the height of the node counting the depth, recursively
static int BGEN_SYM(deepheight)(BGEN_NODE *node) {
    int height = 0;
    while (1) {
        height++;
        if (node->isleaf) {
            return height;
        }
        node = node->children[0];
    }
}

#ifdef BGEN_SPATIAL

static bool BGEN_SYM(rect_intersects)(BGEN_RECT a, BGEN_RECT b) {
    int bits = 0;
    for (int i = 0; i < BGEN_DIMS; i++) {
        bits |= b.min[i] > a.max[i];
        bits |= b.max[i] < a.min[i];
    }
    return bits == 0;
}

static BGEN_RECT BGEN_SYM(rect_join)(BGEN_RECT a, BGEN_RECT b) {
    for (int i = 0; i < BGEN_DIMS; i++) {
        a.min[i] = a.min[i] < b.min[i] ? a.min[i] : b.min[i];
    }
    for (int i = 0; i < BGEN_DIMS; i++) {
        a.max[i] = a.max[i] > b.max[i] ? a.max[i] : b.max[i];
    }
    return a;
}

static bool BGEN_SYM(feq)(BGEN_RTYPE a, BGEN_RTYPE b) {
    return !(a < b || a > b);
}

static bool BGEN_SYM(recteq)(BGEN_RECT a, BGEN_RECT b) {
    for (int i = 0; i < BGEN_DIMS; i++) {
        if (!BGEN_SYM(feq)(a.min[i], b.min[i]) || 
            !BGEN_SYM(feq)(a.max[i], b.max[i]))
        {
            return false;
        }
    }
    return true;
}

static bool BGEN_SYM(rect_onedge)(BGEN_RECT rect, BGEN_RECT other) {
    for (int i = 0; i < BGEN_DIMS; i++) {
        if (BGEN_SYM(feq)(rect.min[i], other.min[i]) || 
            BGEN_SYM(feq)(rect.max[i], other.max[i]))
        {
            return true;
        }
    }
    return false;
}

// Returns a rectangle for child+item at index. 
static BGEN_RECT BGEN_SYM(rect_calc)(BGEN_NODE *node, int i, void *udata) {
    (void)node;
    BGEN_ASSERT(node && !node->isleaf);
#ifndef BGEN_SPATIAL
    (void)i, (void)udata;
    return (BGEN_RECT){ 0 };
#else
    BGEN_NODE *child = node->children[i];
    BGEN_RECT rect;
    if (!child->isleaf) {
        rect = child->rects[0];
        for (int j = 1; j <= child->len; j++) {
            rect = BGEN_SYM(rect_join)(rect, child->rects[j]);
        }
    } else {
        rect = BGEN_SYM(item_rect)(child->items[0], udata);
        for (int j = 1; j < child->len; j++) {
            rect = BGEN_SYM(rect_join)(rect, 
                BGEN_SYM(item_rect)(child->items[j], udata));
        }
    }
    if (i < node->len) {
        rect = BGEN_SYM(rect_join)(rect,
            BGEN_SYM(item_rect)(node->items[i], udata));
    }
    return rect;
#endif
}

static BGEN_RECT BGEN_SYM(deeprect)(BGEN_NODE *node, void *udata) {
    BGEN_RECT rect = { 0 };
    if (node->len <= BGEN_MAXITEMS) {
        rect = BGEN_SYM(item_rect)(node->items[0], udata);
        for (int i = 0; i < node->len; i++) {
            BGEN_RECT irect = BGEN_SYM(item_rect)(node->items[i], udata);
            rect = BGEN_SYM(rect_join)(rect, irect);
        }
        if (!node->isleaf) {
            for (int i = 0; i <= node->len; i++) {
                rect = BGEN_SYM(rect_join)(rect,
                    BGEN_SYM(deeprect)(node->children[i], udata));
            }
        }
    }
    return rect;
}
#endif

static bool BGEN_SYM(sane0)(BGEN_NODE *node, void *udata, int depth) {
    // check the number of items in node.
    if (depth == 0) {
        // the root is allowed to have one item.
        if (node->len < 1 || node->len > BGEN_MAXITEMS) {
            return false;
        }
    } else {
        if (node->len < BGEN_MINITEMS || node->len > BGEN_MAXITEMS) {
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
    if (node->height != BGEN_SYM(deepheight)(node)) {
        return false;
    }
    // check the order of items.
#ifndef BGEN_NOORDER
    for (int i = 1; i < node->len; i++) {
        if (BGEN_SYM(compare)(node->items[i-1], node->items[i], udata) >= 0) {
            return false;
        }
    }
#endif
    if (!node->isleaf) {
        // continue sanity test down the tree.
#ifndef BGEN_NOORDER
        // Check the order of each branch item, comparing to the children to
        // the left and right.
        for (int i = 0; i < node->len; i++) {
            if (node->children[i]->len > 0 && 
                node->children[i]->len <= BGEN_MAXITEMS &&
                node->children[i+1]->len > 0 &&
                node->children[i+1]->len <= BGEN_MAXITEMS)
            {
                if (BGEN_SYM(compare)(
                    node->children[i]->items[node->children[i]->len-1], 
                    node->items[i], udata) >= 0 ||
                    BGEN_SYM(compare)(node->items[i],
                    node->children[i+1]->items[0], udata) >= 0)
                {
                    return false;
                }
            }
        }
#endif
        // check the sanity of child node
        for (int i = 0; i <= node->len; i++) {
#ifdef BGEN_COUNTED
            size_t count = BGEN_SYM(deepcount)(node->children[i]);
            if (count != node->counts[i]) {
                return false;
            }
#endif
#ifdef BGEN_SPATIAL
            BGEN_RECT rect = BGEN_SYM(deeprect)(node->children[i], udata);
            if (i < node->len) {
                BGEN_RECT irect = BGEN_SYM(item_rect)(node->items[i], udata);
                rect = BGEN_SYM(rect_join)(rect, irect);
            }
            if (!BGEN_SYM(recteq)(node->rects[i], rect)) {
                return false;
            }
#endif
            if (!BGEN_SYM(sane0)(node->children[i], udata, depth+1)) {
                return false;
            }
        }
    }
    return true;
}

// sanity checker
static bool BGEN_SYM(sane)(BGEN_NODE **root, void *udata) {
    bool sane = true;
    if (*root) {
        return BGEN_SYM(sane0)(*root, udata, 0);
    }
    return sane;
}

static size_t BGEN_SYM(count0)(BGEN_NODE *node) {
#ifndef BGEN_COUNTED
    return BGEN_SYM(deepcount)(node);
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
static size_t BGEN_SYM(count)(BGEN_NODE **root, void *udata) {
    (void)udata;
    return *root ? BGEN_SYM(count0)(*root) : 0;
}

// returns the number of items in tree
static size_t BGEN_SYM(height)(BGEN_NODE **root, void *udata) {
    (void)udata;
    return *root ? (size_t)(*root)->height : 0;
}

// Returns the number of items in child node at index.
// This will use the 'count' value if available.
static size_t BGEN_SYM(node_count)(BGEN_NODE *branch, int node_index) {
#ifndef BGEN_COUNTED
    return BGEN_SYM(count0)(branch->children[node_index]);
#else
    return branch->counts[node_index];
#endif
}

static void BGEN_SYM(free)(BGEN_NODE *node, void *udata) {
#ifdef BGEN_COW
    if (BGEN_SYM(rc_fetch_sub)(&node->rc, 1) > 0) {
        return;
    }
#endif
    if (!node->isleaf) {
        for (int i = 0; i < node->len+1; i++) {
            BGEN_SYM(free)(node->children[i], udata);
        }
    }
    for (int i = 0; i < node->len; i++) {
        BGEN_SYM(item_free)(node->items[i], udata);
    }
    BGEN_FREE(node);
}

/// Free the tree!
static void BGEN_SYM(clear)(BGEN_NODE **root, void *udata) {
    if (*root) {
        BGEN_SYM(free)(*root, udata);
        *root = 0;
    }
}

#ifdef BGEN_BSEARCH
BGEN_INLINE
static int BGEN_SYM(search_bsearch)(BGEN_ITEM *items, int nitems,
    BGEN_ITEM key, void *udata, int *found)
{
    // Standard bsearch. Balanced. Relies on branch prediction.
    int i = 0;
    int n = nitems;
    while (i < n) {
        int j = (i + n) / 2;
        int cmp = BGEN_SYM(compare)(key, items[j], udata);
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
BGEN_INLINE
static int BGEN_SYM(search_linear)(BGEN_ITEM *items, int nitems, BGEN_ITEM key,
    void *udata, int *found)
{
    int i = 0;
    *found = 0;
#ifdef BGEN_MAYBELESSEQUAL
    for (; i < nitems; i++) {
        if (BGEN_SYM(maybelessequal)(key, items[i], udata)) {
            goto compare;
        }
    }
#endif
#ifdef BGEN_LESS
    for (; i < nitems; i++) {
#ifdef BGEN_MAYBELESSEQUAL
    compare:
#endif
        if (BGEN_SYM(less)(key, items[i], udata)) {
            break;
        }
        if (!BGEN_SYM(less)(items[i], key, udata)) {
            *found = 1;
            break;
        }
    }
#else
    int cmp;
    for (; i < nitems; i++) {
#ifdef BGEN_MAYBELESSEQUAL
    compare:
#endif
        cmp = BGEN_SYM(compare)(key, items[i], udata);
        if (cmp <= 0) {
            *found = cmp == 0;
            break;
        }
    }
#endif
    return i;
}
#endif


static int BGEN_SYM(search)(BGEN_NODE *node, BGEN_ITEM key, void *udata,
    int *found, int depth)
{
#ifndef BGEN_PATHHINT
    (void)depth; // not used
#ifdef BGEN_BSEARCH
    return BGEN_SYM(search_bsearch)(node->items, node->len, key, udata, found);
#else // BGEN_LINEAR
    return BGEN_SYM(search_linear)(node->items, node->len, key, udata, found);
#endif
#else
    // path hints are activated
    BGEN_ITEM *items = node->items;
    int nitems = node->len;
    int i = 0;
    static __thread uint8_t BGEN_SYM(ghint)[BGEN_MAXHEIGHT] = { 0 };
    int j = BGEN_SYM(ghint)[depth];
    if (j >= node->len)  {
        j = node->len-1;
    }
    int cmp = BGEN_SYM(compare)(key, items[j], udata);
    if (cmp == 0) {
        *found = 1;
        return j;
    } else if (cmp < 0) {
        if (j == 0) {
            *found = 0;
            return 0;
        }
        int cmp = BGEN_SYM(compare)(items[j-1], key, udata);
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
        int cmp = BGEN_SYM(compare)(key, items[j+1], udata);
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
#ifdef BGEN_BSEARCH
    i += BGEN_SYM(search_bsearch)(items+i, nitems, key, udata, found);
#else // BGEN_LINEAR
    i += BGEN_SYM(search_linear)(items+i, nitems, key, udata, found);
#endif
okhint:
    BGEN_SYM(ghint)[depth] = (uint8_t)i;
    return i;
#endif
}

static void BGEN_SYM(print_spaces)(FILE *file, int depth) {
    for (int i = 0; i < depth; i++) {
        fprintf(file, "    ");
    }
}

static void BGEN_SYM(node_print)(BGEN_NODE *node, FILE *file,
    void(*print_item)(BGEN_ITEM item, FILE *file, void *udata),
    void(*print_rtype)(BGEN_RTYPE rtype, FILE *file, void *udata), 
    int depth, void *udata)
{
    BGEN_SYM(print_spaces)(file, depth);
    fprintf(file, ".isleaf=%d ", node->isleaf);
#ifdef BGEN_COW
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
#ifdef BGEN_COUNTED
        fprintf(file, ".counts=[ ");
        for (int i = 0; i <= node->len; i++) {
            fprintf(file, "%zu ", node->counts[i]);
        }
        fprintf(file, "] ");
#endif
#ifdef BGEN_SPATIAL
        fprintf(file, ".rects=[ ");
        for (int i = 0; i <= node->len; i++) {
            fprintf(file, "[ ");
            for (int j = 0; j < BGEN_DIMS; j++) {
                if (print_rtype) {
                    print_rtype(node->rects[i].min[j], file, udata);
                    fprintf(file, " ");
                }
            }
            for (int j = 0; j < BGEN_DIMS; j++) {
                if (print_rtype) {
                    print_rtype(node->rects[i].max[j], file, udata);
                    fprintf(file, " ");
                }
            }
            fprintf(file, "] ");
        }
        fprintf(file, "] ");
#endif
        fprintf(file, ".children=[\n");
        for (int i = 0; i <= node->len; i++) {
            BGEN_SYM(node_print)(node->children[i], file, print_item, 
                print_rtype, depth+1, udata);
        }
        BGEN_SYM(print_spaces)(file, depth);
        fprintf(file, "] ");
    }
    fprintf(file, "\n");
}

static void BGEN_SYM(print_feats)(FILE *file) {
    fprintf(file, "( .fanout=%d .minitems=%d .maxitems=%d .counted=%d "
        ".spatial=%d .bsearch=%d .pathhint=%d .cow=%d .atomics=%d", 
        BGEN_SYM(feat_fanout)(), 
        BGEN_SYM(feat_minitems)(), 
        BGEN_SYM(feat_maxitems)(),
        BGEN_SYM(feat_counted)(),
        BGEN_SYM(feat_spatial)(),
        BGEN_SYM(feat_bsearch)(),
        BGEN_SYM(feat_pathhint)(),
        BGEN_SYM(feat_cow)(),
        BGEN_SYM(feat_atomics)()
    );
    #ifdef BGEN_SPATIAL
        fprintf(file, " .dims=%d", BGEN_SYM(feat_dims)());
    #endif
    fprintf(file, " )\n");
}

static void BGEN_SYM(print)(BGEN_NODE **root, FILE *file,
    void(*print_item)(BGEN_ITEM item, FILE *file, void *udata), 
    void(*print_rtype)(BGEN_RTYPE rtype, FILE *file, void *udata),
    void *udata)
{
    BGEN_SYM(print_feats)(file);
    if (*root) {
        BGEN_SYM(node_print)(*root, file, print_item, print_rtype, 0, udata);
    }
}

static BGEN_NODE *BGEN_SYM(node_copy)(BGEN_NODE *node, bool deep, void *udata) {
    BGEN_NODE *node2 = BGEN_SYM(alloc_node)(node->isleaf);
    if (!node2) {
        return 0;
    }
    node2->len = node->len;
    node2->height = node->height;
    
    int icopied = 0;
    int ccopied = 0;

    // Copy items
    for (int i = 0; i < node->len; i++) {
        if (!BGEN_SYM(item_copy)(node->items[i], &node2->items[i], udata)) {
            goto fail;
        }
        icopied++;
    }
    if (!node->isleaf) {
        // Copy children
        for (int i = 0; i <= node->len; i++) {
#ifdef BGEN_COW
            if (!deep) {
                node2->children[i] = node->children[i];
                BGEN_SYM(rc_fetch_add)(&node2->children[i]->rc, 1);
            } else {
#else 
            {
#endif
                node2->children[i] = BGEN_SYM(node_copy)(node->children[i], 
                    deep, udata);
                if (!node2->children[i]) {
                    goto fail;
                }
            }
            ccopied++;
        }
#ifdef BGEN_COUNTED
        for (int i = 0; i <= node->len; i++) {
            node2->counts[i] = node->counts[i];
        }
#endif
#ifdef BGEN_SPATIAL
        for (int i = 0; i <= node->len; i++) {
            node2->rects[i] = node->rects[i];
        }
#endif

    }
    return node2;
fail:
    // Somthing failed to copy. Assume NOMEM and revert the allocated node.
    for (int i = 0; i < icopied; i++) {
        BGEN_SYM(item_free)(node2->items[i], udata);
    }
    if (!node->isleaf) {
        for (int i = 0; i < ccopied; i++) {
            BGEN_SYM(free)(node2->children[i], udata);
        }
    }
    BGEN_FREE(node2);
    return 0;
}

// Check if node is being shared (referenced) by other clones.
static bool BGEN_SYM(shared)(BGEN_NODE *node) {
#ifndef BGEN_COW
    (void)node;
    return false;
#else
    return BGEN_SYM(rc_load)(&node->rc) > 0;
#endif
}

// Perform copy-on-write operation. 
// Returns true on success or false on failure (NOMEM).
static bool BGEN_SYM(cow)(BGEN_NODE **node, void *udata) {
#ifndef BGEN_COW
    (void)node, (void)udata;
#else
    if (BGEN_SYM(shared)(*node)) {
        BGEN_NODE *node2 = BGEN_SYM(node_copy)(*node, false, udata);
        if (!node2) {
            return false;
        }
        BGEN_SYM(free)(*node, udata);
        *node = node2;
    }
#endif
    return true;
}

static bool BGEN_SYM(node_scan)(BGEN_NODE *node, bool(*iter)(BGEN_ITEM item, 
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
        if (!BGEN_SYM(node_scan)(node->children[i], iter, udata)) {
            return false;
        }
        if (!iter(node->items[i], udata)) {
            return false;
        }
    }
    return BGEN_SYM(node_scan)(node->children[node->len], iter, udata);
}

static int BGEN_SYM(scan)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item,
    void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(node_scan)(*root, iter, udata)) {
            status = BGEN_STOPPED;
        }
    }
    return status;
}

static bool BGEN_SYM(node_scan_mut)(BGEN_NODE *node, bool(*iter)(BGEN_ITEM item, 
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
        if (!BGEN_SYM(cow)(&node->children[i], udata)) {
            *status = BGEN_NOMEM;
            return false;
        }
        if (!BGEN_SYM(node_scan_mut)(node->children[i], iter, udata, status)) {
            return false;
        }
        if (!iter(node->items[i], udata)) {
            return false;
        }
    }
    if (!BGEN_SYM(cow)(&node->children[node->len], udata)) {
        *status = BGEN_NOMEM;
        return false;
    }
    return BGEN_SYM(node_scan_mut)(node->children[node->len], iter, udata,
        status);
}

static int BGEN_SYM(scan_mut)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item,
    void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(cow)(root, udata)) {
            return BGEN_NOMEM;
        }
        if (!BGEN_SYM(node_scan_mut)(*root, iter, udata, &status)) {
            if (status == BGEN_FINISHED) {
                status = BGEN_STOPPED;
            }
        }
    }
    return status;
}

static bool BGEN_SYM(node_scan_desc)(BGEN_NODE *node, bool(*iter)(
    BGEN_ITEM item, void *udata), void *udata)
{
    if (node->isleaf) {
        for (int i = node->len-1; i >= 0; i--) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    if (!BGEN_SYM(node_scan_desc)(node->children[node->len], iter, udata)) {
        return false;
    }
    for (int i = node->len-1; i >= 0; i--) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!BGEN_SYM(node_scan_desc)(node->children[i], iter, udata)) {
            return false;
        }
    }
    return true;
}

static int BGEN_SYM(scan_desc)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item,
    void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(node_scan_desc)(*root, iter, udata)) {
            status = BGEN_STOPPED;
        }
    }
    return status;
}

static bool BGEN_SYM(node_scan_desc_mut)(BGEN_NODE *node, bool(*iter)(
    BGEN_ITEM item, void *udata), void *udata, int *status)
{
    if (node->isleaf) {
        for (int i = node->len-1; i >= 0; i--) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        return true;
    }
    if (!BGEN_SYM(cow)(&node->children[node->len], udata)) {
        *status = BGEN_NOMEM;
        return false;
    }
    if (!BGEN_SYM(node_scan_desc_mut)(node->children[node->len], iter, udata,
        status))
    {
        return false;
    }
    for (int i = node->len-1; i >= 0; i--) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!BGEN_SYM(cow)(&node->children[i], udata)) {
            *status = BGEN_NOMEM;
            return false;
        }
        if (!BGEN_SYM(node_scan_desc_mut)(node->children[i], iter, udata,
            status))
        {
            return false;
        }
    }
    return true;
}

static int BGEN_SYM(scan_desc_mut)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item,
    void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(cow)(root, udata)) {
            return BGEN_NOMEM;
        }
        if (!BGEN_SYM(node_scan_desc_mut)(*root, iter, udata, &status)) {
            if (status == BGEN_FINISHED) {
                status = BGEN_STOPPED;
            }
        }
    }
    return status;
}

static bool BGEN_SYM(node_seek)(BGEN_NODE *node, BGEN_ITEM key,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata, int depth)
{
    int found;
    int i = BGEN_SYM(search)(node, key, udata, &found, depth);
    if (!found) {
        if (!node->isleaf) {
            if (!BGEN_SYM(node_seek)(node->children[i], key, iter, udata, 
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
            if (!BGEN_SYM(node_scan)(node->children[i+1], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static int BGEN_SYM(seek)(BGEN_NODE **root, BGEN_ITEM key, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(node_seek)(*root, key, iter, udata, 0)) {
            status = BGEN_STOPPED;
        }
    }
    return status;
}

static bool BGEN_SYM(node_seek_at)(BGEN_NODE *node, size_t index,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
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
        size_t count = BGEN_SYM(node_count)(node, i);
        if (index <= count) {
            found = index == count;
            break;
        }
        index -= count + 1;
    }
    if (!found) {
        if (!BGEN_SYM(node_seek_at)(node->children[i], index, iter, udata)) {
            return false;
        }
    }
    for (; i < node->len; i++) {
        if (!iter(node->items[i], udata)) {
            return false;
        }
        if (!node->isleaf) {
            if (!BGEN_SYM(node_scan)(node->children[i+1], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static int BGEN_SYM(seek_at)(BGEN_NODE **root, size_t index, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(node_seek_at)(*root, index, iter, udata)) {
            status = BGEN_STOPPED;
        }
    }
    return status;
}

static bool BGEN_SYM(node_seek_at_desc)(BGEN_NODE *node, size_t index,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
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
        size_t count = BGEN_SYM(node_count)(node, i);
        if (index <= count) {
            found = index == count;
            break;
        }
        index -= count + 1;
    }
    if (!found) {
        if (!BGEN_SYM(node_seek_at_desc)(node->children[i], index, iter, 
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
            if (!BGEN_SYM(node_scan_desc)(node->children[i], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static int BGEN_SYM(seek_at_desc)(BGEN_NODE **root, size_t index, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(node_seek_at_desc)(*root, index, iter, udata)) {
            status = BGEN_STOPPED;
        }
    }
    return status;
}

static bool BGEN_SYM(node_seek_at_mut)(BGEN_NODE *node, size_t index,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata, int *status)
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
        size_t count = BGEN_SYM(node_count)(node, i);
        if (index <= count) {
            found = index == count;
            break;
        }
        index -= count + 1;
    }
    if (!found) {
        if (!BGEN_SYM(cow)(&node->children[i], udata)) {
            *status = BGEN_NOMEM;
            return false;
        }
        if (!BGEN_SYM(node_seek_at_mut)(node->children[i], index, iter, udata,
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
            if (!BGEN_SYM(cow)(&node->children[i+1], udata)) {
                *status = BGEN_NOMEM;
                return false;
            }
            if (!BGEN_SYM(node_scan_mut)(node->children[i+1], iter, udata,
                status))
            {
                return false;
            }
        }
    }
    return true;
}


static int BGEN_SYM(seek_at_mut)(BGEN_NODE **root, size_t index, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(cow)(root, udata)) {
            return BGEN_NOMEM;
        }
        if (!BGEN_SYM(node_seek_at_mut)(*root, index, iter, udata, &status)) {
            if (status == BGEN_FINISHED) {
                status = BGEN_STOPPED;
            }
        }
    }
    return status;
}


static bool BGEN_SYM(node_seek_at_desc_mut)(BGEN_NODE *node, size_t index,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata, int *status)
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
        size_t count = BGEN_SYM(node_count)(node, i);
        if (index <= count) {
            found = index == count;
            break;
        }
        index -= count + 1;
    }
    if (!found) {
        if (!BGEN_SYM(cow)(&node->children[i], udata)) {
            *status = BGEN_NOMEM;
            return false;
        }
        if (!BGEN_SYM(node_seek_at_desc_mut)(node->children[i], index, iter, 
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
            if (!BGEN_SYM(cow)(&node->children[i], udata)) {
                *status = BGEN_NOMEM;
                return false;
            }
            if (!BGEN_SYM(node_scan_desc)(node->children[i], iter, udata)) {
                return false;
            }
        }
    }
    return true;
}

static int BGEN_SYM(seek_at_desc_mut)(BGEN_NODE **root, size_t index, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(cow)(root, udata)) {
            return BGEN_NOMEM;
        }
        if (!BGEN_SYM(node_seek_at_desc_mut)(*root, index, iter, udata,
            &status))
        {
            if (status == BGEN_FINISHED) {
                status = BGEN_STOPPED;
            }
        }
    }
    return status;
}

static bool BGEN_SYM(node_seek_mut)(BGEN_NODE *node, BGEN_ITEM key,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata, int depth, 
    int *status) 
{
    int found;
    int i = BGEN_SYM(search)(node, key, udata, &found, depth);
    if (!found) {
        if (!node->isleaf) {
            if (!BGEN_SYM(cow)(&node->children[i], udata)) {
                *status = BGEN_NOMEM;
                return false;
            }
            if (!BGEN_SYM(node_seek_mut)(node->children[i], key, iter, udata,
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
            if (!BGEN_SYM(cow)(&node->children[i+1], udata)) {
                *status = BGEN_NOMEM;
                return false;
            }
            if (!BGEN_SYM(node_scan_mut)(node->children[i+1], iter, udata,
                status))
            {
                return false;
            }
        }
    }
    return true;
}

static int BGEN_SYM(seek_mut)(BGEN_NODE **root, BGEN_ITEM key, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(cow)(root, udata)) {
            return BGEN_NOMEM;
        }
        if (!BGEN_SYM(node_seek_mut)(*root, key, iter, udata, 0, &status)) {
            if (status == BGEN_FINISHED) {
                status = BGEN_STOPPED;
            }
        }
    }
    return status;
}

static bool BGEN_SYM(node_seek_desc)(BGEN_NODE *node, BGEN_ITEM key,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata, int depth) 
{
    int found;
    int i = BGEN_SYM(search)(node, key, udata, &found, depth);
    if (!found) {
        if (!node->isleaf) {
            if (!BGEN_SYM(node_seek_desc)(node->children[i], key, iter, udata,
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
            if (!BGEN_SYM(node_scan_desc)(node->children[i], iter, udata)) {
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

static int BGEN_SYM(seek_desc)(BGEN_NODE **root, BGEN_ITEM key, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(node_seek_desc)(*root, key, iter, udata, 0)) {
            status = BGEN_STOPPED;
        }
    }
    return status;
}

static bool BGEN_SYM(node_seek_desc_mut)(BGEN_NODE *node, BGEN_ITEM key,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata, int depth, 
    int *status) 
{
    int found;
    int i = BGEN_SYM(search)(node, key, udata, &found, depth);
    while(1) {
        if (found) {
            if (!iter(node->items[i], udata)) {
                return false;
            }
        }
        if (!node->isleaf) {
            if (!BGEN_SYM(cow)(&node->children[i], udata)) {
                *status = BGEN_NOMEM;
                return false;
            }
            int ok;
            if (found) {
                ok = BGEN_SYM(node_scan_desc_mut)(node->children[i], iter, 
                    udata, status);
            } else {
                ok = BGEN_SYM(node_seek_desc_mut)(node->children[i], key, iter,
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

static int BGEN_SYM(seek_desc_mut)(BGEN_NODE **root, BGEN_ITEM key, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    int status = BGEN_FINISHED;
    if (*root) {
        if (!BGEN_SYM(cow)(root, udata)) {
            return BGEN_NOMEM;
        }
        if (!BGEN_SYM(node_seek_desc_mut)(*root, key, iter, udata, 0, &status)){
            if (status == BGEN_FINISHED) {
                status = BGEN_STOPPED;
            }
        }
    }
    return status;
}

static void BGEN_SYM(shift_right)(BGEN_NODE *node, int i, int n) {
    BGEN_ASSERT(!BGEN_SYM(shared)(node));
    n--;
    for (int j = node->len; j > i; j--) {
        node->items[j+n] = node->items[j-1];
    }
    node->len++;
    if (!node->isleaf) {
        for (int j = node->len; j > i; j--) {
            node->children[j+n] = node->children[j-1];
#ifdef BGEN_COUNTED
            node->counts[j+n] = node->counts[j-1];
#endif
#ifdef BGEN_SPATIAL
            node->rects[j+n] = node->rects[j-1];
#endif
        }
    }
}

static int BGEN_SYM(index_of)(BGEN_NODE **root, BGEN_ITEM key,
    size_t *index_out, void *udata)
{
#ifdef BGEN_NOORDER
    (void)root, (void)key, (void)index_out, (void)udata;
    return BGEN_UNSUPPORTED;
#else
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    int depth = 0;
    size_t index = 0;
    BGEN_NODE *node = *root;
    while (1) {
        int i, found;
        i = BGEN_SYM(search)(node, key, udata, &found, depth);
        index += (size_t)i;
        if (!node->isleaf) {
            for (int j = 0; j < i; j++) {
                index += BGEN_SYM(node_count)(node, j);
            }
            if (found) {
                index += BGEN_SYM(node_count)(node, i);
            }
        }
        if (found) {
            if (index_out) {
                *index_out = index;
            }
            return BGEN_FOUND;
        } else if (node->isleaf) {
            return BGEN_NOTFOUND;
        }
        node = node->children[i];
        depth++;
    }
#endif
}

// returns FOUND or NOTFOUND
static int BGEN_SYM(get)(BGEN_NODE **root, BGEN_ITEM key, BGEN_ITEM *item_out,
    void *udata)
{
#ifdef BGEN_NOORDER
    (void)root, (void)key, (void)item_out, (void)udata;
    return BGEN_UNSUPPORTED;
#else
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    BGEN_NODE *node = *root;
    int depth = 0;
    while (1) {
        int i, found;
        i = BGEN_SYM(search)(node, key, udata, &found, depth);
        if (found) {
            if (item_out) {
                *item_out = node->items[i];
            }
            return BGEN_FOUND;
        } else if (node->isleaf) {
            return BGEN_NOTFOUND;
        }
        node = node->children[i];
        depth++;
    }
#endif
}

// Work like (get) but, if needed, performs copy-on-write operations.
// returns FOUND or NOTFOUND or NOMEM
static int BGEN_SYM(get_mut)(BGEN_NODE **root, BGEN_ITEM key, 
    BGEN_ITEM *item_out, void *udata)
{
#ifdef BGEN_NOORDER
    (void)root, (void)key, (void)item_out, (void)udata;
    return BGEN_UNSUPPORTED;
#else
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    int depth = 0;
    BGEN_NODE *node = *root;
    while (1) {
        BGEN_ASSERT(!BGEN_SYM(shared)(node));
        int i, found;
        i = BGEN_SYM(search)(node, key, udata, &found, depth);
        if (found) {
            if (item_out) {
                *item_out = node->items[i];
            }
            return BGEN_FOUND;
        } else if (node->isleaf) {
            return BGEN_NOTFOUND;
        }
        if (!BGEN_SYM(cow)(&node->children[i], udata)) {
            return BGEN_NOMEM;
        }
        node = node->children[i];
        depth++;
    }
#endif
}

// returns true if key is found
static bool BGEN_SYM(contains)(BGEN_NODE **root, BGEN_ITEM key, void *udata) {
    return BGEN_SYM(get)(root, key, 0, udata) == BGEN_FOUND;
}

static BGEN_NODE *BGEN_SYM(split)(BGEN_NODE *left, BGEN_ITEM *mitem, 
    void *udata)
{
    (void)udata;
    BGEN_NODE *right = BGEN_SYM(alloc_node)(left->isleaf);
    if (!right) {
        return 0;
    }
    int mid = BGEN_MAXITEMS / 2;
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
#ifdef BGEN_COUNTED
        for (int i = 0; i <= right->len; i++) {
            right->counts[i] = left->counts[mid+1+i];
        }
#endif
#ifdef BGEN_SPATIAL
        for (int i = 0; i <= right->len; i++) {
            right->rects[i] = left->rects[mid+1+i];
        }
        left->rects[left->len] = BGEN_SYM(rect_calc)(left, left->len, udata);
#endif
    }
    return right;
}

static bool BGEN_SYM(split_root)(BGEN_NODE **root, void *udata) {
    (void)udata;
    BGEN_ASSERT(!BGEN_SYM(shared)(*root));
    BGEN_NODE *newroot = BGEN_SYM(alloc_node)(0);
    if (!newroot) {
        return false;
    }
    newroot->len = 1;
    newroot->height = (*root)->height+1;
    newroot->children[0] = *root;
    newroot->children[1] = BGEN_SYM(split)(*root, &newroot->items[0], udata);
    if (!newroot->children[1]) {
        BGEN_FREE(newroot);
        return false;
    }
#ifdef BGEN_COUNTED
    newroot->counts[0] = BGEN_SYM(count0)(newroot->children[0]);
    newroot->counts[1] = BGEN_SYM(count0)(newroot->children[1]);
#endif
#ifdef BGEN_SPATIAL
    newroot->rects[0] = BGEN_SYM(rect_calc)(newroot, 0, udata);
    newroot->rects[1] = BGEN_SYM(rect_calc)(newroot, 1, udata);
#endif
    *root = newroot;
    return true;
}


static bool BGEN_SYM(split_child_at)(BGEN_NODE *node, int i, void *udata) {
    (void)udata;
    BGEN_ASSERT(!BGEN_SYM(shared)(node));
    BGEN_ITEM mitem;
    BGEN_NODE *right = BGEN_SYM(split)(node->children[i], &mitem, udata);
    if (!right) {
        return false;
    }
    BGEN_SYM(shift_right)(node, i, 1);
    node->items[i] = mitem;
    node->children[i+1] = right;
#ifdef BGEN_COUNTED
    node->counts[i] = BGEN_SYM(count0)(node->children[i]);
    node->counts[i+1] = BGEN_SYM(count0)(node->children[i+1]);
#endif
#ifdef BGEN_SPATIAL
    node->rects[i] = BGEN_SYM(rect_calc)(node, i, udata);
    node->rects[i+1] = BGEN_SYM(rect_calc)(node, i+1, udata);
#endif
    return true;
}

static void BGEN_SYM(give_left)(BGEN_NODE *node, int index, bool balance) {
    // This will give items from right to left.
    // node->children[i]) to node->children[i-1].
    // It's expected that the children are leaves and that both are cow'd and
    // the right child index > 0.
    // These checks must be done already, prior to calling this function.

    BGEN_NODE *left = node->children[index-1];
    BGEN_NODE *right = node->children[index];

    BGEN_ASSERT(!BGEN_SYM(shared)(node));
    BGEN_ASSERT(!BGEN_SYM(shared)(left));
    BGEN_ASSERT(!BGEN_SYM(shared)(right));
    
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
#ifdef BGEN_COUNTED
    node->counts[index-1] = left->len;
    node->counts[index] = right->len;
#endif

}

static void BGEN_SYM(give_right)(BGEN_NODE *node, int index, bool balance) {
    // This will give items from left to right. 
    // node->children[i]) to node->children[i+1].
    // It's expected that the children are leaves and that both are cow'd and
    // the left child index < node->len.
    // These checks must be done already, prior to calling this function.
    BGEN_NODE *left = node->children[index];
    BGEN_NODE *right = node->children[index+1];
    
    BGEN_ASSERT(!BGEN_SYM(shared)(node));
    BGEN_ASSERT(!BGEN_SYM(shared)(left));
    BGEN_ASSERT(!BGEN_SYM(shared)(right));
    
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

#ifdef BGEN_COUNTED
    node->counts[index] = left->len;
    node->counts[index+1] = right->len;
#endif
}

#define BGEN_MUSTSPLIT 9999
#define BGEN_INSITEM      0
#define BGEN_INSAT        1
#define BGEN_REPAT        2
#define BGEN_PUSHFRONT    3
#define BGEN_PUSHBACK     4

static int BGEN_SYM(insert1)(BGEN_NODE *node, int act, size_t index, 
    BGEN_ITEM item, BGEN_ITEM *olditem, void *udata, int depth)
{
    BGEN_ASSERT(!BGEN_SYM(shared)(node));
    size_t oindex;
    int found, i;
retry:
    switch (act) {
    case BGEN_INSITEM:
        i = BGEN_SYM(search)(node, item, udata, &found, depth);
        break;
    case BGEN_INSAT: 
    case BGEN_REPAT:
        oindex = index;
        found = 0;
        if (node->isleaf) {
            if (index > (size_t)node->len || (index == (size_t)node->len &&
                act == BGEN_REPAT))
            {
                // return NOTFOUND when index is out of bounds
                return BGEN_NOTFOUND;
            }
            i = index;
            found = 1;
        } else {
            i = 0;
            for (; i < node->len; i++) {
                size_t count = BGEN_SYM(node_count)(node, i);
                if (index <= count) {
                    found = index == count;
                    break;
                }
                index -= count + 1;
            }
        }
#ifndef BGEN_NOORDER
        // Check order. 
        if (act == BGEN_REPAT && !node->isleaf && found) {
            // Get the previous and next items
            BGEN_NODE *child;
            child = node->children[i];
            while (1) {
                if (child->isleaf) {
                    if (!BGEN_SYM(less)(child->items[child->len-1], item, 
                        udata))
                    {
                        return BGEN_OUTOFORDER;
                    }
                    break;
                }
                child = child->children[child->len];
            }
            child = node->children[i+1];
            while (1) {
                if (child->isleaf) {
                    if (!BGEN_SYM(less)(item, child->items[0], udata)) {
                       return BGEN_OUTOFORDER;
                    }
                    break;
                }
                child = child->children[0];
            }
        } else {
            int i0 = i-1;
            int i1 = act == BGEN_REPAT && node->isleaf ? i+1 : i;
            if (i0 >= 0) {
                if (!BGEN_SYM(less)(node->items[i0], item, udata)) {
                return BGEN_OUTOFORDER;
                }
            }
            if (i1 < node->len) {
                if (!BGEN_SYM(less)(item, node->items[i1], udata)) {
                return BGEN_OUTOFORDER;
                }
            }
        }
#endif
        found = act == BGEN_INSAT ? 0 : found;
        break;
    case BGEN_PUSHFRONT:
        i = 0;
        found = 0;
        if (node->isleaf) {
#ifndef BGEN_NOORDER
            // check order
            if (!BGEN_SYM(less)(item, node->items[0], udata)) {
                return BGEN_OUTOFORDER;
            }
#endif
            goto isleaf;
        }
        goto isbranch;
    case BGEN_PUSHBACK:
        i = node->len;
        found = 0;
        if (node->isleaf) {
#ifndef BGEN_NOORDER
            // check order
            if (!BGEN_SYM(less)(node->items[node->len-1], item, udata)) {
                return BGEN_OUTOFORDER;
            }
#endif
            goto isleaf;
        }
        goto isbranch;
    }
    while (1) {
        BGEN_ASSERT(i >= 0 && i <= node->len);
        if (found) {
            if (olditem) {
                *olditem = node->items[i];
            }
            node->items[i] = item;
#ifdef BGEN_SPATIAL
            if (!node->isleaf) {
                // Must also update the owning rectangle
                node->rects[i] = BGEN_SYM(rect_calc)(node, i, udata);
            }
#endif
            return BGEN_REPLACED;
        }
        if (node->isleaf) {
        isleaf:
            if (node->len == BGEN_MAXITEMS) {
                return BGEN_MUSTSPLIT;
            }
            BGEN_SYM(shift_right)(node, i, 1);
            node->items[i] = item;
            return BGEN_INSERTED;
        }
    isbranch:
        if (!BGEN_SYM(cow)(&node->children[i], udata)) {
            return BGEN_NOMEM;
        }
        int ret = BGEN_SYM(insert1)(node->children[i], act, index, item,
            olditem, udata, depth+1);
        if (ret != BGEN_MUSTSPLIT || node->len == BGEN_MAXITEMS) {
            if (ret == BGEN_INSERTED) {
#ifdef BGEN_COUNTED
                node->counts[i]++;
#endif
#ifdef BGEN_SPATIAL
                // Expand the rectangle on insert
                node->rects[i] = BGEN_SYM(rect_join)(node->rects[i], 
                    BGEN_SYM(item_rect)(item, udata));
            } else if (ret == BGEN_REPLACED) {
                // Recalculate the rectangle
                node->rects[i] = BGEN_SYM(rect_calc)(node, i, udata);
#endif
            }
            return ret;
        }
        if (!BGEN_SYM(split_child_at)(node, i, udata)) {
            return BGEN_NOMEM;
        }
        if (act == BGEN_INSITEM) {
            int cmp = BGEN_SYM(compare)(item, node->items[i], udata);
            if (cmp <= 0) {
                found = cmp == 0;
            } else {
                i++;
            }
        } else {
            if (act == BGEN_INSAT) {
                // revert the index
                index = oindex;
            }
            goto retry;
        }
    }
}

static int BGEN_SYM(insert0)(BGEN_NODE **root, int act, size_t index, 
    BGEN_ITEM item, BGEN_ITEM *olditem, void *udata)
{
    if (!*root) {
        if (act == BGEN_REPAT || (act == BGEN_INSAT && index > 0)) {
            return BGEN_NOTFOUND;
        }
        *root = BGEN_SYM(alloc_node)(1);
        if (!*root) {
            return BGEN_NOMEM;
        }
        (*root)->items[0] = item;
        (*root)->len = 1;
        (*root)->height = 1;
        return BGEN_INSERTED;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    while (1) {
        int ret = BGEN_SYM(insert1)(*root, act, index, item, olditem, udata, 0);
        if (ret != BGEN_MUSTSPLIT) {
            return ret;
        }
        if (!BGEN_SYM(split_root)(root, udata)) {
            return BGEN_NOMEM;
        }
    }
}

#ifndef BGEN_NOORDER
static int BGEN_SYM(insert_fastpath)(BGEN_NODE **root, BGEN_ITEM item,
    BGEN_ITEM *olditem, void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    int ret = 0;
    int cidx = 0;
    BGEN_NODE *parent = 0;
    BGEN_NODE *node = *root;
    int depth = 0;
#if defined(BGEN_COUNTED) || defined(BGEN_SPATIAL)
    short path[BGEN_MAXHEIGHT];
#endif
#ifdef BGEN_SPATIAL
    BGEN_RECT irect = BGEN_SYM(item_rect)(item, udata);
    BGEN_RECT rects[BGEN_MAXHEIGHT];
#endif
    while (1) {
        BGEN_ASSERT(!BGEN_SYM(shared)(node));
        int found;
        int i = BGEN_SYM(search)(node, item, udata, &found, depth);
        if (found) {
            if (olditem) {
                *olditem = node->items[i];
            }
            node->items[i] = item;
            ret = BGEN_REPLACED;
            break;
        }
        if (node->isleaf) {
            if (node->len == BGEN_MAXITEMS) {
                if (!parent || parent->len == BGEN_MAXITEMS) {
                    break;
                }
                // Use the standard splitting algorithm
#if defined(BGEN_COUNTED) || defined(BGEN_SPATIAL)
#ifdef BGEN_COUNTED
                parent->counts[cidx]--;
#endif
#ifdef BGEN_SPATIAL
                parent->rects[cidx] = rects[depth-1];
#endif
#endif
                depth--;
                node = parent;
                i = cidx;
                if (i > 0 && node->children[i-1]->len < BGEN_MINITEMS+1) {
                    // In the case the the node to left has lots of room then
                    // just give a bunch of items to it rather than split.
                    if (!BGEN_SYM(cow)(&node->children[i-1], udata)) {
                        ret = BGEN_NOMEM;
                        break;
                    }
                    BGEN_SYM(give_left)(node, i, false);
#ifdef BGEN_SPATIAL
                    node->rects[i-1] = BGEN_SYM(rect_calc)(node, i-1, udata);
                    node->rects[i] = BGEN_SYM(rect_calc)(node, i, udata);
#endif
                    continue;
                }
                if (!BGEN_SYM(split_child_at)(node, i, udata)) {
                    ret = BGEN_NOMEM;
                    break;
                }
                int cmp = BGEN_SYM(compare)(item, node->items[i], udata);
                i += cmp > 0;
            } else {
                BGEN_SYM(shift_right)(node, i, 1);
                node->items[i] = item;
                return BGEN_INSERTED;
            }
        }
#if defined(BGEN_COUNTED) || defined(BGEN_SPATIAL)
        path[depth] = i;
#ifdef BGEN_COUNTED
        node->counts[i]++;
#endif
#ifdef BGEN_SPATIAL
        rects[depth] = node->rects[i];
        node->rects[i] = BGEN_SYM(rect_join)(node->rects[i], irect);
#endif
#endif
        depth++;
        cidx = i;
        parent = node;
        if (!BGEN_SYM(cow)(&node->children[i], udata)) {
            ret = BGEN_NOMEM;
            break;
        }
        node = node->children[i];
    }
#if defined(BGEN_COUNTED) || defined(BGEN_SPATIAL)
    node = *root;
    for (int i = 0; i < depth; i++) {
        int j = path[i];
#ifdef BGEN_COUNTED
        node->counts[j]--;
#endif
#ifdef BGEN_SPATIAL
        node->rects[j] = rects[i];
#endif
        node = node->children[j];
    }
#endif
    return ret;
}
#endif

// returns INSERTED, REPLACED, or NOMEM
static int BGEN_SYM(insert)(BGEN_NODE **root, BGEN_ITEM item,
    BGEN_ITEM *olditem, void *udata)
{
#ifdef BGEN_NOORDER
    (void)root, (void)item, (void)olditem, (void)udata;
    return BGEN_UNSUPPORTED;
#else
    int ret = BGEN_SYM(insert_fastpath)(root, item, olditem, udata);
    if (ret) {
        return ret;
    }
    return BGEN_SYM(insert0)(root, BGEN_INSITEM, 0, item, olditem, udata);
#endif
}

static void BGEN_SYM(shift_left)(BGEN_NODE *node, int i, int n, bool for_merge){
    BGEN_ASSERT(!BGEN_SYM(shared)(node));
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
#ifdef BGEN_COUNTED
        for (int j = i; j < node->len; j++) {
            node->counts[j+n] = node->counts[j+1];
        }
#endif
#ifdef BGEN_SPATIAL
        for (int j = i; j < node->len; j++) {
            node->rects[j+n] = node->rects[j+1];
        }
#endif
    }
    node->len--;
}

static void BGEN_SYM(join)(BGEN_NODE *left, BGEN_NODE *right, void *udata) {
    (void)udata;
    BGEN_ASSERT(!BGEN_SYM(shared)(left));
    BGEN_ASSERT(!BGEN_SYM(shared)(right));
    for (int i = 0; i < right->len; i++) {
        left->items[left->len+i] = right->items[i];
    }
    if (!left->isleaf) {
        for (int i = 0; i <= right->len; i++) {
            left->children[left->len+i] = right->children[i];
        }
#ifdef BGEN_COUNTED
        for (int i = 0; i <= right->len; i++) {
            left->counts[left->len+i] = right->counts[i];
        }
#endif
#ifdef BGEN_SPATIAL
        for (int i = 0; i <= right->len; i++) {
            left->rects[left->len+i] = right->rects[i];
        }
        left->rects[left->len-1] = 
            BGEN_SYM(rect_calc)(left, left->len-1, udata);
#endif
    }
    left->len += right->len;
}

static void BGEN_SYM(rebalance)(BGEN_NODE *node, int i, void *udata) {
    BGEN_ASSERT(!BGEN_SYM(shared)(node));

    if (i == node->len) {
        i--;
    }

    BGEN_NODE *left = node->children[i];
    BGEN_NODE *right = node->children[i+1];

    BGEN_ASSERT(!BGEN_SYM(shared)(left));
    BGEN_ASSERT(!BGEN_SYM(shared)(right));

    if (left->len + right->len < BGEN_MAXITEMS) {
        // merge (left,item,right)

        // Merges the left and right children nodes together as a single node
        // that includes (left,item,right), and places the contents into the
        // existing left node. Delete the right node altogether and move the
        // following items and child nodes to the left by one slot.
        left->items[left->len] = node->items[i];
        left->len++;
        BGEN_SYM(join)(left, right, udata);
#ifdef BGEN_COUNTED
        size_t count = node->counts[i] + 1 + node->counts[i+1];
#endif
        BGEN_FREE(right);
        BGEN_SYM(shift_left)(node, i, 1, true);
#ifdef BGEN_COUNTED
        node->counts[i] = count;
#endif
#ifdef BGEN_SPATIAL
        node->rects[i] = BGEN_SYM(rect_calc)(node, i, udata);
        
#endif
        return;
    }
    if (left->isleaf) {
        // For leaves only.
        // Move as many items from one leaf to another to create balance.
        if (left->len < right->len) {
            BGEN_SYM(give_left)(node, i+1, true);
        } else {
            BGEN_SYM(give_right)(node, i, true);
        }
    } else {
        // For branches only.
        // Shift items and children over by one.
        if (left->len < right->len) {
            // move right to left
            left->items[left->len] = node->items[i];
            left->children[left->len+1] = right->children[0];
    #ifdef BGEN_COUNTED
            left->counts[left->len+1] = right->counts[0];
    #endif
            left->len++;
            node->items[i] = right->items[0];
            BGEN_SYM(shift_left)(right, 0, 1, false);
    #ifdef BGEN_SPATIAL
            left->rects[left->len-1] = BGEN_SYM(rect_calc)(left, left->len-1, 
                udata);
            left->rects[left->len] = BGEN_SYM(rect_calc)(left, left->len, 
                udata);
    #endif
        } else {
            // move left to right
            BGEN_SYM(shift_right)(right, 0, 1);
            right->items[0] = node->items[i];
            right->children[0] = left->children[left->len];
    #ifdef BGEN_COUNTED
            right->counts[0] = left->counts[left->len];
    #endif
            node->items[i] = left->items[left->len-1];
            left->len--;
    #ifdef BGEN_SPATIAL
            right->rects[0] = BGEN_SYM(rect_calc)(right, 0, udata);
            left->rects[left->len] = BGEN_SYM(rect_calc)(left, left->len, 
                udata);
        #endif
        }
    }
#ifdef BGEN_COUNTED
    node->counts[i] = BGEN_SYM(count0)(node->children[i]);
    node->counts[i+1] = BGEN_SYM(count0)(node->children[i+1]);
#endif
#ifdef BGEN_SPATIAL
    node->rects[i] = BGEN_SYM(rect_calc)(node, i, udata);
    node->rects[i+1] = BGEN_SYM(rect_calc)(node, i+1, udata);
#endif

}

// deletion actions
#define BGEN_DELKEY   0
#define BGEN_POPMAX   1
#define BGEN_POPFRONT 2
#define BGEN_POPBACK  3
#define BGEN_DELAT    4

static int BGEN_SYM(delete1)(BGEN_NODE *node, int act, BGEN_ITEM key,
    size_t index, void *udata, int depth, BGEN_ITEM *prev)
{
    BGEN_ASSERT(!BGEN_SYM(shared)(node));
    int i = 0;
    int found = 0;
    switch (act) {
    case BGEN_DELKEY:
        i = BGEN_SYM(search)(node, key, udata, &found, depth);
        break;
    case BGEN_POPMAX:
        i = node->isleaf ? node->len-1 : node->len;
        found = 1;
        break;
    case BGEN_POPFRONT:
        i = 0;
        found = node->isleaf;
        break;
    case BGEN_POPBACK:
        i = node->isleaf ? node->len-1 : node->len;
        found = node->isleaf;
        break;
    case BGEN_DELAT:
        if (node->isleaf) {
            if (index < (size_t)node->len) {
                i = index;
                found = 1;
            }
        } else {
            for (; i < node->len; i++) {
                size_t count = BGEN_SYM(node_count)(node, i);
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
            BGEN_SYM(shift_left)(node, i, 1, false);
            return BGEN_DELETED;
        }
        return BGEN_NOTFOUND;
    }

    // Cow the target child node.
    if (!BGEN_SYM(cow)(&node->children[i], udata)) {
        return BGEN_NOMEM;
    }
    if (node->children[i]->len == BGEN_MINITEMS) {
        // There's a good chance that this node will need to be rebalanced at
        // the end of this operation. Make sure that both child nodes that will
        // be used in the rebalancing are cow'd _before_ traversing further.
        if (i == node->len) {
            if (!BGEN_SYM(cow)(&node->children[i-1], udata)) {
                return BGEN_NOMEM;
            }
        } else {
            if (!BGEN_SYM(cow)(&node->children[i+1], udata)) {
                return BGEN_NOMEM;
            }
        }
    }
    if (found) {
        if (act == BGEN_POPMAX) {
            // Popping off the max item into into its parent branch to maintain
            // a balanced tree.
            act = BGEN_POPMAX;
        } else {
            // Item was found in branch, copy its contents, delete it, and 
            // begin popping off the max items in child nodes.
            *prev = node->items[i];
            prev = &node->items[i];
            act = BGEN_POPMAX;
        }
    }
    int ret = BGEN_SYM(delete1)(node->children[i], act, key, index, udata,
        depth+1, prev);
    if (ret != BGEN_DELETED) {
        return ret;
    }
#ifdef BGEN_COUNTED
    node->counts[i]--;
#endif
#ifdef BGEN_SPATIAL
    BGEN_RECT rect = BGEN_SYM(item_rect)(*prev, udata);
    if (act == BGEN_POPMAX || BGEN_SYM(rect_onedge)(rect, node->rects[i])) {
        node->rects[i] = BGEN_SYM(rect_calc)(node, i, udata);
    }
#endif
    if (node->children[i]->len < BGEN_MINITEMS) {
        BGEN_SYM(rebalance)(node, i, udata);
    }
    return BGEN_DELETED;
}

static int BGEN_SYM(delete0)(BGEN_NODE **root, int act, BGEN_ITEM key,
    size_t index, void *udata, BGEN_ITEM *olditem)
{
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    int ret = BGEN_SYM(delete1)(*root, act, key, index, udata, 0, olditem);
    if (ret != BGEN_DELETED) {
        return ret;
    }
    if ((*root)->len == 0) {
        BGEN_NODE *old_root = *root;
        *root = (*root)->isleaf ? 0 : (*root)->children[0];
        BGEN_FREE(old_root);
    }
    return BGEN_DELETED;
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
// COUNTS and SPATIAL rectangles. As long as those are features of the
// tree.
#if !defined(BGEN_SPATIAL) && !defined(BGEN_NOORDER)
static int BGEN_SYM(delete_fastpath)(BGEN_NODE **root, BGEN_ITEM key,
    BGEN_ITEM *olditem, void *udata)
{
    if (!*root || (*root)->isleaf) {
        // Continue using the standard path operation for small trees with no
        // child nodes.
        return 0;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    int ret = 0;
    int depth = 0;
    short path[BGEN_MAXHEIGHT];
    BGEN_NODE *parent = 0;
    BGEN_NODE *node = *root;
    while (1) {
        int found = 0;
        int i = BGEN_SYM(search)(node, key, udata, &found, depth);
        if (node->isleaf) {
            if (!found) {
                ret = BGEN_NOTFOUND;
                break;
            }
            bool take = false;
            bool merge = false;
            int from = 0;
            int ci = path[depth-1];
            if (node->len == BGEN_MINITEMS) {
                // Deleting will cause node to have too few items. 
                BGEN_ASSERT(parent);
                if (ci > 0 && parent->children[ci-1]->len > BGEN_MINITEMS) {
                    take = true;
                    from = -1;

                } else if (ci == 0 && parent->children[ci+1]->len > 
                    BGEN_MINITEMS)
                {
                    take = true;
                    from = +1;
                } else {
                    // Both left and right siblings are at min capacity
                    if (parent->len > BGEN_MINITEMS) {
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
                    if (!BGEN_SYM(cow)(&parent->children[ci+from], udata)) {
                        ret = BGEN_NOMEM;
                        break;
                    }
                }
            }
            if (olditem) {
                *olditem = node->items[i];
            }
            BGEN_SYM(shift_left)(node, i, 1, false);
            if (take) {
                if (!merge) {
                    if (from == -1) {
                        BGEN_SYM(give_right)(parent, ci-1, true);
                    } else {
                        BGEN_SYM(give_left)(parent, ci+1, true);
                    }
                } else {
                    i = ci;
                    if (from == -1) {
                        i--;
                    }
                    BGEN_NODE *left = parent->children[i];
                    BGEN_NODE *right = parent->children[i+1];
                    left->items[left->len] = parent->items[i];
                    left->len++;
                    BGEN_SYM(join)(left, right, udata);
            #ifdef BGEN_COUNTED
                    size_t count = parent->counts[i] + 1 + parent->counts[i+1];
            #endif
                    BGEN_FREE(right);
                    BGEN_SYM(shift_left)(parent, i, 1, true);
            #ifdef BGEN_COUNTED
                    parent->counts[i] = count;
            #endif
                }
            }
            return BGEN_DELETED;
        }
        // branch
        if (found) {
            if (node->height == 2) {
                if (node->children[i]->len > BGEN_MINITEMS) {
                    if (!BGEN_SYM(cow)(&node->children[i], udata)) {
                        ret = BGEN_NOMEM;
                        break;
                    }
                    BGEN_NODE *child = node->children[i];
                    if (olditem) {
                        *olditem = node->items[i];
                    }
                    node->items[i] = child->items[child->len-1];
                    child->len--;
            #ifdef BGEN_COUNTED
                    node->counts[i]--;
            #endif
                } else if (node->children[i+1]->len > BGEN_MINITEMS) {
                    if (!BGEN_SYM(cow)(&node->children[i+1], udata)) {
                        ret = BGEN_NOMEM;
                        break;
                    }
                    BGEN_NODE *child = node->children[i+1];
                    if (olditem) {
                        *olditem = node->items[i];
                    }
                    node->items[i] = child->items[0];
                    child->len--;
                    for (int j = 0; j < child->len; j++) {
                        child->items[j] = child->items[j+1];
                    }
            #ifdef BGEN_COUNTED
                    node->counts[i+1]--;
            #endif
                } else {
                    break;
                }
                return BGEN_DELETED;
            } else {
                break;
            }
        }
        if (!BGEN_SYM(cow)(&node->children[i], udata)) {
            ret = BGEN_NOMEM;
            break;
        }
        path[depth++] = i;
#ifdef BGEN_COUNTED
        node->counts[i]--;
#endif
        parent = node;
        node = node->children[i];
    }
#ifdef BGEN_COUNTED
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
static int BGEN_SYM(delete)(BGEN_NODE **root, BGEN_ITEM key, BGEN_ITEM *olditem, 
    void *udata)
{
#ifdef BGEN_NOORDER
    (void)root, (void)key, (void)olditem, (void)udata;
    return BGEN_UNSUPPORTED;
#else
    int ret;
#ifndef BGEN_SPATIAL
    ret = BGEN_SYM(delete_fastpath)(root, key, olditem, udata);
    if (ret) {
        return ret;
    }
#endif
    BGEN_ITEM spare;
    ret = BGEN_SYM(delete0)(root, BGEN_DELKEY, key, 0, udata, &spare);
    if (ret != BGEN_DELETED) {
        return ret;
    }
    if (olditem) {
        *olditem = spare;
    }
    return BGEN_DELETED;
#endif
}


// returns FOUND or NOTFOUND
static int BGEN_SYM(front)(BGEN_NODE **root, BGEN_ITEM *item_out, void *udata) {
    (void)udata;
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    BGEN_NODE *node = *root;
    while (1) {
        if (node->isleaf) {
            if (item_out) {
                *item_out = node->items[0];
            }
            return BGEN_FOUND;
        } 
        node = node->children[0];
    }
}

static int BGEN_SYM(front_mut)(BGEN_NODE **root, BGEN_ITEM *item_out,
    void *udata)
{
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    BGEN_NODE *node = *root;
    while (1) {
        BGEN_ASSERT(!BGEN_SYM(shared)(node));
        if (node->isleaf) {
            if (item_out) {
                *item_out = node->items[0];
            }
            return BGEN_FOUND;
        } 
        if (!BGEN_SYM(cow)(&node->children[0], udata)) {
            return BGEN_NOMEM;
        }
        node = node->children[0];
    }
}

// returns FOUND or NOTFOUND
static int BGEN_SYM(back)(BGEN_NODE **root, BGEN_ITEM *item_out, void *udata) {
    (void)udata;
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    BGEN_NODE *node = *root;
    while (1) {
        if (node->isleaf) {
            if (item_out) {
                *item_out = node->items[node->len-1];
            }
            return BGEN_FOUND;
        } 
        node = node->children[node->len];
    }
}

// returns FOUND or NOTFOUND or NOMEM
static int BGEN_SYM(back_mut)(BGEN_NODE **root, BGEN_ITEM *item_out,
    void *udata)
{
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    BGEN_NODE *node = *root;
    while (1) {
        BGEN_ASSERT(!BGEN_SYM(shared)(node));
        if (node->isleaf) {
            if (item_out) {
                *item_out = node->items[node->len-1];
            }
            return BGEN_FOUND;
        } 
        if (!BGEN_SYM(cow)(&node->children[node->len], udata)) {
            return BGEN_NOMEM;
        }
        node = node->children[node->len];
    }
}

// returns FOUND or NOTFOUND
static int BGEN_SYM(get_at)(BGEN_NODE **root, size_t index, BGEN_ITEM *item,
    void *udata)
{
    (void)udata;
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    BGEN_NODE *node = *root;
    while (1) {
        if (node->isleaf) {
            if (index >= (size_t)node->len) {
                return BGEN_NOTFOUND;
            }
            if (item) {
                *item = node->items[index];
            }
            return BGEN_FOUND;
        }
        int i = 0;
        for (; i < node->len; i++) {
            size_t count = BGEN_SYM(node_count)(node, i);
            if (index < count) {
                break;
            }
            if (index == count) {
                if (item) {
                    *item = node->items[i];
                }
                return BGEN_FOUND;
            }
            index -= count + 1;
        }
        node = node->children[i];
    }
}

static int BGEN_SYM(get_at_mut)(BGEN_NODE **root, size_t index, BGEN_ITEM *item,
    void *udata)
{
    if (!*root) {
        return BGEN_NOTFOUND;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    BGEN_NODE *node = *root;
    while (1) {
        BGEN_ASSERT(!BGEN_SYM(shared)(node));
        if (node->isleaf) {
            if (index >= (size_t)node->len) {
                return BGEN_NOTFOUND;
            }
            if (item) {
                *item = node->items[index];
            }
            return BGEN_FOUND;
        }
        int i = 0;
        for (; i < node->len; i++) {
            size_t count = BGEN_SYM(node_count)(node, i);
            if (index < count) {
                break;
            }
            if (index == count) {
                if (item) {
                    *item = node->items[i];
                }
                return BGEN_FOUND;
            }
            index -= count + 1;
        }
        if (!BGEN_SYM(cow)(&node->children[i], udata)) {
            return BGEN_NOMEM;
        }
        node = node->children[i];
    }
}

static int BGEN_SYM(delete_at)(BGEN_NODE **root, size_t index, 
    BGEN_ITEM *olditem, void *udata)
{
    BGEN_ITEM spare = { 0 };
    int ret = BGEN_SYM(delete0)(root, BGEN_DELAT, spare, index, udata, &spare);
    if (ret != BGEN_DELETED) {
        return ret;
    }
    if (olditem) {
        *olditem = spare;
    }
    return BGEN_DELETED;
}

static int BGEN_SYM(replace_at)(BGEN_NODE **root, size_t index, BGEN_ITEM item,
    BGEN_ITEM *olditem, void *udata)
{
    return BGEN_SYM(insert0)(root, BGEN_REPAT, index, item, olditem, udata);
}

#ifndef BGEN_SPATIAL
static int BGEN_SYM(pop_front_fastpath)(BGEN_NODE **root, BGEN_ITEM *olditem,
    void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    BGEN_NODE *node = *root;
    BGEN_NODE *parent = 0;
#ifdef BGEN_COUNTED
    int depth = 0;
#endif
    int ret = 0;
    while (1) {
        BGEN_ASSERT(!BGEN_SYM(shared)(node));
        if (node->isleaf) {
            if (node->len <= BGEN_MINITEMS) {
                if (parent && parent->children[1]->len > BGEN_MINITEMS+1) {
                    if (!BGEN_SYM(cow)(&parent->children[1], udata)) {
                        ret = BGEN_NOMEM;
                        break;
                    }
                    BGEN_SYM(give_left)(parent, 1, true);
#ifdef BGEN_COUNTED
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
            return BGEN_DELETED;
        }
#ifdef BGEN_COUNTED
        node->counts[0]--;
        depth++;
#endif
        parent = node;
        if (!BGEN_SYM(cow)(&node->children[0], udata)) {
            ret = BGEN_NOMEM;
            break;
        }
        node = node->children[0];
    }
#ifdef BGEN_COUNTED
    node = *root;
    for (int i = 0; i < depth; i++) {
        node->counts[0]++;
        node = node->children[0];
    }
#endif
    return ret;
}
#endif

static int BGEN_SYM(pop_front)(BGEN_NODE **root, BGEN_ITEM *olditem,
    void *udata)
{
    int ret;
#ifndef BGEN_SPATIAL
    ret = BGEN_SYM(pop_front_fastpath)(root, olditem, udata);
    if (ret) {
        return ret;
    }
#endif
    BGEN_ITEM spare = { 0 };
    ret = BGEN_SYM(delete0)(root, BGEN_POPFRONT, spare, 0, udata, 
        &spare);
    if (ret == BGEN_DELETED) {
        if (olditem) {
            *olditem = spare;
        }
    }
    return ret;
}

#ifndef BGEN_SPATIAL
static int BGEN_SYM(pop_back_fastpath)(BGEN_NODE **root, BGEN_ITEM *olditem,
    void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    BGEN_NODE *parent = 0;
    BGEN_NODE *node = *root;
#ifdef BGEN_COUNTED
    int depth = 0;
#endif
    int ret = 0;
    while (1) {
        BGEN_ASSERT(!BGEN_SYM(shared)(node));
        if (node->isleaf) {
            if (node->len <= BGEN_MINITEMS) {
                if (parent && 
                    parent->children[parent->len-1]->len > BGEN_MINITEMS+1)
                {
                    if (!BGEN_SYM(cow)(&parent->children[parent->len-1],
                        udata))
                    {
                        ret = BGEN_NOMEM;
                        break;
                    }
                    BGEN_SYM(give_right)(parent, parent->len-1, true);
#ifdef BGEN_COUNTED
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
            return BGEN_DELETED;
        }
#ifdef BGEN_COUNTED
        node->counts[node->len]--;
        depth++;
#endif
        parent = node;
        if (!BGEN_SYM(cow)(&node->children[node->len], udata)) {
            ret = BGEN_NOMEM;
            break;
        }
        node = node->children[node->len];
    }
#ifdef BGEN_COUNTED
    node = *root;
    for (int i = 0; i < depth; i++) {
        node->counts[node->len]++;
        node = node->children[node->len];
    }
#endif
    return ret;
}
#endif

static int BGEN_SYM(pop_back)(BGEN_NODE **root, BGEN_ITEM *olditem, void *udata)
{
    int ret;
#ifndef BGEN_SPATIAL
    ret = BGEN_SYM(pop_back_fastpath)(root, olditem, udata);
    if (ret) {
        return ret;
    }
#endif
    BGEN_ITEM spare = { 0 };
    ret = BGEN_SYM(delete0)(root, BGEN_POPBACK, spare, 0, udata,
        &spare);
    if (ret == BGEN_DELETED) {
        if (olditem) {
            *olditem = spare;
        }
    }
    return ret;
}

#ifndef BGEN_SPATIAL
static int BGEN_SYM(push_front_fastpath)(BGEN_NODE **root, BGEN_ITEM item,
    void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    BGEN_NODE *node = *root;
#ifdef BGEN_COUNTED
    int depth = 0;
#endif
    int ret = 0;
    BGEN_NODE *parent = 0;
    while (1) {
        BGEN_ASSERT(!BGEN_SYM(shared)(node));
        if (node->isleaf) {
            if (node->len == BGEN_MAXITEMS) {
                if (!parent || parent->len == BGEN_MAXITEMS) {
                    break;
                }
                if (parent->children[1]->len < BGEN_MAXITEMS) {
                    // Instead of splitting just give the leaf to the right
                    // as many items as possible.
                    if (!BGEN_SYM(cow)(&parent->children[1], udata)) {
                        ret = BGEN_NOMEM;
                        break;
                    }
                    BGEN_SYM(give_right)(parent, 0, false);
                } else {
                    // Use the standard splitting algorithm
                    if (!BGEN_SYM(split_child_at)(parent, 0, udata)) {
                        ret = BGEN_NOMEM;
                        break;
                    }
                }
#ifdef BGEN_COUNTED
                parent->counts[0]++;
#endif
                node = parent->children[0];
            }
#ifndef BGEN_NOORDER
            if (!BGEN_SYM(less)(item, node->items[0], udata)) {
                ret = BGEN_OUTOFORDER;
                break;
            }
#endif
            BGEN_SYM(shift_right)(node, 0, 1);
            node->items[0] = item;
            return BGEN_INSERTED;
        }
#ifdef BGEN_COUNTED
        node->counts[0]++;
        depth++;
#endif
        parent = node;
        if (!BGEN_SYM(cow)(&node->children[0], udata)) {
            ret = BGEN_NOMEM;
            break;
        }
        node = node->children[0];
    }
#ifdef BGEN_COUNTED
    node = *root;
    for (int i = 0; i < depth; i++) {
        node->counts[0]--;
        node = node->children[0];
    }
#endif
    return ret;
}
#endif

static int BGEN_SYM(push_front)(BGEN_NODE **root, BGEN_ITEM item, void *udata) {
#ifndef BGEN_SPATIAL
    int ret = BGEN_SYM(push_front_fastpath)(root, item, udata);
    if (ret) {
        return ret;
    }
#endif
    return BGEN_SYM(insert0)(root, BGEN_PUSHFRONT, 0, item, 0, udata);
}

#ifndef BGEN_SPATIAL
static int BGEN_SYM(push_back_fastpath)(BGEN_NODE **root, BGEN_ITEM item,
    void *udata)
{
    if (!*root) {
        return 0;
    }
    if (!BGEN_SYM(cow)(root, udata)) {
        return BGEN_NOMEM;
    }
    BGEN_NODE *node = *root;
#ifdef BGEN_COUNTED
    int depth = 0;
#endif
    int ret = 0;
    BGEN_NODE *parent = 0;
    while (1) {
        BGEN_ASSERT(!BGEN_SYM(shared)(node));
        if (node->isleaf) {
            if (node->len == BGEN_MAXITEMS) {
                if (!parent || parent->len == BGEN_MAXITEMS) {
                    break;
                }
                if (parent->children[parent->len-1]->len < BGEN_MAXITEMS) {
                    if (!BGEN_SYM(cow)(&parent->children[parent->len-1],
                        udata))
                    {
                        ret = BGEN_NOMEM;
                        break;
                    }
                    // Instead of splitting just give the leaf to the left
                    // as many items as possible.
                    BGEN_SYM(give_left)(parent, parent->len, false);
                } else {
                    // Use the standard splitting algorithm
                    if (!BGEN_SYM(split_child_at)(parent, parent->len, 
                        udata))
                    {
                        ret = BGEN_NOMEM;
                        break;
                    }
                }
#ifdef BGEN_COUNTED
                parent->counts[parent->len]++;
#endif
                node = parent->children[parent->len];
            }
#ifndef BGEN_NOORDER
            if (!BGEN_SYM(less)(node->items[node->len-1], item, udata)) {
                ret = BGEN_OUTOFORDER;
                break;
            }
#endif
            node->items[node->len++] = item;
            return BGEN_INSERTED;
        }
#ifdef BGEN_COUNTED
        node->counts[node->len]++;
        depth++;
#endif
        parent = node;
        if (!BGEN_SYM(cow)(&node->children[node->len], udata)) {
            ret = BGEN_NOMEM;
            break;
        }
        node = node->children[node->len];
    }
#ifdef BGEN_COUNTED
    node = *root;
    for (int i = 0; i < depth; i++) {
        node->counts[node->len]--;
        node = node->children[node->len];
    }
#endif
    return ret;
}
#endif

static int BGEN_SYM(push_back)(BGEN_NODE **root, BGEN_ITEM item, void *udata) {
#ifndef BGEN_SPATIAL
    int ret = BGEN_SYM(push_back_fastpath)(root, item, udata);
    if (ret) {
        return ret;
    }
#endif
    return BGEN_SYM(insert0)(root, BGEN_PUSHBACK, 0, item, 0, udata);
}

// Returns OUTOFORDER: The item cannot be inserted at index due to the item not
// being in order relative to the items at indexes to the right and left.
// Returns NOMEM: System is out of memory.
// Returns NOTFOUND: The item cannot be inserted because the index is out of 
// bounds, thus the index was > tree count.
static int BGEN_SYM(insert_at)(BGEN_NODE **root, size_t index, BGEN_ITEM item,
    void *udata)
{
    return BGEN_SYM(insert0)(root, BGEN_INSAT, index, item, 0, udata);
}

static int BGEN_SYM(copy)(BGEN_NODE **root, BGEN_NODE **newroot, void *udata) {
    if (!*root) {
        if (newroot) {
            *newroot = 0;
        }
        return BGEN_COPIED;
    }
    BGEN_NODE *node2 = BGEN_SYM(node_copy)(*root, true, udata);
    if (!node2) {
        return BGEN_NOMEM;
    }
    if (newroot) {
        *newroot = node2;
    }
    return BGEN_COPIED;
}

static int BGEN_SYM(clone)(BGEN_NODE **root, BGEN_NODE **newroot, void *udata) {
#ifndef BGEN_COW
    return BGEN_SYM(copy)(root, newroot, udata);
#else
    (void)udata;
    if (newroot) {
        *newroot = *root;
    }
    if (*root) {
        BGEN_SYM(rc_fetch_add)(&(*root)->rc, 1);
    }
#endif
    return BGEN_COPIED;
}

#ifdef BGEN_SPATIAL

// The nearby scanner is a kNN operation that uses a heap-based priority queue.

#define BGEN_PQUEUE struct BGEN_SYM(pqueue)
#define BGEN_PITEM struct BGEN_SYM(pitem)
#define BGEN_DSIZE 8

BGEN_PITEM {
    BGEN_RTYPE dist;
    uint64_t index;    // 1+ = node, item = UINT64_MAX
    union {
        BGEN_ITEM item;
        BGEN_NODE *node;
    };
};

BGEN_PQUEUE {
    BGEN_PITEM *items;
    size_t len;
    size_t cap;
    uint64_t index; // index counter, this increments with every added node.
};

static void BGEN_SYM(pqueue_init)(BGEN_PQUEUE *queue) {
    queue->items = 0;
    queue->len = 0;
    queue->cap = 0;
    queue->index = 0;
}

static int BGEN_SYM(pcompare)(BGEN_PQUEUE *queue, size_t i, size_t j, 
    void *udata)
{
    int cmp;
    // Compare distances
    cmp = queue->items[i].dist < queue->items[j].dist ? -1 : 
          queue->items[i].dist > queue->items[j].dist;
    if (cmp != 0) {
        return cmp;
    }
    // Distances are equal, compare the indexes
    cmp = queue->items[i].index < queue->items[j].index ? -1 : 
          queue->items[i].index > queue->items[j].index;
    if (cmp != 0) {
        return cmp;
    }
    // Indexes are equal, must be an item. compare the items
    BGEN_ASSERT(queue->items[i].index == UINT64_MAX);
    BGEN_ASSERT(queue->items[j].index == UINT64_MAX);
    return BGEN_SYM(compare)(queue->items[i].item, queue->items[j].item, udata);
}

static void BGEN_SYM(pclear)(BGEN_PQUEUE *queue) {
    if (queue->items) {
        BGEN_FREE(queue->items);
    }
    BGEN_SYM(pqueue_init)(queue);
}

static void BGEN_SYM(pswap)(BGEN_PQUEUE *queue, size_t i, size_t j) {
    BGEN_PITEM tmp  = queue->items[i];
    queue->items[i] = queue->items[j];
    queue->items[j] = tmp;
}


static BGEN_PITEM BGEN_SYM(pswap_remove)(BGEN_PQUEUE *queue, size_t i) {
    BGEN_PITEM item = queue->items[i];
    queue->items[i] = queue->items[queue->len-1];
    queue->len--;
    return item;
}


static int BGEN_SYM(ppush0)(BGEN_PQUEUE *queue, BGEN_PITEM item, void *udata) {
    if (queue->len == queue->cap) {
        queue->cap = queue->cap == 0 ? 8 : queue->cap*2;
        BGEN_PITEM *items2 = BGEN_MALLOC(sizeof(BGEN_PITEM)*queue->cap);
        if (!items2) {
            return BGEN_NOMEM;
        }
        for (size_t i = 0; i < queue->len; i++) {
            items2[i] = queue->items[i];
        }
        BGEN_FREE(queue->items);
        queue->items = items2;
    }
    queue->items[queue->len++] = item;
    // static size_t nlen = 0;
    // if (queue->len > nlen) {
    //     printf("%zu\n", nlen);
    //     nlen = queue->len;
    // }

    size_t i = queue->len - 1;
    while (i != 0) {
        size_t parent = (i - 1) / 2;
        if (!(BGEN_SYM(pcompare)(queue, parent, i, udata) > 0)) {
            break;
        }
        BGEN_SYM(pswap)(queue, parent, i);
        i = parent;
    }
    return 0;
}

static BGEN_PITEM BGEN_SYM(ppop)(BGEN_PQUEUE *queue, void *udata) {
    BGEN_PITEM item = BGEN_SYM(pswap_remove)(queue, 0);
    size_t i = 0;
    while (1) {
        size_t smallest = i;
        size_t left = i * 2 + 1;
        size_t right = i * 2 + 2;
        if (left < queue->len && BGEN_SYM(pcompare)(queue, left, smallest, 
            udata) <= 0)
        {
            smallest = left;
        }
        if (right < queue->len && BGEN_SYM(pcompare)(queue, right, smallest, 
            udata) <= 0)
        {
            smallest = right;
        }
        if (smallest == i) {
            break;
        }
        BGEN_SYM(pswap)(queue, smallest, i);
        i = smallest;
    }
    return item;
}

static int BGEN_SYM(ppush_item)(BGEN_PQUEUE *queue, BGEN_ITEM item, 
    BGEN_RTYPE dist, void *udata)
{
    queue->index++;
    BGEN_PITEM pitem = { .dist = dist, .index = UINT64_MAX, .item = item };
    return BGEN_SYM(ppush0)(queue, pitem, udata);
}

static int BGEN_SYM(ppush_node)(BGEN_PQUEUE *queue, BGEN_NODE *node, 
    BGEN_RTYPE dist, void *udata)
{
    queue->index++;
    BGEN_PITEM pitem = { .dist = dist, .index = queue->index, .node = node };
    return BGEN_SYM(ppush0)(queue, pitem, udata);
}

#endif

// stack node used by an iterator
BGEN_SNODE {
    BGEN_NODE *node; // the node
    int index;       // index of current item in node, used by iter_item()
};

#define BGEN_SCAN       0
#define BGEN_SCANDESC   1
#define BGEN_INTERSECTS 2
#define BGEN_NEARBY     3

BGEN_ITER {
    BGEN_NODE **root;         // root node
    void *udata;              // user data
    int kind;                 // kind of iterator
    bool mut;                 // this is a mutable iterator
    bool valid;               // iterator is valid
    short status;             // last status code. Zero for no errors
    union {
#ifdef BGEN_SPATIAL
        struct {
            // nearby target and distance callback
            void *ntarget; 
            BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], 
                BGEN_RTYPE max[BGEN_DIMS], void *target, void *udata);
            BGEN_PQUEUE queue; // priority queue
            BGEN_ITEM nitem; // current nearby item
        };
#endif
        struct  {
#ifdef BGEN_SPATIAL
            BGEN_RECT itarget; // intersects target
#endif
            short nstack; // number of path nodes (depth)
            BGEN_SNODE stack[BGEN_MAXHEIGHT]; // traversed path nodes
        };
    };
};

static void BGEN_SYM(iter_init)(BGEN_NODE **root, BGEN_ITER *iter, void *udata){
    iter->root = root;
    iter->udata = udata;
    iter->mut = false;
    iter->valid = false;
    iter->kind = 0;
}

static void BGEN_SYM(iter_init_mut)(BGEN_NODE **root, BGEN_ITER *iter, 
    void *udata)
{
    BGEN_SYM(iter_init)(root, iter, udata);
    iter->mut = 1;
}

static void BGEN_SYM(iter_reset)(BGEN_ITER *iter, int kind) {
    iter->valid = true;
    iter->status = 0;
#ifdef BGEN_SPATIAL
    // printf("A\n");
    if (iter->kind == BGEN_NEARBY && kind != BGEN_NEARBY) {
        // printf("B\n");
        // switching from NEARBY to SCAN
        BGEN_SYM(pclear)(&iter->queue);
    } else if (iter->kind != BGEN_NEARBY && kind == BGEN_NEARBY) {
        // printf("C\n");
        // switching from SCAN to NEARBY
        BGEN_SYM(pqueue_init)(&iter->queue);
    }
    // printf("D\n");
#endif
    iter->nstack = 0;
    iter->kind = kind;
}

static void BGEN_SYM(iter_release)(BGEN_ITER *iter) {
    (void)iter;
#ifdef BGEN_SPATIAL
    if (iter->kind == BGEN_NEARBY) {
        BGEN_SYM(pclear)(&iter->queue);
    }
#endif
}

static bool BGEN_SYM(iter_valid)(BGEN_ITER *iter) {
    return iter->valid;
}

static int BGEN_SYM(iter_status)(BGEN_ITER *iter) {
    return iter->status;
}


#ifdef BGEN_SPATIAL
static bool BGEN_SYM(iter_skip_item)(BGEN_ITER *iter, BGEN_SNODE *snode) {
    if (iter->kind == BGEN_INTERSECTS) {
        BGEN_RECT rect = BGEN_SYM(item_rect)(
            snode->node->items[snode->index], iter->udata);
        if (!BGEN_SYM(rect_intersects)(iter->itarget, rect)) {
            return true;
        }
    }
    return false;
}
static bool BGEN_SYM(iter_skip_node)(BGEN_ITER *iter, BGEN_SNODE *snode) {
    if (iter->kind == BGEN_INTERSECTS) {
        BGEN_RECT rect = snode->node->rects[snode->index];
        if (!BGEN_SYM(rect_intersects)(iter->itarget, rect)) {
            return true;
        }
    }
    return false;
}
#endif

BGEN_NOINLINE
static void BGEN_SYM(iter_next_asc)(BGEN_ITER *iter) {
    BGEN_SNODE *snode = &iter->stack[iter->nstack-1];
    while (1) {
        snode = &iter->stack[iter->nstack-1];
        snode->index++;
        if (snode->node->isleaf && snode->index < snode->node->len) {
        next_item:
#ifdef BGEN_SPATIAL
            if (BGEN_SYM(iter_skip_item)(iter, snode)) {
                continue;
            }
#endif
            // Iterator now points to the next item
            return;
        }
        if (snode->node->isleaf || snode->index == snode->node->len+1) {
            // pop the stack
            while (iter->nstack > 1) {
                iter->nstack--;
                snode = &iter->stack[iter->nstack-1];
                if (snode->index < snode->node->len) {
                    goto next_item;
                }
            }
            // end of iterator
            iter->valid = false;
            return;
        }
#ifdef BGEN_SPATIAL
        if (BGEN_SYM(iter_skip_node)(iter, snode)) {
            continue;
        }
#endif
        if (iter->mut && !BGEN_SYM(cow)(&snode->node->children[snode->index], 
            iter->udata))
        {
            iter->status = BGEN_NOMEM;
            iter->valid = false;
            return;
        }
        iter->stack[iter->nstack++] = (BGEN_SNODE){ 
            snode->node->children[snode->index], -1 };
    }
}

// Move iterator cursor to the previous item.
BGEN_NOINLINE
static void BGEN_SYM(iter_next_desc)(BGEN_ITER *iter) {
    BGEN_SNODE *snode;
    while (1) {
        snode = &iter->stack[iter->nstack-1];
        snode->index--;
        if (snode->node->isleaf && snode->index > -1) {
            // Iterator now points to the next item
            return;
        }
        if (snode->node->isleaf) {
            // pop stack
            while (iter->nstack > 1) {
                iter->nstack--;
                snode = &iter->stack[iter->nstack-1];
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
        if (iter->mut && !BGEN_SYM(cow)(&snode->node->children[snode->index], 
            iter->udata))
        {
            iter->status = BGEN_NOMEM;
            iter->valid = false;
            return;
        }
        BGEN_NODE *node = snode->node->children[snode->index];
        iter->stack[iter->nstack++] = (BGEN_SNODE){ node, node->len };
        snode = &iter->stack[iter->nstack-1];
    }
}

#ifdef BGEN_SPATIAL

static int BGEN_SYM(nearby_addnodecontents)(BGEN_PQUEUE *queue, 
    BGEN_NODE *node, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS], 
    void *target, void *udata), void *udata, bool mut)
{
    BGEN_ASSERT(!mut || !BGEN_SYM(shared)(node));
    for (int i = 0; i < node->len; i++) {
        BGEN_RECT rect = BGEN_SYM(item_rect)(node->items[i], udata);
        BGEN_RTYPE d = dist(rect.min, rect.max, target, udata);
        int status = BGEN_SYM(ppush_item)(queue, node->items[i], d, udata);
        if (status) {
            return status;
        }
    }
    if (!node->isleaf) {
        for (int i = 0; i <= node->len; i++) {
            BGEN_RTYPE d = dist(node->rects[i].min, node->rects[i].max, target,
                udata);
            if (mut && !BGEN_SYM(cow)(&node->children[i], udata)) {
                return BGEN_NOMEM;
            }
            int status = BGEN_SYM(ppush_node)(queue, node->children[i], d,
                udata);
            if (status) {
                return status;
            }
        }
    }
    return 0;
}

BGEN_NOINLINE
static void BGEN_SYM(iter_next_nearby)(BGEN_ITER *iter) {
    // Begin popping queue items.
    while (iter->queue.len > 0) {
        BGEN_PITEM pitem = BGEN_SYM(ppop)(&iter->queue, iter->udata);
        if (pitem.index == UINT64_MAX) {
            // Queue item is a b-tree item. Return to user.
            iter->nitem = pitem.item;
            return;
        } else {
            // Queue item is a node. Add the contents of node and continue 
            // popping queue items.
            int status = BGEN_SYM(nearby_addnodecontents)(&iter->queue, 
                pitem.node, iter->ntarget, iter->dist, iter->udata, iter->mut);
            if (status) {
                iter->valid = false;
                iter->status = status;
                return;
            }
        }
    }
    iter->valid = false;
}

#endif

// Move iterator cursor to the next item.
// REQUIRED: iter_valid()
BGEN_INLINE
static void BGEN_SYM(iter_next)(BGEN_ITER *iter) {
    BGEN_ASSERT(BGEN_SYM(iter_valid)(iter));
#ifdef BGEN_SPATIAL
    if (iter->kind == BGEN_INTERSECTS) {
        BGEN_SYM(iter_next_asc)(iter);
    } else if (iter->kind == BGEN_NEARBY) {
        BGEN_SYM(iter_next_nearby)(iter);
    } else
#endif
    if (iter->kind == BGEN_SCAN) {
        // Fastpath for forward scanning iterators iter_seek and iter_scan,
        // where the next item is in a leaf. Fallback to the function call.
        BGEN_SNODE *snode = &iter->stack[iter->nstack-1];
        if (snode->node->isleaf && snode->index+1 < snode->node->len) {
            snode->index++;
        } else {
            BGEN_SYM(iter_next_asc)(iter);
        }
    } else if (iter->kind == BGEN_SCANDESC) {
        BGEN_SYM(iter_next_desc)(iter);
    }
}

// Moves iterator to first item and resets the status
static void BGEN_SYM(iter_scan)(BGEN_ITER *iter) {
    BGEN_SYM(iter_reset)(iter, BGEN_SCAN);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BGEN_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BGEN_NOMEM;
        iter->valid = false;
        return;
    }
    BGEN_NODE *node = *iter->root;
    while (1) {
        iter->stack[iter->nstack++] = (BGEN_SNODE){ node, 0 };
        if (node->isleaf) {
            return;
        }
        if (iter->mut && !BGEN_SYM(cow)(&node->children[0], iter->udata)) {
            iter->status = BGEN_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[0];
    }
}

// Moves iterator to last item and resets the status
static void BGEN_SYM(iter_scan_desc)(BGEN_ITER *iter) {
    BGEN_SYM(iter_reset)(iter, BGEN_SCANDESC);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BGEN_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BGEN_NOMEM;
        iter->valid = false;
        return;
    }
    BGEN_NODE *node = *iter->root;
    while (1) {
        iter->stack[iter->nstack++] = (BGEN_SNODE){ node, node->len };
        if (node->isleaf) {
            iter->stack[iter->nstack-1].index--;
            return;
        }
        if (iter->mut && !BGEN_SYM(cow)(&node->children[node->len],
            iter->udata))
        {
            iter->status = BGEN_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[node->len];
    }
}

#ifdef BGEN_SPATIAL
// Finds the first intersecting item and fills the stack along the way.
static bool BGEN_SYM(iter_intersects_first)(BGEN_ITER *iter, BGEN_NODE *node) {
    int depth = iter->nstack;
    iter->stack[iter->nstack++] = (BGEN_SNODE){ node, 0 };
    if (node->isleaf) {
        for (int i = 0; i < node->len; i++) {
            BGEN_RECT rect = BGEN_SYM(item_rect)(node->items[i], iter->udata);
            if (BGEN_SYM(rect_intersects)(iter->itarget, rect)) {
                iter->stack[depth].index = i;
                return true;
            }
        }
    } else {
        for (int i = 0; i <= node->len; i++) {
            if (BGEN_SYM(rect_intersects)(iter->itarget, node->rects[i])) {
                if (BGEN_SYM(iter_intersects_first)(iter, node->children[i])) {
                    iter->stack[depth].index = i;
                    return true;
                }
                if (i < node->len) {
                    BGEN_RECT rect = BGEN_SYM(item_rect)(node->items[i], 
                        iter->udata);
                    if (BGEN_SYM(rect_intersects)(iter->itarget, rect)) {
                        iter->stack[depth].index = i;
                        return true;
                    }
                }
            }
        }
    }
    iter->nstack--;
    return false;
}
#endif

static void BGEN_SYM(iter_seek)(BGEN_ITER *iter, BGEN_ITEM key) {
#ifdef BGEN_NOORDER
    (void)iter, (void)key;
    iter->valid = false;
    iter->status = BGEN_UNSUPPORTED;
#else
    BGEN_SYM(iter_reset)(iter, BGEN_SCAN);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BGEN_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BGEN_NOMEM;
        iter->valid = false;
        return;
    }
    int depth = 0;
    BGEN_NODE *node = *iter->root;
    while (1) {
        int found;
        int i = BGEN_SYM(search)(node, key, iter->udata, &found, depth);
        iter->stack[iter->nstack++] = (BGEN_SNODE){ node, i };
        if (found) {
            return;
        }
        if (node->isleaf) {
            iter->stack[iter->nstack-1].index--;
            BGEN_SYM(iter_next)(iter);
            return;
        }
        if (iter->mut && !BGEN_SYM(cow)(&node->children[i], iter->udata)) {
            iter->status = BGEN_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[i];
        depth++;
    }
#endif
}


static void BGEN_SYM(iter_seek_at)(BGEN_ITER *iter, size_t index) {
    BGEN_SYM(iter_reset)(iter, BGEN_SCAN);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BGEN_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BGEN_NOMEM;
        iter->valid = false;
        return;
    }
    BGEN_NODE *node = *iter->root;
    while (1) {
        iter->stack[iter->nstack++] = (BGEN_SNODE){ node, 0 };
        if (node->isleaf) {
            if (index >= (size_t)node->len) {
                iter->stack[iter->nstack-1].index = node->len;
            } else {
                iter->stack[iter->nstack-1].index = index;
            }
            iter->stack[iter->nstack-1].index--;
            BGEN_SYM(iter_next)(iter);
            return;
        }
        int i = 0;
        bool found = false;
        for (; i < node->len; i++) {
            size_t count = BGEN_SYM(node_count)(node, i);
            if (index <= count) {
                found = index == count;
                break;
            }
            index -= count + 1;
        }
        iter->stack[iter->nstack-1].index = i;
        if (found) {
            return;
        }
        if (iter->mut && !BGEN_SYM(cow)(&node->children[i], iter->udata)) {
            iter->status = BGEN_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[i];
    }
}

static void BGEN_SYM(iter_seek_at_desc)(BGEN_ITER *iter, size_t index) {
    BGEN_SYM(iter_reset)(iter, BGEN_SCANDESC);
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BGEN_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BGEN_NOMEM;
        iter->valid = false;
        return;
    }
    BGEN_NODE *node = *iter->root;
    while (1) {
        iter->stack[iter->nstack++] = (BGEN_SNODE){ node, 0 };
        if (node->isleaf) {
            if (index >= (size_t)node->len) {
                iter->stack[iter->nstack-1].index = node->len-1;
            } else {
                iter->stack[iter->nstack-1].index = index;
            }
            return;
        }
        int i = 0;
        bool found = false;
        for (; i < node->len; i++) {
            size_t count = BGEN_SYM(node_count)(node, i);
            if (index <= count) {
                found = index == count;
                break;
            }
            index -= count + 1;
        }
        iter->stack[iter->nstack-1].index = i;
        if (found) {
            return;
        }
        if (iter->mut && !BGEN_SYM(cow)(&node->children[i], iter->udata)) {
            iter->status = BGEN_NOMEM;
            iter->valid = false;
            return;
        }
        node = node->children[i];
    }
}

static void BGEN_SYM(iter_intersects)(BGEN_ITER *iter, BGEN_RTYPE min[], 
    BGEN_RTYPE max[])
{
    BGEN_SYM(iter_reset)(iter, BGEN_INTERSECTS);
#ifndef BGEN_SPATIAL
    (void)iter, (void)min, (void)max;
    iter->valid = false;
#else
    for (int i = 0; i < BGEN_DIMS; i++) {
        iter->itarget.min[i] = min[i];
        iter->itarget.max[i] = max[i];
    }
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    if (iter->mut && !BGEN_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BGEN_NOMEM;
        iter->valid = false;
        return;
    }
    iter->valid = BGEN_SYM(iter_intersects_first)(iter, *iter->root);
#endif
}


static void BGEN_SYM(iter_nearby)(BGEN_ITER *iter, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS],
    void *target, void *udata))
{
    BGEN_SYM(iter_reset)(iter, BGEN_NEARBY);
#ifndef BGEN_SPATIAL
    (void)iter, (void)target, (void)dist;
    iter->valid = false;
#else
    iter->ntarget = target;
    iter->dist = dist;
    if (!*iter->root) {
        iter->valid = false;
        return;
    }
    
    // Start out by adding the contents of the root node.
    if (iter->mut && !BGEN_SYM(cow)(iter->root, iter->udata)) {
        iter->status = BGEN_NOMEM;
        iter->valid = false;
        return;
    }
    iter->status = BGEN_SYM(nearby_addnodecontents)(&iter->queue, *iter->root, 
        target, iter->dist, iter->udata, iter->mut);
    if (iter->status) {
        iter->valid = false;
        return;
    }
    // At this point there must be at least one item in the queue.
    BGEN_ASSERT(iter->queue.len > 0);
    // Call next to get the first nearby item.
    BGEN_SYM(iter_next_nearby)(iter);
#endif
}

// Get the current iterator item.
// REQUIRES: iter_valid() and item != NULL
static void BGEN_SYM(iter_item)(BGEN_ITER *iter, BGEN_ITEM *item) {
#ifdef BGEN_SPATIAL
    if (iter->kind == BGEN_NEARBY) {
        *item = iter->nitem;
        return;
    }
#endif
    BGEN_SNODE *snode = &iter->stack[iter->nstack-1];
    *item = snode->node->items[snode->index];
}

static void BGEN_SYM(iter_seek_desc)(BGEN_ITER *iter, BGEN_ITEM key) {
#ifdef BGEN_NOORDER
    (void)iter, (void)key;
    iter->valid = false;
    iter->status = BGEN_UNSUPPORTED;
#else
    BGEN_SYM(iter_seek)(iter, key);
    if (!BGEN_SYM(iter_valid)(iter)) {
        if (BGEN_SYM(iter_status)(iter) == 0) {
            BGEN_SYM(iter_scan_desc)(iter);
        }
    } else {
        BGEN_ITEM item;
        BGEN_SYM(iter_item)(iter, &item);
        if (BGEN_SYM(compare)(item, key, iter->udata) > 0) {
            BGEN_SYM(iter_next_desc)(iter);
        }
    }
    iter->kind = BGEN_SCANDESC;
#endif
}

#ifdef BGEN_SPATIAL
static bool BGEN_SYM(node_intersects)(BGEN_NODE *node, BGEN_RECT target,
    bool(*iter)(BGEN_ITEM item, void *udata),
    void *udata)
{
    if (node->isleaf) {
        for (int i = 0; i < node->len; i++) {
            BGEN_RECT rect = BGEN_SYM(item_rect)(node->items[i], udata);
            if (BGEN_SYM(rect_intersects)(target, rect)) {
                if (!iter(node->items[i], udata)) {
                    return false;
                }
            }
        }
        return true;
    }
    for (int i = 0; i < node->len; i++) {
        if (BGEN_SYM(rect_intersects)(target, node->rects[i])) {
            BGEN_NODE *child = node->children[i];
            if (!BGEN_SYM(node_intersects)(child, target, iter, udata)) {
                return false;
            }
            BGEN_RECT rect = BGEN_SYM(item_rect)(node->items[i], udata);
            if (BGEN_SYM(rect_intersects)(target, rect)) {
                if (!iter(node->items[i], udata)) {
                    return false;
                }
            }
        }
    }
    if (BGEN_SYM(rect_intersects)(target, node->rects[node->len])) {
        BGEN_NODE *child = node->children[node->len];
        if (!BGEN_SYM(node_intersects)(child, target, iter, udata)) {
            return false;
        }
    }
    return true;
}
#endif

static int BGEN_SYM(intersects)(BGEN_NODE **root, BGEN_RTYPE min[],
    BGEN_RTYPE max[], bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{  
    (void)root, (void)min, (void)max, (void)iter, (void)udata; 
    int status = BGEN_FINISHED;
#ifdef BGEN_SPATIAL
    BGEN_RECT target;
    for (int i = 0; i < BGEN_DIMS; i++) {
        target.min[i] = min[i];
    }
    for (int i = 0; i < BGEN_DIMS; i++) {
        target.max[i] = max[i];
    }
    if (*root) {
        if (!BGEN_SYM(node_intersects)(*root, target, iter, udata)) {
            status = BGEN_STOPPED;
        }
    }
#endif
    return status;
}

#ifdef BGEN_SPATIAL
static bool BGEN_SYM(node_intersects_mut)(BGEN_NODE *node, BGEN_RECT target,
    bool(*iter)(BGEN_ITEM item, void *udata),
    void *udata, int *status)
{
    if (node->isleaf) {
        for (int i = 0; i < node->len; i++) {
            BGEN_RECT rect = BGEN_SYM(item_rect)(node->items[i], udata);
            if (BGEN_SYM(rect_intersects)(target, rect)) {
                if (!iter(node->items[i], udata)) {
                    return false;
                }
            }
        }
        return true;
    }
    for (int i = 0; i < node->len; i++) {
        if (BGEN_SYM(rect_intersects)(target, node->rects[i])) {
            if (!BGEN_SYM(cow)(&node->children[i], udata)) {
                *status = BGEN_NOMEM;
                return false;
            }
            BGEN_NODE *child = node->children[i];
            if (!BGEN_SYM(node_intersects_mut)(child, target, iter, udata, 
                status))
            {
                return false;
            }
            BGEN_RECT rect = BGEN_SYM(item_rect)(node->items[i], udata);
            if (BGEN_SYM(rect_intersects)(target, rect)) {
                if (!iter(node->items[i], udata)) {
                    return false;
                }
            }
        }
    }
    if (!BGEN_SYM(cow)(&node->children[node->len], udata)) {
        *status = BGEN_NOMEM;
        return false;
    }
    if (BGEN_SYM(rect_intersects)(target, node->rects[node->len])) {
        BGEN_NODE *child = node->children[node->len];
        if (!BGEN_SYM(node_intersects_mut)(child, target, iter, udata, status)){
            return false;
        }
    }
    return true;
}
#endif

static int BGEN_SYM(intersects_mut)(BGEN_NODE **root, 
    BGEN_RTYPE min[], BGEN_RTYPE max[], 
    bool(*iter)(BGEN_ITEM item, void *udata),
    void *udata)
{  
    (void)root, (void)min, (void)max, (void)iter, (void)udata; 
    int status = BGEN_FINISHED;
#ifdef BGEN_SPATIAL
    if (*root) {
        BGEN_RECT target;
        for (int i = 0; i < BGEN_DIMS; i++) {
            target.min[i] = min[i];
        }
        for (int i = 0; i < BGEN_DIMS; i++) {
            target.max[i] = max[i];
        }
        if (!BGEN_SYM(cow)(root, udata)) {
            return BGEN_NOMEM;
        }
        if (!BGEN_SYM(node_intersects_mut)(*root, target, iter, udata, &status))
        {
            if (status == BGEN_FINISHED) {
                status = BGEN_STOPPED;
            }
        }
    }
#endif
    return status;
}

#ifdef BGEN_SPATIAL

static int BGEN_SYM(nearby0)(BGEN_NODE **root, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS], 
    void *target, void *udata), bool(*iter)(BGEN_ITEM item, void *udata),
    void *udata, bool mut)
{
    if (!*root) {
        return BGEN_FINISHED;
    }
    BGEN_PQUEUE queue;
    BGEN_SYM(pqueue_init)(&queue);
    int status = 0;
    // Start out by adding the contents of the root node.
    if (mut && !BGEN_SYM(cow)(root, udata)) {
        status = BGEN_NOMEM;
        goto done;
    }
    status = BGEN_SYM(nearby_addnodecontents)(&queue, *root, target, dist,
        udata, mut);
    if (status) {
        goto done;
    }
    // At this point there must be at least one item in the queue.
    BGEN_ASSERT(queue.len > 0);
    // Begin popping queue items.
    while (queue.len > 0) {
        BGEN_PITEM pitem = BGEN_SYM(ppop)(&queue, udata);
        if (pitem.index == UINT64_MAX) {
            // Queue item is a b-tree item. Return to user.
            if (!iter(pitem.item, udata)) {
                status = BGEN_STOPPED;
                goto done;
            }
        } else {
            // Queue item is a node. Add the contents of node and continue 
            // popping queue items.
            status = BGEN_SYM(nearby_addnodecontents)(&queue, pitem.node,
                target, dist, udata, mut);
            if (status) {
                goto done;
            }
        }
    }
done:
    BGEN_SYM(pclear)(&queue);
    if (status == 0) {
        status = BGEN_FINISHED;
    }
    return status;
}
#else
static int BGEN_SYM(nearby0)(BGEN_NODE **root, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS], 
    void *target, void *udata), bool(*iter)(BGEN_ITEM item, void *udata),
    void *udata, bool mut)
{
    (void)root, (void)target, (void)dist, (void)iter, (void)udata, (void)mut;
    return BGEN_FINISHED;
}
#endif

static int BGEN_SYM(nearby)(BGEN_NODE **root, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS], 
    void *target, void *udata), bool(*iter)(BGEN_ITEM item, void *udata),
    void *udata)
{
    return BGEN_SYM(nearby0)(root, target, dist, iter, udata, 0);
}

static int BGEN_SYM(nearby_mut)(BGEN_NODE **root, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS], 
    void *target, void *udata), bool(*iter)(BGEN_ITEM item, void *udata),
    void *udata)
{
    return BGEN_SYM(nearby0)(root, target, dist, iter, udata, 1);
}

#ifdef BGEN_SPATIAL
static void BGEN_SYM(node_scan_rects)(BGEN_NODE *node,
    void(*iter)(BGEN_RTYPE min[], BGEN_RTYPE max[], int depth, void *udata),
    int depth, void *udata)
{
    if (!node->isleaf) {
        for (int i = 0; i <= node->len; i++) {
            iter(node->rects[i].min, node->rects[i].max, depth, udata);
            BGEN_SYM(node_scan_rects)(node->children[i], iter, depth+1, udata);
        }
    }
}
#endif

static void BGEN_SYM(scan_rects)(BGEN_NODE **root, void(*iter)(BGEN_RTYPE min[], 
    BGEN_RTYPE max[], int depth, void *udata), void *udata)
{
    (void)root, (void)iter, (void)udata;
#ifdef BGEN_SPATIAL
    if (*root) {
        BGEN_SYM(node_scan_rects)(*root, iter, 0, udata);
    }
#endif
}

static void BGEN_SYM(rect)(BGEN_NODE **root, BGEN_RTYPE min[BGEN_DIMS], 
    BGEN_RTYPE max[BGEN_DIMS], void *udata)
{
    (void)root, (void)min, (void)max, (void)udata;
#ifdef BGEN_SPATIAL
    if (*root) {
        BGEN_NODE *node = *root;
        BGEN_RECT rect;
        if (!node->isleaf) {
            rect = node->rects[0];
            for (int j = 1; j <= node->len; j++) {
                rect = BGEN_SYM(rect_join)(rect, node->rects[j]);
            }
        } else {
            rect = BGEN_SYM(item_rect)(node->items[0], udata);
            for (int j = 1; j < node->len; j++) {
                rect = BGEN_SYM(rect_join)(rect, 
                    BGEN_SYM(item_rect)(node->items[j], udata));
            }
        }
        for (int i = 0; i < BGEN_DIMS; i++) {
            min[i] = rect.min[i];
            max[i] = rect.max[i];
        }
    } else
#endif
    {
        for (int i = 0; i < BGEN_DIMS; i++) {
            min[i] = 0.0;
            max[i] = 0.0;
        }
    }
}

static inline void BGEN_SYM(all_sym_calls)(void) {
    // All internal symbols
    (void)BGEN_SYM(all_sym_calls);
    (void)BGEN_SYM(feat_maxitems);
    (void)BGEN_SYM(feat_minitems);
    (void)BGEN_SYM(feat_maxheight);
    (void)BGEN_SYM(feat_fanout);
    (void)BGEN_SYM(feat_counted);
    (void)BGEN_SYM(feat_spatial);
    (void)BGEN_SYM(feat_ordered);
    (void)BGEN_SYM(feat_bsearch);
    (void)BGEN_SYM(feat_pathhint);
    (void)BGEN_SYM(feat_cow);
    (void)BGEN_SYM(feat_atomics);
    (void)BGEN_SYM(feat_dims);
    (void)BGEN_SYM(print);
    (void)BGEN_SYM(get_mut);
    (void)BGEN_SYM(get_at_mut);
    (void)BGEN_SYM(insert);
    (void)BGEN_SYM(get);
    (void)BGEN_SYM(index_of);
    (void)BGEN_SYM(contains);
    (void)BGEN_SYM(delete);
    (void)BGEN_SYM(get_at);
    (void)BGEN_SYM(insert_at);
    (void)BGEN_SYM(delete_at);
    (void)BGEN_SYM(replace_at);
    (void)BGEN_SYM(count);
    (void)BGEN_SYM(height);
    (void)BGEN_SYM(clear);
    (void)BGEN_SYM(sane);
    (void)BGEN_SYM(front);
    (void)BGEN_SYM(front_mut);
    (void)BGEN_SYM(back);
    (void)BGEN_SYM(back_mut);
    (void)BGEN_SYM(pop_front);
    (void)BGEN_SYM(pop_back);
    (void)BGEN_SYM(push_front);
    (void)BGEN_SYM(push_back);
    (void)BGEN_SYM(copy);
    (void)BGEN_SYM(clone);
    (void)BGEN_SYM(compare);
    (void)BGEN_SYM(less);
    (void)BGEN_SYM(iter_init);
    (void)BGEN_SYM(iter_init_mut);
    (void)BGEN_SYM(iter_release);
    (void)BGEN_SYM(iter_valid);
    (void)BGEN_SYM(iter_status);
    (void)BGEN_SYM(iter_seek);
    (void)BGEN_SYM(iter_seek_desc);
    (void)BGEN_SYM(iter_scan);
    (void)BGEN_SYM(iter_scan_desc);
    (void)BGEN_SYM(iter_intersects);
    (void)BGEN_SYM(iter_nearby);
    (void)BGEN_SYM(iter_seek_at);
    (void)BGEN_SYM(iter_seek_at_desc);
    (void)BGEN_SYM(iter_next);
    (void)BGEN_SYM(iter_item);
    (void)BGEN_SYM(intersects);
    (void)BGEN_SYM(scan);
    (void)BGEN_SYM(scan_desc);
    (void)BGEN_SYM(seek);
    (void)BGEN_SYM(seek_at);
    (void)BGEN_SYM(seek_at_desc);
    (void)BGEN_SYM(seek_desc);
    (void)BGEN_SYM(scan_mut);
    (void)BGEN_SYM(scan_desc_mut);
    (void)BGEN_SYM(seek_mut);
    (void)BGEN_SYM(seek_desc_mut);
    (void)BGEN_SYM(intersects_mut);
    (void)BGEN_SYM(nearby);
    (void)BGEN_SYM(nearby_mut);
    (void)BGEN_API(seek_at_mut);
    (void)BGEN_SYM(seek_at_desc_mut);
    (void)BGEN_SYM(rect);
    (void)BGEN_SYM(scan_rects);
    (void)BGEN_SYM(shared);
}

static inline void BGEN_SYM(all_api_calls)(void) {
    // All external symbols
    (void)BGEN_SYM(all_api_calls);
    (void)BGEN_API(feat_maxitems);
    (void)BGEN_API(feat_minitems);
    (void)BGEN_API(feat_maxheight);
    (void)BGEN_API(feat_fanout);
    (void)BGEN_API(feat_counted);
    (void)BGEN_API(feat_spatial);
    (void)BGEN_API(feat_ordered);
    (void)BGEN_API(feat_bsearch);
    (void)BGEN_API(feat_pathhint);
    (void)BGEN_API(feat_cow);
    (void)BGEN_API(feat_atomics);
    (void)BGEN_API(feat_dims);
    (void)BGEN_API(get_mut);
    (void)BGEN_API(get_at_mut);
    (void)BGEN_API(insert);
    (void)BGEN_API(get);
    (void)BGEN_API(index_of);    
    (void)BGEN_API(contains);
    (void)BGEN_API(delete);
    (void)BGEN_API(get_at);
    (void)BGEN_API(insert_at);
    (void)BGEN_API(delete_at);
    (void)BGEN_API(replace_at);
    (void)BGEN_API(count);
    (void)BGEN_API(height);
    (void)BGEN_API(clear);
    (void)BGEN_API(sane);
    (void)BGEN_API(front);
    (void)BGEN_API(front_mut);
    (void)BGEN_API(back);
    (void)BGEN_API(back_mut);
    (void)BGEN_API(pop_front);
    (void)BGEN_API(pop_back);
    (void)BGEN_API(push_front);
    (void)BGEN_API(push_back);
    (void)BGEN_API(copy);
    (void)BGEN_API(clone);
    (void)BGEN_API(compare);
    (void)BGEN_API(less);
    (void)BGEN_API(iter_init);
    (void)BGEN_API(iter_init_mut);
    (void)BGEN_API(iter_release);
    (void)BGEN_API(iter_valid);
    (void)BGEN_API(iter_status);
    (void)BGEN_API(iter_seek);
    (void)BGEN_API(iter_seek_desc);
    (void)BGEN_API(iter_scan);
    (void)BGEN_API(iter_scan_desc);
    (void)BGEN_API(iter_intersects);
    (void)BGEN_API(iter_nearby);
    (void)BGEN_API(iter_seek_at);
    (void)BGEN_API(iter_seek_at_desc);
    (void)BGEN_API(iter_next);
    (void)BGEN_API(iter_item);
    (void)BGEN_API(scan);
    (void)BGEN_API(scan_desc);
    (void)BGEN_API(seek);
    (void)BGEN_API(seek_at);
    (void)BGEN_API(seek_at_desc);
    (void)BGEN_API(seek_desc);
    (void)BGEN_API(intersects);
    (void)BGEN_API(scan_mut);
    (void)BGEN_API(scan_desc_mut);
    (void)BGEN_API(seek_mut);
    (void)BGEN_API(seek_desc_mut);
    (void)BGEN_API(intersects_mut);
    (void)BGEN_API(nearby);
    (void)BGEN_API(nearby_mut);
    (void)BGEN_API(seek_at_mut);
    (void)BGEN_API(seek_at_desc_mut);
    (void)BGEN_API(rect);
}

///////////////////////////////////////////////////////////////////////////////
// Exposed API
///////////////////////////////////////////////////////////////////////////////
int BGEN_API(feat_maxitems)(void) {
    return BGEN_SYM(feat_maxitems)();
}

int BGEN_API(feat_minitems)(void) {
    return BGEN_SYM(feat_minitems)();
}

int BGEN_API(feat_maxheight)(void) {
    return BGEN_SYM(feat_maxheight)();
}

int BGEN_API(feat_fanout)(void) {
    return BGEN_SYM(feat_fanout)();
}

bool BGEN_API(feat_counted)(void) {
    return BGEN_SYM(feat_counted)();
}

bool BGEN_API(feat_spatial)(void) {
    return BGEN_SYM(feat_spatial)();
}

bool BGEN_API(feat_ordered)(void) {
    return BGEN_SYM(feat_ordered)();
}

bool BGEN_API(feat_bsearch)(void) {
    return BGEN_SYM(feat_bsearch)();
}

bool BGEN_API(feat_pathhint)(void) {
    return BGEN_SYM(feat_pathhint)();
}

bool BGEN_API(feat_cow)(void) {
    return BGEN_SYM(feat_cow)();
}

bool BGEN_API(feat_atomics)(void) {
    return BGEN_SYM(feat_atomics)();
}

int BGEN_API(feat_dims)(void) {
    return BGEN_SYM(feat_dims)();
}

void BGEN_API(clear)(BGEN_NODE **root, void *udata) {
    BGEN_SYM(clear)(root, udata);
}

bool BGEN_API(sane)(BGEN_NODE **root, void *udata) {
    return BGEN_SYM(sane)(root, udata);
}

size_t BGEN_API(count)(BGEN_NODE **root, void *udata) {
    return BGEN_SYM(count)(root, udata);
}

size_t BGEN_API(height)(BGEN_NODE **root, void *udata) {
    return BGEN_SYM(height)(root, udata);
}

int BGEN_API(index_of)(BGEN_NODE **root, BGEN_ITEM key, size_t *index,
    void *udata)
{
    return BGEN_SYM(index_of)(root, key, index, udata);
}

int BGEN_API(get)(BGEN_NODE **root, BGEN_ITEM key, BGEN_ITEM *item_out,
    void *udata)
{
    return BGEN_SYM(get)(root, key, item_out, udata);
}

int BGEN_API(get_mut)(BGEN_NODE **root, BGEN_ITEM key, BGEN_ITEM *item_out,
    void *udata)
{
    return BGEN_SYM(get_mut)(root, key, item_out, udata);
}

bool BGEN_API(contains)(BGEN_NODE **root, BGEN_ITEM key, void *udata) {
    return BGEN_SYM(contains)(root, key, udata);
}

int BGEN_API(insert)(BGEN_NODE **root, BGEN_ITEM item, BGEN_ITEM *olditem,
    void *udata)
{
    return BGEN_SYM(insert)(root, item, olditem, udata);
}

int BGEN_API(delete)(BGEN_NODE **root, BGEN_ITEM key, BGEN_ITEM *olditem, 
    void *udata)
{
    return BGEN_SYM(delete)(root, key, olditem, udata);
}

int BGEN_API(front)(BGEN_NODE **root, BGEN_ITEM *item_out, void *udata) {
    return BGEN_SYM(front)(root, item_out, udata);
}

int BGEN_API(front_mut)(BGEN_NODE **root, BGEN_ITEM *item_out, void *udata) {
    return BGEN_SYM(front_mut)(root, item_out, udata);
}

int BGEN_API(back)(BGEN_NODE **root, BGEN_ITEM *item_out, void *udata) {
    return BGEN_SYM(back)(root, item_out, udata);
}

int BGEN_API(back_mut)(BGEN_NODE **root, BGEN_ITEM *item_out, void *udata) {
    return BGEN_SYM(back_mut)(root, item_out, udata);
}

int BGEN_API(delete_at)(BGEN_NODE **root, size_t index, BGEN_ITEM *olditem,
    void *udata)
{
    return BGEN_SYM(delete_at)(root, index, olditem, udata);
}

int BGEN_API(replace_at)(BGEN_NODE **root, size_t index, BGEN_ITEM item,
    BGEN_ITEM *olditem, void *udata)
{
    return BGEN_SYM(replace_at)(root, index, item, olditem, udata);
}

int BGEN_API(pop_front)(BGEN_NODE **root, BGEN_ITEM *olditem, void *udata) {
    return BGEN_SYM(pop_front)(root, olditem, udata);
}

int BGEN_API(pop_back)(BGEN_NODE **root, BGEN_ITEM *olditem, void *udata) {
    return BGEN_SYM(pop_back)(root, olditem, udata);
}

int BGEN_API(get_at)(BGEN_NODE **root, size_t index, BGEN_ITEM *item,
    void *udata)
{
    return BGEN_SYM(get_at)(root, index, item, udata);
}

int BGEN_API(get_at_mut)(BGEN_NODE **root, size_t index, BGEN_ITEM *item,
    void *udata)
{
    return BGEN_SYM(get_at_mut)(root, index, item, udata);
}

int BGEN_API(push_front)(BGEN_NODE **root, BGEN_ITEM item, void *udata) {
    return BGEN_SYM(push_front)(root, item, udata);
}

int BGEN_API(push_back)(BGEN_NODE **root, BGEN_ITEM item, void *udata) {
    return BGEN_SYM(push_back)(root, item, udata);
}

int BGEN_API(insert_at)(BGEN_NODE **root, size_t index, BGEN_ITEM item,
    void *udata)
{
    return BGEN_SYM(insert_at)(root, index, item, udata);
}

int BGEN_API(copy)(BGEN_NODE **root, BGEN_NODE **newroot, void *udata) {
    return BGEN_SYM(copy)(root, newroot, udata);
}

int BGEN_API(clone)(BGEN_NODE **root, BGEN_NODE **newroot, void *udata) {
    return BGEN_SYM(clone)(root, newroot, udata);
}

int BGEN_API(compare)(BGEN_ITEM a, BGEN_ITEM b, void *udata) {
    return BGEN_SYM(compare)(a, b, udata);
}

bool BGEN_API(less)(BGEN_ITEM a, BGEN_ITEM b, void *udata) {
    return BGEN_SYM(less)(a, b, udata);
}

void BGEN_API(iter_init)(BGEN_NODE **root, BGEN_ITER *iter, void *udata) {
    BGEN_SYM(iter_init)(root, iter, udata);
}

void BGEN_API(iter_init_mut)(BGEN_NODE **root, BGEN_ITER *iter, void *udata) {
    BGEN_SYM(iter_init_mut)(root, iter, udata);
}

int BGEN_API(iter_status)(BGEN_ITER *iter) {
    return BGEN_SYM(iter_status)(iter);
}

bool BGEN_API(iter_valid)(BGEN_ITER *iter) {
    return BGEN_SYM(iter_valid)(iter);
}

void BGEN_API(iter_release)(BGEN_ITER *iter) {
    BGEN_SYM(iter_release)(iter);
}

void BGEN_API(iter_seek)(BGEN_ITER *iter, BGEN_ITEM key) {
    BGEN_SYM(iter_seek)(iter, key);
}

void BGEN_API(iter_seek_at)(BGEN_ITER *iter, size_t index) {
    BGEN_SYM(iter_seek_at)(iter, index);
}

void BGEN_API(iter_seek_at_desc)(BGEN_ITER *iter, size_t index) {
    BGEN_SYM(iter_seek_at_desc)(iter, index);
}

void BGEN_API(iter_seek_desc)(BGEN_ITER *iter, BGEN_ITEM key) {
    BGEN_SYM(iter_seek_desc)(iter, key);
}

void BGEN_API(iter_scan)(BGEN_ITER *iter) {
    BGEN_SYM(iter_scan)(iter);
}

void BGEN_API(iter_scan_desc)(BGEN_ITER *iter) {
    BGEN_SYM(iter_scan_desc)(iter);
}

void BGEN_API(iter_intersects)(BGEN_ITER *iter, BGEN_RTYPE min[],
    BGEN_RTYPE max[])
{
    BGEN_SYM(iter_intersects)(iter, min, max);
}

void BGEN_API(iter_nearby)(BGEN_ITER *iter, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS],
    void *target, void *udata))
{
    BGEN_SYM(iter_nearby)(iter, target, dist);
}

void BGEN_API(iter_next)(BGEN_ITER *iter) {
    BGEN_SYM(iter_next)(iter);
}

void BGEN_API(iter_item)(BGEN_ITER *iter, BGEN_ITEM *item) {
    BGEN_SYM(iter_item)(iter, item);
}

int BGEN_API(scan)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item, void *udata),
    void *udata)
{
    return BGEN_SYM(scan)(root, iter, udata);
}

int BGEN_API(scan_desc)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item, 
    void *udata), void *udata)
{
    return BGEN_SYM(scan_desc)(root, iter, udata);
}

int BGEN_API(seek)(BGEN_NODE **root, BGEN_ITEM key, bool(*iter)(BGEN_ITEM item,
    void *udata), void *udata)
{
    return BGEN_SYM(seek)(root, key, iter, udata);
}

int BGEN_API(seek_desc)(BGEN_NODE **root, BGEN_ITEM key, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    return BGEN_SYM(seek_desc)(root, key, iter, udata);
}

int BGEN_API(intersects)(BGEN_NODE **root, BGEN_RTYPE min[BGEN_DIMS], 
    BGEN_RTYPE max[BGEN_DIMS], bool(*iter)(BGEN_ITEM item, void *udata), 
    void *udata)
{
    return BGEN_SYM(intersects)(root, min, max, iter, udata);
}

int BGEN_API(seek_at)(BGEN_NODE **root, size_t index, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    return BGEN_SYM(seek_at)(root, index, iter, udata);
}

int BGEN_API(seek_at_mut)(BGEN_NODE **root, size_t index, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    return BGEN_SYM(seek_at_mut)(root, index, iter, udata);
}

int BGEN_API(seek_at_desc)(BGEN_NODE **root, size_t index, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    return BGEN_SYM(seek_at_desc)(root, index, iter, udata);
}

int BGEN_API(seek_at_desc_mut)(BGEN_NODE **root, size_t index, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    return BGEN_SYM(seek_at_desc_mut)(root, index, iter, udata);
}

int BGEN_API(scan_mut)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item,
    void *udata), void *udata)
{
    return BGEN_SYM(scan_mut)(root, iter, udata);
}

int BGEN_API(scan_desc_mut)(BGEN_NODE **root, bool(*iter)(BGEN_ITEM item, 
    void *udata), void *udata)
{
    return BGEN_SYM(scan_desc_mut)(root, iter, udata);
}

int BGEN_API(seek_mut)(BGEN_NODE **root, BGEN_ITEM key,
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    return BGEN_SYM(seek_mut)(root, key, iter, udata);
}

int BGEN_API(seek_desc_mut)(BGEN_NODE **root, BGEN_ITEM key, 
    bool(*iter)(BGEN_ITEM item, void *udata), void *udata)
{
    return BGEN_SYM(seek_desc_mut)(root, key, iter, udata);
}

int BGEN_API(intersects_mut)(BGEN_NODE **root, BGEN_RTYPE min[BGEN_DIMS], 
    BGEN_RTYPE max[BGEN_DIMS], bool(*iter)(BGEN_ITEM item, void *udata), 
    void *udata)
{
    return BGEN_SYM(intersects_mut)(root, min, max, iter, udata);
}

int BGEN_API(nearby)(BGEN_NODE **root, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS], 
    void *target, void *udata), bool(*iter)(BGEN_ITEM item, void *udata), 
    void *udata)
{
    return BGEN_SYM(nearby)(root, target, dist, iter, udata);
}

int BGEN_API(nearby_mut)(BGEN_NODE **root, void *target,
    BGEN_RTYPE(*dist)(BGEN_RTYPE min[BGEN_DIMS], BGEN_RTYPE max[BGEN_DIMS], 
    void *target, void *udata), bool(*iter)(BGEN_ITEM item, void *udata), 
    void *udata)
{
    return BGEN_SYM(nearby_mut)(root, target, dist, iter, udata);
}

/// Returns the minimum bounding rectangle for a spatial B-tree.
void BGEN_API(rect)(BGEN_NODE **root, BGEN_RTYPE min[BGEN_DIMS], 
    BGEN_RTYPE max[BGEN_DIMS], void *udata)
{
    BGEN_SYM(rect)(root, min, max, udata);
}

#endif // BGEN_DEFS

// undefine everything
// use `gcc -dM -E <source>` to help find leftover BGEN_* defines
#undef BGEN_COW
#undef BGEN_KEY
#undef BGEN_KEYTYPE
#undef BGEN_NOTFOUND
#undef BGEN_NOPATHHINT
#undef BGEN_NODE
#undef BGEN_ASSERT
#undef BGEN_MINITEMS
#undef BGEN_NOINLINE
#undef BGEN_DIMS
#undef BGEN_FREE
#undef BGEN_ITEM
#undef BGEN_REPLACED
#undef BGEN_SCANDESC
#undef BGEN_CC
#undef BGEN_C
#undef BGEN_PUSHFRONT
#undef BGEN_OUTOFORDER
#undef BGEN_NOATOMICS
#undef BGEN_BSEARCH
#undef BGEN_EXTERN
#undef BGEN_MAP
#undef BGEN_SYM
#undef BGEN_UNSUPPORTED
#undef BGEN_POPBACK
#undef BGEN_INSERTED
#undef BGEN_ITEMCOPY
#undef BGEN_MAXHEIGHT
#undef BGEN_INTERSECTS
#undef BGEN_MUSTSPLIT
#undef BGEN_ITEMFREE
#undef BGEN_ITEMRECT
#undef BGEN_MAXITEMS
#undef BGEN_KEYED
#undef BGEN_NOMEM
#undef BGEN_PITEM
#undef BGEN_REPAT
#undef BGEN_COPIED
#undef BGEN_DELKEY
#undef BGEN_RECT
#undef BGEN_SCAN
#undef BGEN_TYPE
#undef BGEN_API
#undef BGEN_POPFRONT
#undef BGEN_PUSHBACK
#undef BGEN_LINEAR
#undef BGEN_MALLOC
#undef BGEN_PQUEUE
#undef BGEN_BTREE
#undef BGEN_DELAT
#undef BGEN_DSIZE
#undef BGEN_NEARBY
#undef BGEN_SPATIAL
#undef BGEN_REALLOC
#undef BGEN_STOPPED
#undef BGEN_VALUETYPE
#undef BGEN_FINISHED
#undef BGEN_FANOUTUSED
#undef BGEN_POPMAX
#undef BGEN_VALUE
#undef BGEN_DEFS
#undef BGEN_NOORDER
#undef BGEN_RTYPE
#undef BGEN_SNODE
#undef BGEN_PATHHINT
#undef BGEN_DELETED
#undef BGEN_INSITEM
#undef BGEN_FANOUT
#undef BGEN_INLINE
#undef BGEN_ITER
#undef BGEN_LESS
#undef BGEN_NAME
#undef BGEN_COMPARE
#undef BGEN_COUNTED
#undef BGEN_FOUND
#undef BGEN_INSAT
