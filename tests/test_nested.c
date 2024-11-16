// Tests nested trees. 

#define TESTNAME "nested"
#define NOCOV // Not a base. ignore coverage 

#include "testutils.h"

#define BGEN_BTREE
#define BGEN_NAME     bt1
#define BGEN_TYPE     int
#define BGEN_COW
#define BGEN_MALLOC   { return malloc0(size); }
#define BGEN_FREE     { free0(ptr); }
#define BGEN_LESS     { return a < b; }
#include "../bgen.h"

struct col {
    atomic_int rc;
    char *name;
    struct bt1 *tree;
};

struct col *col_new(void) {
    struct col *col = malloc0(sizeof(struct col)); 
    assert(col);
    memset(col, 0, sizeof(struct col));
    col->name = malloc0(100);
    assert(col->name);
    col->name[0] = '\0';
    return col;
}

void col_free(struct col *col, void *udata) {
    bt1_clear(&col->tree, udata);
    free0(col->name);
    free0(col);
}

bool col_copy(struct col *col, struct col **copy, void *udata) {
    struct col *col2 = col_new();
    strcpy(col2->name, col->name);
    if (bt1_clone(&col->tree, &col2->tree, udata) != bt1_COPIED) {
        col_free(col2, udata);
        return false;
    }
    *copy = col2;
    return true;
}

#define BGEN_BTREE
#define BGEN_NAME     bt0
#define BGEN_TYPE     struct col*     /* pointer to a collection */
#define BGEN_COW
#define BGEN_MALLOC   { return malloc0(size); }
#define BGEN_FREE     { free0(ptr); }
#define BGEN_ITEMCOPY { return col_copy(item, copy, udata); }
#define BGEN_ITEMFREE { col_free(item, udata); }
#define BGEN_COMPARE  { return strcmp(a->name, b->name); }
#include "../bgen.h"

void test_clone(void) {
    testinit();
    struct bt0 *tree = 0;
    for (int i = 0; i < 1000; i++) {
        struct col *col = col_new();
        snprintf(col->name, 100, "col:%d", i);
        for (int j = 0; j < 1000; j++) {
            assert(bt1_insert(&col->tree, j, 0, 0) == bt1_INSERTED);
        }
        assert(bt0_insert(&tree, col, 0, 0) == bt0_INSERTED);
    }
    // clone the root
    struct bt0 *tree2 = 0;
    assert(bt0_clone(&tree, &tree2, 0) == bt0_COPIED);
    struct col *col;
    assert(bt0_delete(&tree2, &(struct col){.name="col:750"}, &col, 0) == bt0_DELETED);
    col_free(col, 0);
    assert(bt0_delete(&tree, &(struct col){.name="col:750"}, &col, 0) == bt0_DELETED);
    col_free(col, 0);
    bt0_clear(&tree2, 0);
    bt0_clear(&tree, 0);
    checkmem();
}

int main(void) {
    initrand();
    test_clone();
    return 0;
}
