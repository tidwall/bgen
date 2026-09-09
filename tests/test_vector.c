#define TESTNAME "vector"
#define NOCOV // Not a base. ignore coverage 
#include "testutils.h"

#define BTREE_NAME kv
#define BTREE_TYPE int
#define BTREE_COW
#define BTREE_COUNTED
#define BTREE_ASSERT
#define BTREE_FANOUT   16
#define BTREE_MALLOC   return malloc0(size);
#define BTREE_FREE     free0(ptr);
#define BTREE_NOORDER
#include "../btree.h"

static __thread int val = -1;
static __thread struct kv *tree = 0;
static __thread int *keys = 0;
static __thread int nkeys = 1000; // do not change this value
static __thread int asum = 0;

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

void tree_print(struct kv **root) {
    _kv_internal_print(root, stdout, pitem, 0);
}

void tree_print_dim(struct kv **root) {
    printf("\033[2m");
    tree_print(root);
    printf("\033[0m");
}

void tree_fill(void) {
    shuffle(keys, nkeys);
    assert(kv_insert(&tree, keys[0], 0, 0) == kv_UNSUPPORTED);
    for (int i = 0; i < nkeys; i++) {
        assert(kv_push_back(&tree, keys[i], 0) == kv_INSERTED);
    }
}

void test_basic(void) {
    testinit();

    tree_fill();
    assert(kv_sane(&tree, 0));

    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_get_at(&tree, i, &val, 0) == kv_FOUND);
        // printf("%d %d\n", val, keys[i]);
        assert(val == keys[i]);
    }

    for (int i = 0; i < nkeys; i++) {
        val = -1;
        assert(kv_get_at(&tree, i, &val, 0) == kv_FOUND);
        int val2 = -1;
        assert(kv_replace_at(&tree, i, keys[i]+1, &val2, 0) == kv_REPLACED);
        assert(val == val2);
        assert(kv_sane(&tree, 0));
    }

    kv_clear(&tree, 0);
    checkmem();

}

int main(void) {
    initrand();
    initkeys();

    test_basic();

    free(keys);

    return 0;
}
