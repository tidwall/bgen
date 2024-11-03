#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include "testutils.h"
#include "points.h"

#define OUTOFORDER 1

// #define SPATIAL
#ifndef DIMS
#define DIMS 2
#endif

static __thread uint64_t copysum = 0;
static __thread uint64_t freesum = 0;
static __thread int failcounter = 0;
static __thread int failrandom = 0;

bool item_copy(int item, int *copy, void *udata) {
    (void)udata;
    if (failrandom > 0) {
        if (rand()%(failrandom*100) == 0) {
            // printf("\n(((item_copy fail))\n");
            return false;
        }
    }
    copysum += item;
    *copy = item;
    return true;
}

void item_free(int item, void *udata) {
    (void)item;
    freesum += item;
    (void)udata;
}

#define PREDEFBASE 100000000
#define CITIESBASE 101000000


static __thread bool use_static_3d = false;

void item_rect(int item, double min[], double max[]) {
#ifndef SPATIAL
    (void)item;
    for (int i = 0; i < DIMS; i++) {
        min[i] = 0;
        max[i] = 0;
    }
#else
    double point[DIMS];
    if (item < PREDEFBASE) {
        if (DIMS > 0) point[0] = item;
        if (DIMS > 1) point[1] = item;
    } else if (item < CITIESBASE) {
        if (DIMS > 0) point[0] = getpointx(predef, item-PREDEFBASE);
        if (DIMS > 1) point[1] = getpointy(predef, item-PREDEFBASE);
    } else  {
        if (DIMS > 0) point[0] = getpointx(cities, item-CITIESBASE);
        if (DIMS > 1) point[1] = getpointy(cities, item-CITIESBASE);
    }
    if (DIMS > 2) {
        if (use_static_3d) {
            point[2] = 999;
        } else {
            point[2] = item;
        }
    }

    for (int i = 0; i < DIMS; i++) {
        min[i] = point[i];
        max[i] = point[i];
    }
#endif
}

void *malloc1(size_t size) {
    if (failcounter > 0) {
        failcounter--;
        if (failcounter == 0) {
            return 0;
        }
    }
    if (failrandom > 0) {
        if (rand()%failrandom == 0) {
            return 0;
        }
    }
    return malloc0(size);
}

void free1(void *ptr) {
    free0(ptr);
}

#define BGEN_BTREE
#define BGEN_NAME kv
#define BGEN_TYPE int
#define BGEN_COW
#ifdef COUNTED
#define BGEN_COUNTED
#endif
#ifdef SPATIAL
#define BGEN_SPATIAL
#define BGEN_ITEMRECT { item_rect(item, min, max); }
#define BGEN_DIMS     DIMS
#endif
#define BGEN_ASSERT
#define BGEN_FANOUT   16
#define BGEN_MALLOC   malloc1
#define BGEN_FREE     free1
#define BGEN_ITEMCOPY { return item_copy(item, copy, udata); }
#define BGEN_ITEMFREE { item_free(item, udata); }
#ifdef NOORDER
#define BGEN_NOORDER
#else
#ifdef LINEAR
#define BGEN_LINEAR
#define BGEN_LESS     { return a < b; }
#elif defined(BSEARCH)
#define BGEN_BSEARCH
#define BGEN_COMPARE  { return a < b ? -1 : a > b; }
#endif
#endif

#include "../bgen.h"

static __thread int val = -1;
static __thread struct kv *tree = 0;
static __thread int *keys = 0;
static __thread int nkeys = 1000; // do not change this value
static __thread uint64_t asum = 0;

void initkeys(void) {
    keys = (int*)malloc(nkeys * sizeof(int));
    assert(keys);
    for (int i = 0; i < nkeys; i++) {
        keys[i] = i*10;
        asum += keys[i];
    }
}

void pitem(int item, FILE *file, void *udata) {
    (void)udata;
    fprintf(file, "%d", item);
}

void prtype(double rtype, FILE *file, void *udata) {
    (void)udata;
    fprintf(file, "%.0f", rtype);
}

void tree_print(struct kv **root) {
    _kv_internal_print(root, stdout, pitem, prtype, 0);
}

void tree_print_dim(struct kv **root) {
    printf("\033[2m");
    tree_print(root);
    printf("\033[0m");
}

void tree_fill(void) {
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_insert(&tree, keys[i], 0, 0) == kv_INSERTED);
    }
}

void tree_fill_sorted(void) {
    sort(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_insert(&tree, keys[i], 0, 0) == kv_INSERTED);
    }
}

void tree_swap(struct kv *tree1, struct kv *tree2) {
    struct kv *tmp = tree1;
    tree1 = tree2;
    tree2 = tmp;
}

void test_sane(void) {
    // This test actually checks for "insane" trees.
    // The other tests continually check for sane trees.
    testinit();

    struct kv *tree = 0;
    assert(kv_sane(&tree, 0));

    // Various failures
    struct kv node = { 0 };
    tree = &node;
    node.len = 0;
    assert(kv_sane(&tree, 0) == false);
    node.len = 1000;
    assert(kv_sane(&tree, 0) == false);

    node.isleaf = 1;
    node.len = 1;
    node.items[0] = 1;
    assert(kv_sane(&tree, 0) == false);

    node.height = 1;
    assert(kv_sane(&tree, 0) == true);

    node.isleaf = 0;
    node.height = 1;
    assert(kv_sane(&tree, 0) == false);

    node.isleaf = 1;
    node.len = 2;
    node.items[1] = 0;
    assert(kv_sane(&tree, 0) == false);

    // Make a valid tree
    struct kv cnode0 = { .isleaf=1, .height=1, .len=8, .items = { 
        10, 20, 30, 40, 50, 60, 70, 80,
    }, };
    struct kv cnode1 = { .isleaf=1, .height=1, .len=8, .items = { 
        100, 110, 120, 130, 140, 150, 160, 170
    }, };

    node.isleaf = 0;
    node.len = 1;
    node.height = 2;
    node.items[0] = 90;
    node.children[0] = &cnode0;
    node.children[1] = &cnode1;
#ifdef COUNTED
    node.counts[0] = 8;
    node.counts[1] = 8;
#endif
#ifdef SPATIAL
    for (int i = 0; i < DIMS; i++) {
        node.rects[0].min[i] = 10;
        node.rects[0].max[i] = 90;
    }
    for (int i = 0; i < DIMS; i++) {
        node.rects[1].min[i] = 100;
        node.rects[1].max[i] = 170;
    }
#endif
    assert(kv_sane(&tree, 0) == true);

    // Break stuff
    cnode1.items[0] = 75;
    assert(kv_sane(&tree, 0) == false);
    cnode1.items[0] = 100;
    cnode1.height = 0;
    assert(kv_sane(&tree, 0) == false);
    cnode1.height = 1;

    cnode0.items[0] = 500;
    assert(kv_sane(&tree, 0) == false);
    cnode0.items[0] = 10;
    cnode0.height = 0;
    assert(kv_sane(&tree, 0) == false);
    cnode0.height = 1;
    assert(kv_sane(&tree, 0) == true);

    node.height = 20;
    assert(kv_sane(&tree, 0) == false);
    node.height = 2;

    cnode0.len = 0;
    assert(kv_sane(&tree, 0) == false);
    cnode0.len = 1000;
    assert(kv_sane(&tree, 0) == false);
    cnode0.len = 8;


    // finally unit test private function on broken leaf len
    cnode0.len = 0;
    assert(_kv_internal_sane0(&cnode0, 0, 2) == false);
    cnode0.len = 8;
    assert(_kv_internal_sane0(&cnode0, 0, 2) == true);
    

    // now add a bunch of ints untils the tree has a height of 3
    tree = 0;
    for (int i = 0; ; i++) {
        assert(kv_push_back(&tree, i, 0) == kv_INSERTED);
        if (kv_height(&tree, 0) == 3) {
            break;
        }
    }

    // break the first leaf
    int plen = tree->children[0]->children[0]->len;
    tree->children[0]->children[0]->len = 1;
    assert(kv_sane(&tree, 0) == false);
    tree->children[0]->children[0]->len = plen;
    assert(kv_sane(&tree, 0) == true);


    kv_clear(&tree, 0);


    checkmem();
}

void test_various(void) {
    testinit();
    seedrand();

    // touch all things
    _kv_internal_all_api_calls();
    _kv_internal_all_sym_calls();

    // check features

    assert(kv_feat_cow() == 1);
    assert(kv_feat_atomics() == 1);
#ifdef COUNTED
    assert(kv_feat_counted() == 1);
#else
    assert(kv_feat_counted() == 0);
#endif
#ifdef SPATIAL
    assert(kv_feat_spatial() == 1);
    assert(kv_feat_dims() == DIMS);
#else
    assert(kv_feat_spatial() == 0);
    assert(kv_feat_dims() == 0);
#endif
    assert(kv_feat_fanout() == 16);
    assert(kv_feat_maxheight() == 21);
    assert(kv_feat_maxitems() == 15);
    assert(kv_feat_minitems() == 7);
#ifdef NOORDER
    assert(kv_feat_ordered() == 0);
#else
    assert(kv_feat_ordered() == 1);
#endif
#ifdef BSEARCH
    assert(kv_feat_bsearch() == 1);
    assert(kv_feat_pathhint() == 1);
#else
    assert(kv_feat_bsearch() == 0);
    assert(kv_feat_pathhint() == 0);
#endif

    struct kv *tree = 0;
    assert(kv_sane(&tree, 0));
    int val;

    val = -1;
    assert(kv_get(&tree, 0, &val, 0) == kv_NOTFOUND);
    assert(val == -1);
    assert(kv_get_mut(&tree, 0, &val, 0) == kv_NOTFOUND);
    assert(val == -1);
    assert(kv_front(&tree, &val, 0) == kv_NOTFOUND);
    assert(val == -1);
    assert(kv_front_mut(&tree, &val, 0) == kv_NOTFOUND);
    assert(val == -1);
    assert(kv_back(&tree, &val, 0) == kv_NOTFOUND);
    assert(val == -1);
    assert(kv_back_mut(&tree, &val, 0) == kv_NOTFOUND);
    assert(val == -1);
    assert(kv_get_at(&tree, 0, &val, 0) == kv_NOTFOUND);
    assert(val == -1);
    assert(kv_get_at_mut(&tree, 0, &val, 0) == kv_NOTFOUND);
    assert(val == -1);

    // insert random values (also test min and max)
    int min = 99999999;
    int max = -99999999;
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_insert(&tree, keys[i], &val, 0) == kv_INSERTED);
        assert(val == -1);
        assert(kv_sane(&tree, 0));
        if (keys[i] < min) {
            val = -1;
            assert(kv_front(&tree, &val, 0) == kv_FOUND);
            assert(val == keys[i]);
            min = val;
        }
        if (keys[i] > max) {
            val = -1;
            assert(kv_back(&tree, &val, 0) == kv_FOUND);
            assert(val == keys[i]);
            max = val;
        }
    }


    assert(kv_insert(&tree, 999999, 0, 0) == kv_INSERTED);


    // test printing (internal function only)
    unlink("test.dat.out");
    FILE *file = fopen("test.dat.out", "wb+");
    assert(file);
    _kv_internal_print(&tree, file, pitem, prtype, 0);
    rewind(file);
    char *buf = malloc(100000);
    assert(buf);
    int n = fread(buf, 1, 100000, file);
    buf[n] = '\0';
    assert(strstr(buf, " 999999 "));
    fclose(file);
    unlink("test.dat.out");
    free(buf);

    assert(kv_delete(&tree, 999999, 0, 0) == kv_DELETED);

    // re-insert random values
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_insert(&tree, keys[i], &val, 0) == kv_REPLACED);
        assert(val == keys[i]);
        assert(kv_sane(&tree, 0));
    }

    // re-insert random values (no return value)
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_insert(&tree, keys[i], 0, 0) == kv_REPLACED);
        assert(kv_sane(&tree, 0));
    }

    // get random value (not exists)
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_get(&tree, keys[i]+1, &val, 0) == kv_NOTFOUND);
        assert(val == -1);
        val = -1;
        assert(kv_get_mut(&tree, keys[i]+1, &val, 0) == kv_NOTFOUND);
        assert(val == -1);
    }

    // get random value
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_get(&tree, keys[i], &val, 0) == kv_FOUND);
        assert(val == keys[i]);
        val = -1;
        assert(kv_get_mut(&tree, keys[i], &val, 0) == kv_FOUND);
        assert(val == keys[i]);
    }


    // // scan the tree in order
    // struct kv_iter iter;
    // kv_iter_init_mut(&tree, &iter, 0);
    // sort(keys, nkeys);
    // int ret = kv_iter_first(&iter);
    // assert(ret == kv_FOUND);
    // for (int i = 0; i < nkeys; i++) {
    //     val = -1;
    //     kv_iter_item(&iter, &val);
    //     assert(val == keys[i]);
    //     ret = kv_iter_next(&iter);
    //     if (i == nkeys-1) {
    //         assert(ret == kv_NOTFOUND);
    //     } else {
    //         assert(ret == kv_FOUND);
    //     }
    // }

    // // scan in reverse order
    // kv_iter_init_mut(&tree, &iter, 0);
    // ret = kv_iter_last(&iter);
    // assert(ret == kv_FOUND);
    // for (int i = nkeys-1; i >= 0; i--) {
    //     val = -1;
    //     kv_iter_item(&iter, &val);
    //     assert(val == keys[i]);
    //     ret = kv_iter_prev(&iter);
    //     if (i == 0) {
    //         assert(ret == kv_NOTFOUND);
    //     } else {
    //         assert(ret == kv_FOUND);
    //     }
    // }

    // get random value (no return value)
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_get(&tree, keys[i], 0, 0) == kv_FOUND);
    }

    // get random value (not exists)
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_get(&tree, keys[i]+1, 0, 0) == kv_NOTFOUND);
    }

    // contains 
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_contains(&tree, keys[i], 0));
    }

    // not contains 
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(!kv_contains(&tree, keys[i]+1, 0));
    }

    // delete random value
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_delete(&tree, keys[i], &val, 0) == kv_DELETED);
        assert(val == keys[i]);
        assert(kv_sane(&tree, 0));
    }

    // reinsert
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_insert(&tree, keys[i], 0, 0) == kv_INSERTED);
        assert(kv_sane(&tree, 0));
    }
 

    // delete random value using internal function
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(_kv_internal_delete0(&tree, 0, keys[i]+1, 0, 0, &val) == kv_NOTFOUND);
        assert(_kv_internal_delete0(&tree, 0, keys[i], 0, 0, &val) == kv_DELETED);
        assert(val == keys[i]);
        assert(kv_sane(&tree, 0));
    }
    


    // reinsert
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_insert(&tree, keys[i], 0, 0) == kv_INSERTED);
        assert(kv_sane(&tree, 0));
    }

    // replace
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_insert(&tree, keys[i], &val, 0) == kv_REPLACED);
        assert(val == keys[i]);
        assert(kv_sane(&tree, 0));
    }


    // replace using internal function so we can try the recursive route.
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(_kv_internal_insert0(&tree, 0, 0, keys[i], &val, 0) == kv_REPLACED);
        assert(val == keys[i]);
        assert(kv_sane(&tree, 0));
    }




    // replace
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_insert(&tree, keys[i], &val, 0) == kv_REPLACED);
        assert(val == keys[i]);
        assert(kv_sane(&tree, 0));
    }

    


    // get_at
    sort(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_get_at(&tree, i, &val, 0) == kv_FOUND);
        assert(val == keys[i]);
        assert(kv_get_at_mut(&tree, i, &val, 0) == kv_FOUND);
        assert(val == keys[i]);
    }
    // out of bounds
    val = -1;
    assert(kv_get_at(&tree, nkeys, &val, 0) == kv_NOTFOUND);
    assert(val == -1);
    assert(kv_get_at_mut(&tree, nkeys, &val, 0) == kv_NOTFOUND);
    assert(val == -1);

    // delete random values (not exists)
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_delete(&tree, keys[i]+1, &val, 0) == kv_NOTFOUND);
        assert(val == -1);
        assert(kv_sane(&tree, 0));
    }

    val = -1;
    assert(kv_get_at(&tree, nkeys, &val, 0) == kv_NOTFOUND);
    assert(val == -1);

    // delete random values at index
    double sum = 0;
    size_t count = kv_count(&tree, 0);
    while (count > 0) {
        val = -1;
        int index = rand() % count;
        assert(kv_delete_at(&tree, index, &val, 0) == kv_DELETED);
        assert(val != -1);
        sum += val;
        count--;
        assert(kv_count(&tree, 0) == count);
        assert(kv_sane(&tree, 0));
    }
    assert(sum == asum);
    assert(kv_delete_at(&tree, -1, 0, 0) == kv_NOTFOUND);

    kv_clear(&tree, 0);


    // push items to back
    sort(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_push_back(&tree, keys[i], 0) == kv_INSERTED);
        assert(kv_sane(&tree, 0));
        val = -1;
        assert(kv_front(&tree, &val, 0) == kv_FOUND);
        assert(val == keys[0]);
        assert(kv_front_mut(&tree, &val, 0) == kv_FOUND);
        assert(val == keys[0]);
        val = -1;
        assert(kv_back(&tree, &val, 0) == kv_FOUND);
        assert(val == keys[i]);
        assert(kv_back_mut(&tree, &val, 0) == kv_FOUND);
        assert(val == keys[i]);
    }
    assert(kv_push_back(&tree, 0, 0) == kv_OUTOFORDER);
    assert(kv_sane(&tree, 0));

    kv_clear(&tree, 0);

    // push items to front
    sort(keys, nkeys);
    for (int i = nkeys-1; i >= 0; i--) {
        assert(kv_push_front(&tree, keys[i], 0) == kv_INSERTED);
        assert(kv_sane(&tree, 0));
        val = -1;
        assert(kv_front(&tree, &val, 0) == kv_FOUND);
        assert(val == keys[i]);
        assert(kv_front_mut(&tree, &val, 0) == kv_FOUND);
        assert(val == keys[i]);
        val = -1;
        assert(kv_back(&tree, &val, 0) == kv_FOUND);
        assert(val == keys[nkeys-1]);
        assert(kv_back_mut(&tree, &val, 0) == kv_FOUND);
        assert(val == keys[nkeys-1]);
    }
    assert(kv_push_front(&tree, 9999999, 0) == kv_OUTOFORDER);
    assert(kv_sane(&tree, 0));

    kv_clear(&tree, 0);

    checkmem();
}


void test_pop_front(void) {
    testinit();
    kv_clear(&tree, 0);
    assert(kv_pop_front(&tree, 0, 0) == kv_NOTFOUND);
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_insert(&tree, keys[i], 0, 0) == kv_INSERTED);
    }
    assert(kv_sane(&tree, 0));
    sort(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_pop_front(&tree, &val, 0) == kv_DELETED);
        assert(val == keys[i]);
        assert(kv_sane(&tree, 0));
    }
    checkmem();
}

void test_pop_back(void) {
    testinit();
    kv_clear(&tree, 0);
    assert(kv_pop_back(&tree, 0, 0) == kv_NOTFOUND);
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_insert(&tree, keys[i], 0, 0) == kv_INSERTED);
    }
    assert(kv_sane(&tree, 0));
    sort(keys, nkeys);
    for (int i = nkeys-1; i >= 0; i--) {
        val = -1;
        assert(kv_pop_back(&tree, &val, 0) == kv_DELETED);
        assert(val == keys[i]);
        assert(kv_sane(&tree, 0));
    }
    checkmem();
}

void test_counted(void) {
    testinit();
    kv_clear(&tree, 0);
    sort(keys, nkeys);

    assert(kv_insert_at(&tree, -1, 0, 0) == kv_NOTFOUND);
    assert(kv_replace_at(&tree, -1, 0, 0, 0) == kv_NOTFOUND);
    assert(kv_replace_at(&tree, 0, 0, 0, 0) == kv_NOTFOUND);


    kv_clear(&tree, 0);

    // insert every other key
    int j = 0;
    for (int i = 1; i < nkeys; i+=2) {
        // printf("%d at %d\n", keys[i], j);
        assert(kv_insert_at(&tree, j, keys[i], 0) == kv_INSERTED);
        // tree_print_dim(&tree);
        assert(kv_sane(&tree, 0));
        j++;
    }

    // tree_print_dim(&tree);
    j = 0;
    for (int i = 0; i < nkeys; i+=2) {
        // printf("%d at %d\n", keys[i], j);
        assert(kv_insert_at(&tree, j, keys[i], 0) == kv_INSERTED);
        assert(kv_sane(&tree, 0));
        j+=2;
    }

    // check that all keys exists
    shuffle(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_get_at(&tree, keys[i]/10, &val, 0) == kv_FOUND);
        assert(val == keys[i]);
    }

    for (int i = 1; i < nkeys; i++) {
        assert(kv_insert_at(&tree, i-1, keys[i], 0) == kv_OUTOFORDER);
        assert(kv_insert_at(&tree, i, keys[i], 0) == kv_OUTOFORDER);
        assert(kv_insert_at(&tree, i+1, keys[i], 0) == kv_OUTOFORDER);
    }
    assert(kv_sane(&tree, 0));
    assert(kv_insert_at(&tree, nkeys+10, 9999999, 0) == kv_NOTFOUND);

    kv_clear(&tree, 0);

    for (int h = 0; h < 100; h++) {    
        shuffle(keys, nkeys);
        for (int i = 0; i < nkeys; i++) {
            int count = kv_count(&tree, 0);
            int j = count?(rand()%count):0;

            int pval = -1;
            int nval = 99999999;
            if (j > 0) {
                assert(kv_get_at(&tree, j-1, &pval, 0) == kv_FOUND);
                assert(pval != -1);
            }
            if (j < count) {    
                assert(kv_get_at(&tree, j, &nval, 0) == kv_FOUND);
                assert(nval != -1);
            }
            int ret = kv_insert_at(&tree, j, keys[i], 0);
            assert(ret == kv_INSERTED || ret == kv_OUTOFORDER);
            if (ret == kv_INSERTED) {
                assert(keys[i] > pval);
                assert(keys[i] < nval);
                if (nval < 99999999) {
                    val = -1;
                    assert(kv_get_at(&tree, j+1, &val, 0) == kv_FOUND);
                    assert(val == nval);
                }
            }
        }
    }
    assert(kv_sane(&tree, 0));
    kv_clear(&tree, 0);

    sort(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_insert_at(&tree, i, keys[i], 0) == kv_INSERTED);
    }
    assert(kv_sane(&tree, 0));

    for (int i = 0; i < nkeys; i++) {
        size_t pos = -1;
        assert(kv_index_of(&tree, keys[i], &pos, 0) == kv_FOUND);
        assert(pos == (size_t)i);
        val = -1;
        assert(kv_get_at(&tree, pos, &val, 0) == kv_FOUND);
        assert(val == keys[i]);
    }

    for (int i = 0; i < nkeys; i++) {
        assert(kv_index_of(&tree, keys[i]+1, 0, 0) == kv_NOTFOUND);
    }

    kv_clear(&tree, 0);
    assert(kv_index_of(&tree, 0, 0, 0) == kv_NOTFOUND);

    checkmem();
}

void test_copy_or_clone(bool clone) {
    
    tree_fill();

    // Copy btree
    copysum = 0;
    // freesum = 0;
    struct kv *tree2 = 0;
    if (clone) {
        assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
        assert(copysum == 0);
    } else {
        assert(kv_copy(&tree, &tree2, 0) == kv_COPIED);
        assert(copysum == asum);
    }
    for (int i = 0; i < nkeys; i++) {
        assert(kv_contains(&tree, keys[i], 0));
    }
    copysum = 0;
    // freesum = 0;

    sort(keys, nkeys);
    // delete half the items from tree 2. every other key.
    for (int i = 0; i < nkeys; i += 2) {
        val = -1;
        
        assert(kv_delete(&tree2, keys[i], &val, 0) == kv_DELETED);
        assert(val == keys[i]);
    }

    if (clone) {
        // tree_print_dim(&tree);
        assert(copysum == asum);
    } else {
        assert(copysum == 0);
    }

    // make sure all items still exist in first btree
    for (int i = 0; i < nkeys; i++) {
        assert(kv_contains(&tree, keys[i], 0));
    }


    kv_clear(&tree2, 0);
    kv_clear(&tree, 0);

    // check that copy works on an empty tree
    if (clone) {
        assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
    } else {
        assert(kv_copy(&tree, &tree2, 0) == kv_COPIED);
    }
}

void test_copy(void) {
    testinit();
    test_copy_or_clone(false);
    checkmem();
}

void test_clone(void) {
    testinit();
    test_copy_or_clone(true);
    checkmem();
}

// The ac_loopstep is where a bunch of atomic-cow operations will occur.
// It's important that upon returns the tree contains all the same items that
// it started with.
void ac_loopstep(void) {
    failrandom = 2;
    // each case runs different scenarios
    switch (rand()%8) {
    case 0:
        // delete()
        shuffle(keys, nkeys);
        for (int i = 0; i < nkeys; i += 10) {
            assert(kv_contains(&tree, keys[i], 0));
            int ret = kv_delete(&tree, keys[i], 0, 0);
            if (ret == kv_NOMEM) {
                i -= 10;
                continue;
            }
            assert(ret == kv_DELETED);
            assert(!kv_contains(&tree, keys[i], 0));
        }
        assert(kv_sane(&tree, 0));
        for (int i = 0; i < nkeys; i += 10) {
            int ret = kv_delete(&tree, keys[i], 0, 0);
            if (ret == kv_NOMEM) {
                i -= 10;
                continue;
            }
            assert(kv_delete(&tree, keys[i], 0, 0) == kv_NOTFOUND);
        }
        assert(kv_sane(&tree, 0));
        for (int i = 0; i < nkeys; i += 10) {
            int ret = kv_insert(&tree, keys[i], 0, 0);
            if (ret == kv_NOMEM) {
                i -= 10;
                continue;
            }
            assert(ret == kv_INSERTED);
        }
        break;
    case 1:
        // get_mut()
        shuffle(keys, nkeys);
        for (int i = 0; i < nkeys; i += 10) {
            val = -1;
            int ret = kv_get_mut(&tree, keys[i], &val, 0);
            if (ret == kv_NOMEM) {
                i -= 10;
                continue;
            }
            assert(val == keys[i]);
            assert(ret == kv_FOUND);
        }
        break;
    case 2:
        // set()
        shuffle(keys, nkeys);
        for (int i = 0; i < nkeys; i += 10) {
            int ret = kv_insert(&tree, keys[i]+1, 0, 0);
            if (ret == kv_NOMEM) {
                i -= 10;
                continue;
            }
            assert(ret == kv_INSERTED);
        }
        assert(kv_sane(&tree, 0));
        for (int i = 0; i < nkeys; i += 10) {
            int ret = kv_delete(&tree, keys[i]+1, 0, 0);
            if (ret == kv_NOMEM) {
                i -= 10;
                continue;
            }
            assert(ret == kv_DELETED);
        }
        break;
    case 3:
        // interal_insert0()
        shuffle(keys, nkeys);
        for (int i = 0; i < nkeys; i += 10) {
            int ret = _kv_internal_insert0(&tree, 0, 0, keys[i]+1, 0, 0);
            if (ret == kv_NOMEM) {
                i -= 10;
                continue;
            }
            assert(ret == kv_INSERTED);
        }
        assert(kv_sane(&tree, 0));
        for (int i = 0; i < nkeys; i += 10) {
            int ret = kv_delete(&tree, keys[i]+1, 0, 0);
            if (ret == kv_NOMEM) {
                i -= 10;
                continue;
            }
            assert(ret == kv_DELETED);
        }
        break;
    case 4:
        // front_mut()
        sort(keys, nkeys);
        while (1) {
            val = -1;
            int ret = kv_front_mut(&tree, &val, 0);
            if (ret == kv_NOMEM) {
                continue;
            }
            assert(val == keys[0]);
            assert(ret == kv_FOUND);
            break;
        }
        break;
    case 5:
        // last_mut()
        sort(keys, nkeys);
        while (1) {
            val = -1;
            int ret = kv_back_mut(&tree, &val, 0);
            if (ret == kv_NOMEM) {
                continue;
            }
            assert(val == keys[nkeys-1]);
            assert(ret == kv_FOUND);
            break;
        }
        break;
    case 6:
        // get_at_mut()
        sort(keys, nkeys);
        for (int i = 0; i < nkeys; i += 10) {
            val = -1;
            int ret = kv_get_at_mut(&tree, i, &val, 0);
            if (ret == kv_NOMEM) {
                i -= 10;
                continue;
            }
            assert(val == keys[i]);
            assert(ret == kv_FOUND);
        }
        break;
    case 7:
        // pop_front()
        sort(keys, nkeys);
        while (1) {
            val = -1;
            int ret = kv_pop_front(&tree, &val, 0);
            if (ret == kv_NOMEM) {
                continue;
            }
            assert(val == keys[0]);
            assert(ret == kv_DELETED);
            break;
        }
        while (1) {
            int ret = kv_insert(&tree, keys[0], 0, 0);
            if (ret == kv_NOMEM) {
                continue;
            }
            assert(ret == kv_INSERTED);
            break;
        }
        
        break;


    }
    assert(kv_sane(&tree, 0));
    failrandom = 0;
}

void ac_dostuff(double duration) {
    double start = now();

    while (now()-start < duration) {
        usleep((rand()%100000)+1000); // 1 to 100 ms
        ac_loopstep();
    }
    kv_clear(&tree, 0);
}

struct thread_context {
    struct kv *tree;
    int depth;
    int nthreads;
    double duration;
};

void thread_step(int nthreads, int depth, double duration);

void *thread(void *arg) {
    struct thread_context *ctx = (struct thread_context *)arg;
    initkeys();
    tree = ctx->tree;
    thread_step(ctx->nthreads, ctx->depth, ctx->duration);
    free(keys);
    free(ctx);
    return 0;
}

pthread_t start_clone_thread(int nthreads, int depth, double duration) {
    struct kv *tree2 = 0;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
    // assert(kv_copy(&tree, &tree2, 0) == kv_COPIED);
    pthread_t th;
    struct thread_context *ctx = (struct thread_context *)malloc(sizeof(struct thread_context));
    assert(ctx);
    memset(ctx, 0, sizeof(struct thread_context));
    ctx->tree = tree2;
    ctx->depth = depth;
    ctx->nthreads = nthreads;
    ctx->duration = duration;
    assert(!pthread_create(&th, 0, thread, ctx));
    return th;
}


void thread_step(int nthreads, int depth, double duration) {
    pthread_t *threads = (pthread_t *)malloc(nthreads*sizeof(pthread_t));
    assert(threads);
    if (depth > 0) {
        for (int i = 0; i < nthreads; i++) {
            threads[i] = start_clone_thread(nthreads, depth-1, duration);
        }
    }
    ac_dostuff(duration);
    if (depth > 0) {
        for (int i = 0; i < nthreads; i++) {
            pthread_join(threads[i], 0);
        }
    }
    free(threads);
}

void test_cow_opts(bool usethreads) {
    tree_fill();
    int depth = 4; // clone depth 
    int nthreads = 4; // number of threads per depth
    double duration = 1.0; // seconds
    if (usethreads) {
        // multi-threaded
        thread_step(nthreads, depth, duration);
    } else {
        // single-threaded
        int ntrees = depth*nthreads;
        struct kv **trees = (struct kv **)malloc(ntrees*sizeof(struct kv*));
        assert(trees);
        for (int i = 0; i < ntrees; i++) {
            assert(kv_clone(&tree, &trees[i], 0) == kv_COPIED);
        }
        double start = now();
        while (now()-start < duration) {
            for (int i = 0; i < ntrees; i++) {
                tree_swap(tree, trees[i]);
                ac_loopstep();
                tree_swap(tree, trees[i]);
            }
        }
        for (int i = 0; i < ntrees; i++) {
            kv_clear(&trees[i], 0);
        }
        kv_clear(&tree, 0);
        free(trees);
    }
}

void test_cow(void) {
    testinit();
    test_cow_opts(0);
    test_cow_opts(1);
    checkmem();
}

void test_cow_threads(void) {
    testinit();
    test_cow_opts(true);
    checkmem();
}

void test_cow_coroutines(void) {
    testinit();
    test_cow_opts(false);
    checkmem();
}

double mmin(double x, double y) {
    return x < y ? x : y;
}

double mmax(double x, double y) {
    return x > y ? x : y;
}

double box_dist(double amin[], double amax[], double bmin[], double bmax[]) {
    double dist = 0.0;
    double squared = 0.0;
    for (int i = 0; i < DIMS; i++) {
        squared = mmax(amin[i], bmin[i]) - mmin(amax[i], bmax[i]);
        if (squared > 0) {
            dist += squared * squared;
        }
    }
    return dist;
}

double dist_noop(double min[], double max[], void *target, void *udata) {
    (void)min, (void)max, (void)target, (void)udata;
    return rand_double();
    // return box_dist(min, max, (double[]){ nkeys/2, nkeys/2, nkeys/2},
    //     (double[]){ nkeys/2, nkeys/2, nkeys/2});
}

bool siter_noop(int item, void *udata) {
    (void)item, (void)udata;
    return true;
}

void test_failures(void) {
    testinit();
    const int K = 20;
    const int N = 500;
    // Test insertion errors using a random malloc failures
    failrandom = 2;
    for (int ii = 0; ii < 10; ii++) {
        // insert
        shuffle(keys, nkeys);
        for (int i = 0; i < nkeys; i++) {
            int ret = kv_insert(&tree, keys[i], 0, 0);
            if (ret == kv_NOMEM) {
                i--;
                continue;
            }
            assert(ret == kv_INSERTED);
        }
        assert(kv_sane(&tree, 0));

        // push_last
        kv_clear(&tree, 0);
        sort(keys, nkeys);
        for (int i = 0; i < nkeys; i++) {
            int ret = kv_push_back(&tree, keys[i], 0);
            if (ret == kv_NOMEM) {
                i--;
                continue;
            }
            assert(ret == kv_INSERTED);
        }
        assert(kv_sane(&tree, 0));

        // push_first
        kv_clear(&tree, 0);
        sort(keys, nkeys);
        for (int i = nkeys-1; i >= 0; i--) {
            int ret = kv_push_front(&tree, keys[i], 0);
            if (ret == kv_NOMEM) {
                i++;
                continue;
            }
            assert(ret == kv_INSERTED);
        }
        assert(kv_sane(&tree, 0));
        kv_clear(&tree, 0);
    }
    failrandom = 0;


    // Test copy failures
    shuffle(keys, nkeys);
    int j = 0;
    while (kv_height(&tree, 0) < 3) {
        assert(kv_insert(&tree, j++, 0, 0) == kv_INSERTED);
    }

    failrandom = 3;
    struct kv *tree2 = 0;
    int n = 0;
    while (1) {
        int ret = kv_copy(&tree, &tree2, 0);
        if (ret == kv_COPIED) {
            break;
        }
        n++;
    }
    (void)n;
    // printf("took %d tries to copy (%zu items).\n", n, kv_count(&tree, 0));
    kv_clear(&tree2, 0);

    failrandom = 0;

    kv_clear(&tree, 0);

    // test various nomem failures
    double start = now();
    while (now() - start < 1) {
        tree_fill();
        struct kv *tree2 = 0;

        // push_first
        kv_clear(&tree2, 0);
        assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
        failrandom = 0;
        failcounter = 1;
        assert(_kv_internal_delete0(&tree, 0, 0, 0, 0, &val) == kv_NOMEM);
        failcounter = 0;

        failrandom = 2;
        int j = 0; 

        // push_first
        j = 0;
        while (j < N) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            copysum = freesum = 0;
            int ret = kv_push_front(&tree, -(j+1), 0);
            if (ret == kv_NOMEM) {
                continue;
            }
            assert(ret == kv_INSERTED);
            j++;
        }

        // push_last
        j = 0;
        while (j < N) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            copysum = freesum = 0;
            int ret = kv_push_back(&tree, 1000000+j, 0);
            if (ret == kv_NOMEM) {
                continue;
            }
            assert(ret == kv_INSERTED);
            j++;
        }

        j = 0;
        while (j < N) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            copysum = freesum = 0;
            int ret = kv_pop_front(&tree, 0, 0);
            if (ret == kv_NOMEM) {
                continue;
            }
            assert(ret == kv_DELETED);
            j++;
        }

        j = 0;
        while (j < N) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            copysum = freesum = 0;
            int ret = kv_pop_back(&tree, 0, 0);
            if (ret == kv_NOMEM) {
                continue;
            }
            assert(ret == kv_DELETED);
            j++;
        }

        // (scan)
        for (int k = 0; k < K; k++) {
            j = k;
            while (j < N) {
                kv_clear(&tree2, 0);
                assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
                copysum = freesum = 0;
                struct kv_iter iter;
                kv_iter_init_mut(&tree, &iter, 0);
                kv_iter_scan(&iter);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                while (kv_iter_valid(&iter)) {
                    kv_iter_next(&iter);
                }
                j++;
            }
        }

        // (scan_mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                if (kv_scan_mut(&tree, siter_noop, 0) == kv_NOMEM) {
                    continue;
                }
                j++;
            }
        }

        // (iter_scan_desc) (mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                struct kv_iter iter;
                kv_iter_init_mut(&tree, &iter, 0);
                kv_iter_scan_desc(&iter);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                kv_iter_next(&iter);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                while (kv_iter_valid(&iter)) {
                    kv_iter_next(&iter);
                }
                j++;
            }
        }

        // (scan_desc_mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                if (kv_scan_desc_mut(&tree, siter_noop, 0) == kv_NOMEM) {
                    continue;
                }
                j++;
            }
        }

        // (iter_seek) (mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                struct kv_iter iter;
                kv_iter_init_mut(&tree, &iter, 0);
                kv_iter_seek(&iter, j);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                kv_iter_next(&iter);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                while (kv_iter_valid(&iter)) {
                    kv_iter_next(&iter);
                }
                j++;
            }
        }

        // (seek_mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                if (kv_seek_mut(&tree, j, siter_noop, 0) == kv_NOMEM) {
                    continue;
                }
                j++;
            }
        }

        // (iter_seek_desc) (mut)
        // failrandom = 4;
        int l = 0;
        for (int k = 0; k < K; k++) {
            j = k;
            while (j < N) {
                kv_clear(&tree2, 0);
                assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
                copysum = freesum = 0;
                struct kv_iter iter;
                kv_iter_init_mut(&tree, &iter, 0);
                kv_iter_seek_desc(&iter, (l++)%kv_count(&tree, 0));
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                kv_iter_next(&iter);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                while (kv_iter_valid(&iter)) {
                    kv_iter_next(&iter);
                }
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                j++;
            }
        }

        // (seek_desc_mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                if (kv_seek_desc_mut(&tree, j, siter_noop, 0) == kv_NOMEM) {
                    continue;
                }
                j++;
            }
        }

        // (iter_seek_at) (mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                struct kv_iter iter;
                kv_iter_init_mut(&tree, &iter, 0);
                kv_iter_seek_at(&iter, j);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                kv_iter_next(&iter);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                while (kv_iter_valid(&iter)) {
                    kv_iter_next(&iter);
                }
                j++;
            }
        }

        // (seek_at_mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                if (kv_seek_at_mut(&tree, j, siter_noop, 0) == kv_NOMEM) {
                    continue;
                }
                j++;
            }
        }

        // (iter_seek_at_desc) (mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                struct kv_iter iter;
                kv_iter_init_mut(&tree, &iter, 0);
                kv_iter_seek_at_desc(&iter, j);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                kv_iter_next(&iter);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                while (kv_iter_valid(&iter)) {
                    kv_iter_next(&iter);
                }
                j++;
            }
        }

        // (seek_at_mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                if (kv_seek_at_desc_mut(&tree, j, siter_noop, 0) == kv_NOMEM) {
                    continue;
                }
                j++;
            }
        }


#ifdef SPATIAL
        double min[DIMS];
        double max[DIMS];
        for (int d = 0; d < DIMS; d++) {
            min[d] = 0;
        }
        for (int d = 0; d < DIMS; d++) {
            max[d] = 99999999;
        }

        // (iter_intersects) (mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = k;
            while (j < N) {
                copysum = freesum = 0;
                struct kv_iter iter;
                kv_iter_init_mut(&tree, &iter, 0);
                kv_iter_intersects(&iter, min, max);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                kv_iter_next(&iter);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    continue;
                }
                while (kv_iter_valid(&iter)) {
                    kv_iter_next(&iter);
                }
                j++;
            }
        }

        // (intersects_mut)
        for (int k = 0; k < K; k++) {
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            j = 0;
            while (j < N) {
                copysum = freesum = 0;
                if (kv_intersects_mut(&tree, min, max, siter_noop, 0) == kv_NOMEM) {
                    continue;
                }
                j++;
            }
        }

        // (nearby_mut)
        for (int k = 0; k < K; k++) {
            j = 0;
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            while (j < N) {
                copysum = freesum = 0;
                if (kv_nearby_mut(&tree, 0, dist_noop, siter_noop, 0) == kv_NOMEM) {
                    continue;
                }
                j++;
            }
        }

        // (iter_nearby) (mut)
        for (int k = 0; k < K; k++) {
            j = k;
            kv_clear(&tree2, 0);
            assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
            while (j < N) {
                copysum = freesum = 0;
                struct kv_iter iter;
                kv_iter_init_mut(&tree, &iter, 0);
                kv_iter_nearby(&iter, 0, dist_noop);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    kv_iter_release(&iter);
                    continue;
                }
                kv_iter_next(&iter);
                if (kv_iter_status(&iter) == kv_NOMEM) {
                    kv_iter_release(&iter);
                    continue;
                }
                while (kv_iter_valid(&iter)) {
                    kv_iter_next(&iter);
                }
                kv_iter_release(&iter);
                j++;
            }
        }
#endif
        failrandom = 0;
        kv_clear(&tree2, 0);
        kv_clear(&tree, 0);
    }
    checkmem();
}


void test_push(void) {
    testinit();
    kv_clear(&tree, 0);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_push_back(&tree, keys[i], 0) == kv_INSERTED);
        assert(kv_push_back(&tree, keys[i], 0) == kv_OUTOFORDER);
    }
    kv_clear(&tree, 0);
    for (int i = nkeys-1; i >= 0; i--) {
        assert(kv_push_front(&tree, keys[i], 0) == kv_INSERTED);
        assert(kv_push_front(&tree, keys[i], 0) == kv_OUTOFORDER);
    }
    
    kv_clear(&tree, 0);
    checkmem();
}

void test_compare(void) {
    testinit();
    assert(kv_compare(1, 2, 0) == -1);
    assert(kv_compare(1, 1, 0) == 0);
    assert(kv_compare(1, 0, 0) == 1);
    assert(kv_less(1, 2, 0) == true);
    assert(kv_less(1, 1, 0) == false);
    assert(kv_less(1, 0, 0) == false);
    checkmem();
}

void test_replace_at(void) {
    testinit();
    tree_fill();
    sort(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_get_at(&tree, i, &val, 0) == kv_FOUND);
        assert(val == keys[i]);
        if (i > 0) {
            val = -1;
            assert(kv_replace_at(&tree, i-1, keys[i], &val, 0) == kv_OUTOFORDER);
            assert(val == -1);
        }
        if (i < nkeys-1) {
            val = -1;
            assert(kv_replace_at(&tree, i+1, keys[i], &val, 0) == kv_OUTOFORDER);
            assert(val == -1);
        }
        val = -1;
        assert(kv_replace_at(&tree, i, keys[i]+1, &val, 0) == kv_REPLACED);
        assert(val == keys[i]);

        val = -1;
        assert(kv_get_at(&tree, i, &val, 0) == kv_FOUND);
        assert(val == keys[i]+1);
        assert(kv_sane(&tree, 0));
        
    }
    kv_clear(&tree, 0);
    checkmem();
}

struct iiter_ctx {
    int limit;
    int count;
    double sum;
    char *label;
};

bool iiter(int item, void *udata) {
    struct iiter_ctx *ctx = udata;
    if (ctx->limit == 0) {
        return false;
    }
    ctx->limit--;
    ctx->count++;
    ctx->sum += item;
    if (ctx->count == 1) {
        // printf("%s %d\n", ctx->label, item);
    }
    return true;
} 

bool tintersects(double amin[], double amax[], double bmin[], double bmax[]) {
    int bits = 0;
    for (int i = 0; i < DIMS; i++) {
        bits |= bmin[i] > amax[i];
        bits |= bmax[i] < amin[i];
    }
    return bits == 0;
}

void slow_intersects(double min[], double max[], void *udata) {
    (void)min, (void)max, (void)udata;
#ifdef SPATIAL
    sort(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        double kmin[DIMS], kmax[DIMS];
        item_rect(keys[i], kmin, kmax);
        if (tintersects(kmin, kmax, min, max)) {
            if (!iiter(keys[i], udata)) {
                return;
            }
        }
    }
#endif
}

bool test_intersects_opt2(char *label, int a, int b, int stop, bool mut) {
    bool doit = stop > 0;
if (doit) {
    // printf("> %d %d\n", a, b);
}

    void(*iter_init)(struct kv **root, struct kv_iter *iter, void *udata);
    int(*intersects)(struct kv **root, double min[], double max[], 
        bool(*iter)(int item, void *udata), void *udata);
    if (mut) {
        iter_init = kv_iter_init_mut;
        intersects = kv_intersects_mut;
    } else {
        iter_init = kv_iter_init;
        intersects = kv_intersects;
    }


    struct kv *tree2 = tree;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);

    if (mut) {
        // delete half the items
        for (int i = 0; i < nkeys; i+=2) {
            assert(kv_delete(&tree, keys[i], 0, 0) == kv_DELETED);
        }
    }

    double min[DIMS];
    double max[DIMS];
    for (int i = 0; i < DIMS; i++) {
        min[i] = a;
        max[i] = b;
    }
    sort(keys, nkeys);
    int count = 0;
    double sum = 0;
    struct iiter_ctx ctx1 = (struct iiter_ctx){ .label="B",.limit = stop };
    struct iiter_ctx ctx2 = (struct iiter_ctx){ .label="C",.limit = stop };
    struct iiter_ctx ctx3 = (struct iiter_ctx){ .label="D",.limit = stop };
#ifdef SPATIAL
    for (int i = 0; i < nkeys; i++) {
        if (keys[i] >= a && keys[i] <= b) {
            if (stop <= 0) {
                break;
            }
            stop--;
            count++;
            sum += keys[i];
        }
    }
#endif
    slow_intersects(min, max, &ctx1);
    intersects(&tree2, min, max, iiter, &ctx2);

    struct kv_iter iter;
    iter_init(&tree2, &iter, 0);
    kv_iter_intersects(&iter, min, max);
#ifdef SPATIAL
    for (; kv_iter_valid(&iter); kv_iter_next(&iter)) {
        kv_iter_item(&iter, &val);
        if (!iiter(val, &ctx3)) {
            break;
        }
    }
#else
    assert(!kv_iter_valid(&iter));
#endif
    if (!(ctx3.count == ctx2.count && ctx2.count == ctx1.count && 
        ctx1.count == count))
    {
        printf("%d %d %d %d\n", ctx1.count, ctx2.count, ctx3.count, count);
        fprintf(stderr, "%s: count mismatch\n", label);
        return false;
    }
    if (!(ctx3.sum == ctx2.sum && ctx2.sum == ctx1.sum && ctx1.sum == sum)) {
        fprintf(stderr, "%s: sum mismatch\n", label);
        return false;
    }

    kv_clear(&tree, 0);
    tree = tree2;
    return true;
}


void test_intersects_opt(bool mut) {
    void(*iter_init)(struct kv **root, struct kv_iter *iter, void *udata);
    // int(*intersects)(struct kv **root, double min[], double max[], void *udata);
    if (mut) {
        iter_init = kv_iter_init_mut;
    } else {
        iter_init = kv_iter_init;
    }


    // test iter_intersects on an empty tree
    struct kv_iter iter;
    iter_init(&tree, &iter, 0);
    double min[DIMS] = { 0 };
    double max[DIMS] = { 0 };
    kv_iter_intersects(&iter, min, max);
    assert(!kv_iter_valid(&iter));

    tree_fill();


    struct kv *tree2 = tree;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);

    if (mut) {
        // delete half the items
        for (int i = 0; i < nkeys; i+=2) {
            assert(kv_delete(&tree, keys[i], 0, 0) == kv_DELETED);
        }
    }

    // try intersects to find each item
    sort(keys, nkeys);
    iter_init(&tree2, &iter, 0);
    for (int i = 0; i < nkeys; i++) {
        bool found = false;
        item_rect(keys[i], min, max);
        kv_iter_intersects(&iter, min, max);
        while (kv_iter_valid(&iter)) {
            kv_iter_item(&iter, &val);
            if (val == keys[i]) {
                found = true;
            }
            kv_iter_next(&iter);
        }
#ifdef SPATIAL
        assert(found);
#else
        assert(!found);
#endif
    }

    kv_clear(&tree, 0);
    tree = tree2;

    for (int i = 0; i < 100; i++) {
        assert(test_intersects_opt2("i", 3000+i, 4000+i, i, mut));
    }
    assert(test_intersects_opt2("F", 3000, 7000, 9999999, mut));
    assert(test_intersects_opt2("G", 7000, 3000, 0, mut));
    assert(test_intersects_opt2("H", 7000, 3000, 1, mut));
    assert(test_intersects_opt2("I", 7000, 3000, 10, mut));
    assert(test_intersects_opt2("J", 7000, 3000, 100, mut));
    assert(test_intersects_opt2("K", 7000, 3000, 1000, mut));
    assert(test_intersects_opt2("L", 7000, 3000, 9999999, mut));
    assert(test_intersects_opt2("M", 300, 700, 0, mut));
    assert(test_intersects_opt2("N", 300, 700, 1, mut));
    assert(test_intersects_opt2("O", 300, 700, 10, mut));
    assert(test_intersects_opt2("P", 300, 700, 100, mut));
    assert(test_intersects_opt2("Q", 300, 700, 1000, mut));
    assert(test_intersects_opt2("R", 300, 700, 9999999, mut));

    kv_clear(&tree, 0);
}

void test_intersects(void) {
    testinit();
    for (int i = 0; i < 10; i++) {
        test_intersects_opt(0);
        test_intersects_opt(1);
    }
    checkmem();
}

struct siter_ctx {
    int limit;
    int count;
    double sum;
    bool stopped;
};

bool siter(int item, void *udata) {
    struct siter_ctx *ctx = udata;
    if (ctx->limit == 0) {
        ctx->stopped = true;
        return false;
    }
    // printf("%d\n", item);
    ctx->limit--;
    ctx->count++;
    ctx->sum += item;
    return true;
} 


void slow_scan(bool(*iter)(int item, void *udata), void *udata) {
    sort(keys, nkeys);
    for (int i = 0; i < nkeys; i++) {
        if (!iter(keys[i], udata)) {
            break;
        }
    }
}

void slow_scan_desc(bool(*iter)(int item, void *udata), void *udata) {
    sort(keys, nkeys);
    for (int i = nkeys-1; i >= 0; i--) {
        if (!iter(keys[i], udata)) {
            break;
        }
    }
}

void slow_seek(int pivot, bool(*iter)(int item, void *udata), void *udata) {
    sort(keys, nkeys);
    int i = 0;
    for (; i < nkeys; i++) {
        if (pivot <= keys[i]) {
            break;
        }
    }
    for (; i < nkeys; i++) {
        if (!iter(keys[i], udata)) {
            break;
        }
    }
}

void slow_seek_desc(int pivot, bool(*iter)(int item, void *udata), void *udata) {
    sort(keys, nkeys);
    int i = nkeys-1;
    for (; i >= 0; i--) {
        if (pivot >= keys[i]) {
            break;
        }
    }
    for (; i >= 0; i--) {
        if (!iter(keys[i], udata)) {
            break;
        }
    }
}

void slow_seek_at(size_t index, bool(*iter)(int item, void *udata), void *udata) {
    sort(keys, nkeys);
    for (size_t i = 0; i < (size_t)nkeys; i++) {
        if (i >= index) {
            if (!iter(keys[i], udata)) {
                break;
            }
        }
    }
}

void slow_seek_at_desc(size_t index, bool(*iter)(int item, void *udata), void *udata) {
    sort(keys, nkeys);
    for (int i = nkeys-1; i >= 0; i--) {
        if ((size_t)i <= index) {
            if (!iter(keys[i], udata)) {
                break;
            }
        }
    }
}

void test_scan_opt(bool mut) {
    struct siter_ctx ctx;
    struct kv_iter iter;

    void(*iter_init)(struct kv **root, struct kv_iter *iter, void *udata);
    int(*scan)(struct kv **root, bool(*iter)(int item, void *udata), void *udata);
    if (mut) {
        iter_init = kv_iter_init_mut;
        scan = kv_scan_mut;
    } else {
        iter_init = kv_iter_init;
        scan = kv_scan;
    }

    iter_init(&tree, &iter, 0);
    kv_iter_scan(&iter);
    assert(!kv_iter_valid(&iter));

    tree_fill();

    ctx = (struct siter_ctx){ 0 };
    ctx.limit = 9999999;
    assert(scan(&tree, siter, &ctx) == kv_FINISHED);
    assert(ctx.sum == asum);


    struct kv *tree2 = tree;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);

    if (mut) {
        // delete half the items
        for (int i = 0; i < nkeys; i+=2) {
            assert(kv_delete(&tree, keys[i], 0, 0) == kv_DELETED);
        }
    }

    ctx = (struct siter_ctx){ .limit = 9999999 };
    iter_init(&tree2, &iter, 0);
    kv_iter_scan(&iter);
    while (kv_iter_valid(&iter)) {
        kv_iter_item(&iter, &val);
        if (!siter(val, &ctx)) {
            break;
        }
        kv_iter_next(&iter);
    }
    kv_iter_release(&iter); // not really needed for scan
    assert(ctx.sum == asum);

    for (int i = 0; i < 150; i++) {
        ctx = (struct siter_ctx){ .limit = i };
        slow_scan(siter, &ctx);
        assert(ctx.count == i);
        double bsum = ctx.sum;

        ctx = (struct siter_ctx){ .limit = i };
        assert(scan(&tree2, siter, &ctx) == kv_STOPPED);
        assert(ctx.count == i);
        assert(ctx.sum == bsum);

        ctx = (struct siter_ctx){ .limit = i };
        iter_init(&tree2, &iter, 0);
        kv_iter_scan(&iter);
        while (kv_iter_valid(&iter)) {
            kv_iter_item(&iter, &val);
            if (!siter(val, &ctx)) {
                break;
            }
            kv_iter_next(&iter);
        }
        assert(ctx.count == i);
        assert(ctx.sum == bsum);
    }

    kv_clear(&tree2, 0);
    kv_clear(&tree, 0);
}


void test_scan(void) {
    testinit();
    test_scan_opt(0);
    test_scan_opt(1);
    checkmem();
}


void test_scan_desc_opt(bool mut) {
    
    struct siter_ctx ctx;
    struct kv_iter iter;

    void(*iter_init)(struct kv **root, struct kv_iter *iter, void *udata);
    int(*scan_desc)(struct kv **root, bool(*iter)(int item, void *udata), void *udata);
    if (mut) {
        iter_init = kv_iter_init_mut;
        scan_desc = kv_scan_desc_mut;
    } else {
        iter_init = kv_iter_init;
        scan_desc = kv_scan_desc;
    }

    iter_init(&tree, &iter, 0);
    kv_iter_scan(&iter);
    assert(!kv_iter_valid(&iter));

    tree_fill();

    ctx = (struct siter_ctx){ 0 };
    ctx.limit = 9999999;
    assert(scan_desc(&tree, siter, &ctx) == kv_FINISHED);
    assert(ctx.sum == asum);


    struct kv *tree2 = tree;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);

    if (mut) {
        // delete half the items
        for (int i = 0; i < nkeys; i+=2) {
            assert(kv_delete(&tree, keys[i], 0, 0) == kv_DELETED);
        }
    }

    ctx = (struct siter_ctx){ .limit = 9999999 };
    iter_init(&tree2, &iter, 0);
    kv_iter_scan_desc(&iter);
    while (kv_iter_valid(&iter)) {
        kv_iter_item(&iter, &val);
        if (!siter(val, &ctx)) {
            break;
        }
        kv_iter_next(&iter);
    }
    assert(ctx.sum == asum);

    for (int i = 0; i < 150; i++) {
        ctx = (struct siter_ctx){ .limit = i };
        slow_scan_desc(siter, &ctx);
        assert(ctx.count == i);
        double bsum = ctx.sum;

        ctx = (struct siter_ctx){ .limit = i };
        assert(scan_desc(&tree2, siter, &ctx) == kv_STOPPED);
        assert(ctx.count == i);
        assert(ctx.sum == bsum);

        ctx = (struct siter_ctx){ .limit = i };
        iter_init(&tree2, &iter, 0);
        kv_iter_scan_desc(&iter);
        while (kv_iter_valid(&iter)) {
            kv_iter_item(&iter, &val);
            if (!siter(val, &ctx)) {
                break;
            }
            kv_iter_next(&iter);
        }
        assert(ctx.count == i);
        assert(ctx.sum == bsum);
    }

    kv_clear(&tree, 0);
    kv_clear(&tree2, 0);
}

void test_scan_desc(void) {
    testinit();
    test_scan_desc_opt(0);
    test_scan_desc_opt(1);
    checkmem();
}

void test_seek_opt(bool mut) {
    struct siter_ctx ctx;
    struct kv_iter iter;

    void(*iter_init)(struct kv **root, struct kv_iter *iter, void *udata);
    int(*seek)(struct kv **root, int pivot, bool(*iter)(int item, void *udata), void *udata);
    if (mut) {
        iter_init = kv_iter_init_mut;
        seek = kv_seek_mut;
    } else {
        iter_init = kv_iter_init;
        seek = kv_seek;
    }

    iter_init(&tree, &iter, 0);
    kv_iter_seek(&iter, 0);
    assert(!kv_iter_valid(&iter));

    tree_fill();


    struct kv *tree2 = tree;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);

    if (mut) {
        // delete half the items
        for (int i = 0; i < nkeys; i+=2) {
            assert(kv_delete(&tree, keys[i], 0, 0) == kv_DELETED);
        }
    }

    sort(keys, nkeys);
    for (int i = 0; i <= keys[nkeys-1]; i += 5) {
        for (int j = 0; j <= 100; j += 10) {
            int limit = j == 100 ? 9999999 : j;

            ctx = (struct siter_ctx){ .limit = limit };
            slow_seek(i, siter, &ctx);
            int count = ctx.count;
            double bsum = ctx.sum;
            bool stopped = ctx.stopped;

            ctx = (struct siter_ctx){ .limit = limit };
            int ret = seek(&tree2, i, siter, &ctx);
            if (stopped) {
                assert(ret == kv_STOPPED);
            } else {
                assert(ret == kv_FINISHED);
            }
            assert(ctx.count == count);
            assert(ctx.sum == bsum);

            ctx = (struct siter_ctx){ .limit = limit };
            iter_init(&tree2, &iter, 0);
            kv_iter_seek(&iter, i);
            while (kv_iter_valid(&iter)) {
                kv_iter_item(&iter, &val);
                if (!siter(val, &ctx)) {
                    break;
                }
                kv_iter_next(&iter);
            }
            assert(ctx.count == count);
            assert(ctx.sum == bsum);
        }
    }

    kv_clear(&tree2, 0);
    kv_clear(&tree, 0);
}

void test_seek(void) {
    testinit();
    test_seek_opt(0);
    test_seek_opt(1);
    checkmem();
}

void test_seek_desc_opt(bool mut) {
    void(*iter_init)(struct kv **root, struct kv_iter *iter, void *udata);
    int(*seek_desc)(struct kv **root, int pivot, bool(*iter)(int item, void *udata), void *udata);
    if (mut) {
        iter_init = kv_iter_init_mut;
        seek_desc = kv_seek_desc_mut;
    } else {
        iter_init = kv_iter_init;
        seek_desc = kv_seek_desc;
    }

    struct siter_ctx ctx;
    struct kv_iter iter;

    // test iter with empty trees
    iter_init(&tree, &iter, 0);
    kv_iter_seek_desc(&iter, 99999999);
    assert(!kv_iter_valid(&iter));

    tree_fill();


    struct kv *tree2 = tree;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);

    if (mut) {
        // delete half the items
        for (int i = 0; i < nkeys; i+=2) {
            assert(kv_delete(&tree, keys[i], 0, 0) == kv_DELETED);
        }
    }


    sort(keys, nkeys);

    for (int i = keys[nkeys-1]; i >= 0 ; i -= 5) {
        for (int j = 0; j <= 100; j += 10) {
            int limit = j == 100 ? 9999999 : j;

            ctx = (struct siter_ctx){ .limit = limit };
            slow_seek_desc(i, siter, &ctx);
            bool stopped = ctx.stopped;
            int count = ctx.count;
            double bsum = ctx.sum;

            ctx = (struct siter_ctx){ .limit = limit };
            int ret = seek_desc(&tree2, i, siter, &ctx);
            if (stopped) {
                assert(ret == kv_STOPPED);
            } else {
                assert(ret == kv_FINISHED);
            }
            assert(ctx.count == count);
            assert(ctx.sum == bsum);

            ctx = (struct siter_ctx){ .limit = limit };
            iter_init(&tree2, &iter, 0);
            kv_iter_seek_desc(&iter, i);
            while (kv_iter_valid(&iter)) {
                kv_iter_item(&iter, &val);
                if (!siter(val, &ctx)) {
                    break;
                }
                kv_iter_next(&iter);
            }
            assert(ctx.count == count);
            assert(ctx.sum == bsum);
        }
    }

    kv_clear(&tree2, 0);
    kv_clear(&tree, 0);
}


void test_seek_desc(void) {
    testinit();
    test_seek_desc_opt(0);
    test_seek_desc_opt(1);
    checkmem();
}

void test_seek_at_opt(bool mut) {
    struct siter_ctx ctx;
    struct kv_iter iter;

    void(*iter_init)(struct kv **root, struct kv_iter *iter, void *udata);
    int(*seek_at)(struct kv **root, size_t index, bool(*iter)(int item, void *udata), void *udata);
    if (mut) {
        iter_init = kv_iter_init_mut;
        seek_at = kv_seek_at_mut;
    } else {
        iter_init = kv_iter_init;
        seek_at = kv_seek_at;
    }

    iter_init(&tree, &iter, 0);
    kv_iter_seek_at(&iter, 0);
    assert(!kv_iter_valid(&iter));

    tree_fill();

    iter_init(&tree, &iter, 0);
    kv_iter_seek_at(&iter, 999999);
    assert(!kv_iter_valid(&iter));


    struct kv *tree2 = tree;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);

    if (mut) {
        // delete half the items
        for (int i = 0; i < nkeys; i+=2) {
            assert(kv_delete(&tree, keys[i], 0, 0) == kv_DELETED);
        }
    }

    for (int i = 0; i < nkeys; i++) {
        for (int j = 0; j <= 100; j += 10) {
            int limit = j == 100 ? 9999999 : j;

            ctx = (struct siter_ctx){ .limit = limit };
            // printf("slow %d:%d\n", i, limit);
            slow_seek_at(i, siter, &ctx);
            int count = ctx.count;
            double bsum = ctx.sum;
            bool stopped = ctx.stopped;
            // printf("========\n");
            // printf("seek_at %d:%d\n", i, limit);
            ctx = (struct siter_ctx){ .limit = limit };
            int ret = seek_at(&tree2, i, siter, &ctx);
            if (stopped) {
                assert(ret == kv_STOPPED);
            } else {
                assert(ret == kv_FINISHED);
            }
            assert(ctx.count == count);
            assert(ctx.sum == bsum);

            // printf("========\n");
            // printf("iter_seek_at %d:%d\n", i, limit);
            ctx = (struct siter_ctx){ .limit = limit };
            iter_init(&tree2, &iter, 0);
            kv_iter_seek_at(&iter, i);
            while (kv_iter_valid(&iter)) {
                kv_iter_item(&iter, &val);
                if (!siter(val, &ctx)) {
                    break;
                }
                kv_iter_next(&iter);
            }
            assert(ctx.count == count);
            assert(ctx.sum == bsum);
        }
    }

    kv_clear(&tree2, 0);
    kv_clear(&tree, 0);
}

void test_seek_at(void) {
    testinit();
    test_seek_at_opt(0);
    test_seek_at_opt(1);
    checkmem();
}

void test_seek_at_desc_opt(bool mut) {
    struct siter_ctx ctx;
    struct kv_iter iter;

    void(*iter_init)(struct kv **root, struct kv_iter *iter, void *udata);
    int(*seek_at_desc)(struct kv **root, size_t index, bool(*iter)(int item, void *udata), void *udata);
    if (mut) {
        iter_init = kv_iter_init_mut;
        seek_at_desc = kv_seek_at_desc_mut;
    } else {
        iter_init = kv_iter_init;
        seek_at_desc = kv_seek_at_desc;
    }

    iter_init(&tree, &iter, 0);
    kv_iter_seek_at_desc(&iter, 0);
    assert(!kv_iter_valid(&iter));

    tree_fill();

    iter_init(&tree, &iter, 0);
    kv_iter_seek_at_desc(&iter, 999999);
    assert(kv_iter_valid(&iter));

    ctx = (struct siter_ctx){ .limit = 1 };
    int ret = seek_at_desc(&tree, 999999, siter, &ctx);
    assert(ret == kv_STOPPED);
    assert(ctx.count == 1);


    struct kv *tree2 = tree;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);

    if (mut) {
        // delete half the items
        for (int i = 0; i < nkeys; i+=2) {
            assert(kv_delete(&tree, keys[i], 0, 0) == kv_DELETED);
        }
    }

    for (int i = nkeys-1; i >= 0; i--) {
        for (int j = 0; j <= 100; j += 10) {
            int limit = j == 100 ? 9999999 : j;

            ctx = (struct siter_ctx){ .limit = limit };
            // printf("slow %d:%d\n", i, limit);
            slow_seek_at_desc(i, siter, &ctx);
            int count = ctx.count;
            double bsum = ctx.sum;
            bool stopped = ctx.stopped;
            // printf("========\n");
            // printf("seek_at %d:%d\n", i, limit);
            ctx = (struct siter_ctx){ .limit = limit };
            int ret = seek_at_desc(&tree2, i, siter, &ctx);
            if (stopped) {
                assert(ret == kv_STOPPED);
            } else {
                assert(ret == kv_FINISHED);
            }
            assert(ctx.count == count);
            assert(ctx.sum == bsum);

            // printf("========\n");
            // printf("iter_seek_at %d:%d\n", i, limit);
            ctx = (struct siter_ctx){ .limit = limit };
            iter_init(&tree2, &iter, 0);
            kv_iter_seek_at_desc(&iter, i);
            while (kv_iter_valid(&iter)) {
                kv_iter_item(&iter, &val);
                if (!siter(val, &ctx)) {
                    break;
                }
                kv_iter_next(&iter);
            }
            assert(ctx.count == count);
            assert(ctx.sum == bsum);

            // printf("%d:%d:%d:%.0f\n", i, limit, count, bsum);

        }
    }

    kv_clear(&tree2, 0);
    kv_clear(&tree, 0);
}



void test_seek_at_desc(void) {
    testinit();
    test_seek_at_desc_opt(0);
    test_seek_at_desc_opt(1);
    checkmem();
}


struct point {
    double x;
    double y;
};

struct nctx {
    struct point point;
    int count;
    int *items;
    int limit;
};

double ndist(double min[], double max[], void *target, void *udata) {
    (void)udata;
    double *point = target;
    return box_dist(point, point, min, max);
}

bool niter(int item, void *udata) {
    struct nctx *ctx = udata;
    ctx->items[ctx->count++] = item;
    if (ctx->count == ctx->limit) {
        return false;
    }
    return true;
}

struct nitem {
    double dist;
    int item;
};

int ncompare(const void *a, const void *b) {
    const struct nitem *na = a;
    const struct nitem *nb = b;
    return na->dist < nb->dist ? -1 :
           na->dist > nb->dist ? 1 :
           na->item < nb->item ? -1 : 
           na->item > nb->item;
}

int slow_nearby(struct kv **root, void *target,
    double(*dist)(double min[DIMS], double max[DIMS], 
    void *target, void *udata), bool(*iter)(int item, void *udata), 
    void *udata, bool mut)
{
#ifndef SPATIAL
    (void)root, (void)target, (void)dist, (void)iter, (void)udata, (void)mut;
    return kv_FINISHED;
#else
    struct nitem *items = malloc(sizeof(struct nitem) * kv_count(root, udata));
    assert(items);
    struct kv_iter iter2;
    if (mut) {
        kv_iter_init_mut(root, &iter2, udata);
    } else {
        kv_iter_init(root, &iter2, udata);
    }
    kv_iter_scan(&iter2);
    int count = 0;
    while (kv_iter_valid(&iter2)) {
        kv_iter_item(&iter2, &val);
        double min[DIMS], max[DIMS];
        item_rect(val, min, max);
        items[count].dist = dist(min, max, target, udata);
        items[count].item = val;
        count++;
        kv_iter_next(&iter2);
    }
    assert(kv_iter_status(&iter2) == 0);
    kv_iter_release(&iter2);
    qsort(items, count, sizeof(struct nitem), ncompare);
    int status = kv_FINISHED;
    for (int i = 0; i < count; i++) {
        if (!iter(items[i].item, udata)) {
            status = kv_STOPPED;
            break;
        }
    }
    free(items);
    return status;
#endif
}

void test_nearby_opt(bool mut) {
    tree = 0;
    if (mut) {
        assert(kv_nearby_mut(&tree, 0, ndist, niter, 0) == kv_FINISHED);
    } else {
        assert(kv_nearby(&tree, 0, ndist, niter, 0) == kv_FINISHED);
    }

    struct kv_iter iter;
    kv_iter_init(&tree, &iter, 0);
    kv_iter_nearby(&iter, 0, ndist);
    assert(kv_iter_valid(&iter) == false);
    

#ifndef SPATIAL
    return;
#else
    int count = ncities;
    for (int i = 0; i < count; i++) {
        int item = CITIESBASE+i;
        assert(kv_insert(&tree, item, 0, 0) == kv_INSERTED);
    }
    double point[] = { -112.0, 33.0, count/2 };

    struct kv *tree2, *tree3;
    assert(kv_clone(&tree, &tree2, 0) == kv_COPIED);
    assert(kv_clone(&tree, &tree3, 0) == kv_COPIED);

    if (mut) {
        // delete half the items
        for (int i = 0; i < count; i+=2) {
            int item = CITIESBASE+i;
            assert(kv_delete(&tree, item, 0, 0) == kv_DELETED);
        }
    }

    struct nctx ctx1 = { .limit = 10000000 };
    ctx1.items = malloc(sizeof(int) * count);
    assert(ctx1.items);
    if (mut) {
        assert(kv_nearby_mut(&tree2, point, ndist, niter, &ctx1) == kv_FINISHED);
    } else {
        assert(kv_nearby(&tree2, point, ndist, niter, &ctx1) == kv_FINISHED);
    }

    assert(ctx1.count == count);
    
    struct nctx ctx2 = { .limit = 10000000 };
    ctx2.items = malloc(sizeof(int) * count);
    assert(ctx2.items);
    assert(slow_nearby(&tree2, point, ndist, niter, &ctx2, mut) == kv_FINISHED);
    assert(ctx2.count == count);
    for (int i = 0; i < count; i++) {
        assert(ctx1.items[i] == ctx2.items[i]);
    }

    struct nctx ctx3 = { .limit = 10000000 };
    ctx3.items = malloc(sizeof(int) * count);
    assert(ctx3.items);
    if (mut) {
        kv_iter_init_mut(&tree3, &iter, &ctx3);
    } else {
        kv_iter_init(&tree3, &iter, &ctx3);
    }
    kv_iter_nearby(&iter, point, ndist);
    while (kv_iter_valid(&iter)) {
        kv_iter_item(&iter, &val);
        if (!niter(val, &ctx3)) {
            break;
        }
        kv_iter_next(&iter);
    }
    kv_iter_release(&iter);

    assert(ctx3.count == count);
    for (int i = 0; i < count; i++) {
        assert(ctx2.items[i] == ctx3.items[i]);
    }


    ctx1.count = 0;
    ctx1.limit = count/2;
    if (mut) {
        assert(kv_nearby_mut(&tree2, point, ndist, niter, &ctx1) == kv_STOPPED);
    } else {
        assert(kv_nearby(&tree2, point, ndist, niter, &ctx1) == kv_STOPPED);
    }
    assert(ctx1.count == count/2);

    ctx2.count = 0;
    ctx2.limit = count/2;
    assert(slow_nearby(&tree2, point, ndist, niter, &ctx2, mut) == kv_STOPPED);
    assert(ctx2.count == count/2);



    ctx3.count = 0;
    ctx3.limit = count/2;
    
    if (mut) {
        kv_iter_init_mut(&tree3, &iter, &ctx3);
    } else {
        kv_iter_init(&tree3, &iter, &ctx3);
    }
    kv_iter_nearby(&iter, point, ndist);
    while (kv_iter_valid(&iter)) {
        kv_iter_item(&iter, &val);
        if (!niter(val, &ctx3)) {
            break;
        }
        kv_iter_next(&iter);
    }
    // printf("============\n");
    kv_iter_scan(&iter); // switch to the different scanner
    // printf("============\n");


    kv_iter_release(&iter);

    assert(ctx3.count == count/2);

    free(ctx1.items);
    free(ctx2.items);
    free(ctx3.items);
    kv_clear(&tree, 0);
    kv_clear(&tree2, 0);
    kv_clear(&tree3, 0);
#endif
}

void test_nearby(void) {
    testinit();
    use_static_3d = true;
    test_nearby_opt(0);
    test_nearby_opt(1);
    use_static_3d = false;
    checkmem();
}


void riter(double *min, double *max, int depth, void *udata) {
    (void)depth, (void)udata;
    // printf("( ");
    for (int i = 0 ;i < DIMS; i++) {
        assert(!(min[i] > max[i]));
        // printf("%f ", min[i]);
    }
    // for (int i = 0 ;i < DIMS; i++) {
        // printf("%f ", max[i]);
    // }
    // printf(")\n");
}

void check_rect(void) {
    double min[DIMS] = { 0 };
    double max[DIMS] = { 0 };
    double imin[DIMS] = { 0 };
    double imax[DIMS] = { 0 };
    struct kv_iter iter;
    kv_iter_init(&tree, &iter, 0);
    kv_iter_scan(&iter);
    int i = 0;
    while (kv_iter_valid(&iter)) {
        kv_iter_item(&iter, &val);
        if (i == 0) {
            item_rect(val, min, max);
        } else {
            item_rect(val, imin, imax);
            for (int j = 0; j < DIMS; j++) {
                if (imin[j] < min[j]) {
                    min[j] = imin[j];
                }
                if (imax[j] > max[j]) {
                    max[j] = imax[j];
                }
            }
        }
        kv_iter_next(&iter);
        i++;
    }
    kv_rect(&tree, imin, imax, 0);
    for (int i = 0; i < DIMS; i++) {
        assert(!(min[i] < imin[i] || min[i] > imin[i]));
        assert(!(max[i] < imax[i] || max[i] > imax[i]));
    }
}

void test_rect(void) {
    testinit();
    tree_fill();
    _kv_internal_scan_rects(&tree, riter, 0);
    check_rect();
    kv_clear(&tree, 0);
    for (int i = 0; i < 5; i++) {
        assert(kv_insert(&tree, i, 0, 0) == kv_INSERTED);
    }
    check_rect();

    kv_clear(&tree, 0);
    check_rect();

    checkmem();
}

int main(void) {
    initrand();
    initkeys();

    test_sane();
    test_counted();
    test_push();
    test_pop_front();
    test_pop_back();
    test_replace_at();
    test_copy();
    test_clone();
    test_cow_threads();
    test_cow_coroutines();
    test_various();
    test_compare();
    test_failures();
    test_intersects();
    test_nearby();
    test_scan();
    test_scan_desc();
    test_seek();
    test_seek_desc();
    test_seek_at();
    test_seek_at_desc();
    test_rect();

    free(keys);

    return 0;
}
