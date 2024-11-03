#include <stdio.h>
#include "testutils.h"


#define M 16

int N = 1000000;
int G = 50;
int C = 0;        // -1 = worse-case, 0 = average, +1 = best-case

// #define COW
// #define COUNTED
// #define SPATIAL
// #define NOATOMIC
// #define BSEARCH
// #define NOPATHHINT
// #define PATHHINT
// #define USECOMPARE

#define BGEN_NAME      kv
#define BGEN_TYPE      int
#define BGEN_MALLOC    malloc0
#define BGEN_FREE      free0
#ifdef COW
#define BGEN_COW
#endif
#ifdef COUNTED
#define BGEN_COUNTED
#endif
#ifdef SPATIAL
#define BGEN_SPATIAL
#endif
#ifdef NOATOMIC
#define BGEN_NOATOMIC
#endif
#ifdef BSEARCH
#define BGEN_BSEARCH
#endif
#ifdef NOPATHHINT
#define BGEN_NOPATHHINT
#endif
#ifdef USEPATHHINT
#define BGEN_PATHHINT
#endif
#define BGEN_FANOUT M
// #define BGEN_ITEMRECT  { min[0] = item; min[1] = item; max[0] = item; max[1] = item; }
#ifdef USECOMPARE
#define BGEN_COMPARE      { return a < b ? -1 : a > b; }
#else
#define BGEN_LESS      { return a < b; }
#endif
#include "../bgen.h"

static bool iter_scan(int item, void *udata) {
    double *sum = udata;
    (*sum) += item;
    return true;
}

#define reset_tree() { \
    kv_clear(&tree, 0); \
    shuffle(keys, N); \
    for (int i = 0; i < N; i++) { \
        kv_insert(&tree, keys[i], &val, 0); \
    } \
}(void)0

#define run_op(label, nruns, preop, op) {\
    double gelapsed = C < 1 ? 0 : 9999; \
    double gstart = now(); \
    double gmstart = mtotal; \
    double gmtotal = 0; \
    for (int g = 0; g < (nruns); g++) { \
        printf("\r%-20s", label); \
        printf("%d/%d ", g+1, (nruns)); \
        fflush(stdout); \
        preop \
        double start = now(); \
        size_t mstart = mtotal; \
        op \
        double elapsed = now()-start; \
        if (C == -1) { \
            if (elapsed > gelapsed) { \
                gelapsed = elapsed; \
            } \
        } else if (C == 0) { \
            gelapsed += elapsed; \
        } else { \
            if (elapsed < gelapsed) { \
                gelapsed = elapsed; \
            } \
        } \
        if (mtotal > mstart) { \
            gmtotal += mtotal-mstart; \
        } \
    } \
    printf("\r"); \
    printf("%-19s", label); \
    if (C == 0) { \
        gelapsed /= nruns; \
    } \
    bench_print_mem(N, gstart, gstart+gelapsed, \
        gmstart, gmstart+(gmtotal/(nruns))); \
    assert(kv_sane(&tree, 0)); \
    reset_tree(); \
}(void)0

int main(void) {
    if (getenv("N")) {
        N = atoi(getenv("N"));
    }
    if (getenv("G")) {
        G = atoi(getenv("G"));
    }
    if (getenv("C")) {
        C = atoi(getenv("C"));
    }
    printf("Benchmarking %d items, %d times, taking the %s result\n", 
        N, G, C == -1 ? "worst": C == 0 ? "average" : "best");

    // _kv_internal_print_feats(stdout);

    seedrand();
    double asum = 0;
    int *keys = malloc(N * sizeof(int));
    assert(keys);
    for (int i = 0; i < N; i++) {
        keys[i] = i*10;
        asum += keys[i];
    }
    assert(asum > 0);

    struct kv *tree = 0;
    int val;

    run_op("insert(seq)", G, {
        kv_clear(&tree, 0);
        sort(keys, N);
    },{
        for (int i = 0; i < N; i++) {
            assert(kv_insert(&tree, keys[i], &val, 0) == kv_INSERTED);
        }
    });

    run_op("insert(rand)", G, {
        kv_clear(&tree, 0);
        shuffle(keys, N);
    },{
        for (int i = 0; i < N; i++) {
            assert(kv_insert(&tree, keys[i], &val, 0) == kv_INSERTED);
        }
    });

    run_op("get(seq)", G, {
        sort(keys, N);
    }, {
        for (int i = 0; i < N; i++) {
            assert(kv_get(&tree, keys[i], &val, 0) == kv_FOUND);
        }
    });

    run_op("get(rand)", G, {
        shuffle(keys, N);
    }, {
        for (int i = 0; i < N; i++) {
            assert(kv_get(&tree, keys[i], &val, 0) == kv_FOUND);
        }
    });

    run_op("delete(seq)", G, {
        reset_tree();
        sort(keys, N);
    }, {
        for (int i = 0; i < N; i++) {
            assert(kv_delete(&tree, keys[i], &val, 0) == kv_DELETED);
        }
    });

    run_op("delete(rand)", G, {
        reset_tree();
        shuffle(keys, N);
    }, {
        for (int i = 0; i < N; i++) {
            assert(kv_delete(&tree, keys[i], &val, 0) == kv_DELETED);
        }
    });

    // return 0;

    run_op("reinsert(rand)", G, {
        shuffle(keys, N);
    },{
        for (int i = 0; i < N; i++) {
            kv_insert(&tree, keys[i], &val, 0);
        }
    });

    if (kv_feat_cow()) {
        struct kv *tree2 = 0;
        run_op("reinsert-cow(rand)", G, {
            kv_clear(&tree2, 0);    
            kv_clone(&tree, &tree2, 0);
            shuffle(keys, N);
        },{
            for (int i = 0; i < N; i++) {
                assert(kv_insert(&tree, keys[i], &val, 0) == kv_REPLACED);
            }
        });
        kv_clear(&tree2, 0);
    }


    if (kv_feat_cow()) {
        struct kv *tree2 = 0;
        run_op("get_mut-cow(rand)", G, {
            kv_clear(&tree2, 0);    
            kv_clone(&tree, &tree2, 0);
            shuffle(keys, N);
        },{
            for (int i = 0; i < N; i++) {
                assert(kv_get_mut(&tree, keys[i], &val, 0) == kv_FOUND);
            }
        });
        kv_clear(&tree2, 0);
    }


    if (kv_feat_counted()) {
        run_op("get_at(seq)", G, {},{
            for (int i = 0; i < N; i++) {
                assert(kv_get_at(&tree, i, &val, 0) == kv_FOUND);
            }
        });

        run_op("get_at(rand)", G, {
            shuffle(keys, N);
        }, {
            // keys are index * 10
            for (int i = 0; i < N; i++) {
                assert(kv_get_at(&tree, keys[i]/10, &val, 0) == kv_FOUND);
            }
        });

        run_op("delete_at(head)", G, {
            reset_tree();
        }, {
            for (int i = 0; i < N; i++) {
                assert(kv_delete_at(&tree, 0, &val, 0) == kv_DELETED);
            }
        });

        run_op("delete_at(mid)", G, {
            reset_tree();
        }, {
            for (int i = 0; i < N; i++) {
                assert(kv_delete_at(&tree, (N-i)/2, &val, 0) == kv_DELETED);
            }
        });

        run_op("delete_at(tail)", G, {
            reset_tree();
        }, {
            for (int i = 0; i < N; i++) {
                assert(kv_delete_at(&tree, (N-i)-1, &val, 0) == kv_DELETED);
            }
        });


        int *delidxs = malloc(N*sizeof(int));
        assert(delidxs);

        run_op("delete_at(rand)", G, {
            reset_tree();
            int i = 0;
            int count = N;
            while (count > 0) {
                int index = rand() % count;
                count--;
                delidxs[i++] = index;
            }
        }, {
            for (int i = 0; i < N; i++) {
                assert(kv_delete_at(&tree, delidxs[i], &val, 0) == kv_DELETED);
            }
        });
        free(delidxs);
    }

    run_op("push_first", G, {
        kv_clear(&tree, 0);
        sort(keys, N);
    }, {
        for (int i = N-1; i >= 0; i--) {
            assert(kv_push_front(&tree, keys[i], 0) == kv_INSERTED);
        }
    });

    run_op("push_last", G, {
        kv_clear(&tree, 0);
        sort(keys, N);
    }, {
        for (int i = 0; i < N; i++) {
            assert(kv_push_back(&tree, keys[i], 0) == kv_INSERTED);
        }
    });

    run_op("pop_first", G, {
        reset_tree();
    }, {
        for (int i = 0; i < N; i++) {
            assert(kv_pop_front(&tree, 0, 0) == kv_DELETED);
        }
    });

    run_op("pop_last", G, {
        reset_tree();
    }, {
        for (int i = 0; i < N; i++) {
            assert(kv_pop_back(&tree, 0, 0) == kv_DELETED);
        }
    });

    run_op("scan", G, {
        reset_tree();
    }, {
        double bsum = 0;
        kv_scan(&tree, iter_scan, &bsum);
        assert(asum == bsum);
    });

    run_op("scan_desc", G, {
        reset_tree();
    }, {
        double bsum = 0;
        kv_scan_desc(&tree, iter_scan, &bsum);
        assert(asum == bsum);
    });

    run_op("iter_scan", G, {
        reset_tree();
    }, {
        double bsum = 0;
        struct kv_iter iter;
        kv_iter_init(&tree, &iter, 0);
        kv_iter_scan(&iter);
        for (int i = 0; i < N; i++) {
            assert(kv_iter_valid(&iter));
            kv_iter_item(&iter, &val);
            bsum += val;
            kv_iter_next(&iter);
        }
        assert(asum == bsum);
    });

    run_op("iter_scan_desc", G, {
        reset_tree();
    }, {
        double bsum = 0;
        struct kv_iter iter;
        kv_iter_init(&tree, &iter, 0);
        kv_iter_scan_desc(&iter);
        for (int i = 0; i < N; i++) {
            assert(kv_iter_valid(&iter));
            kv_iter_item(&iter, &val);
            bsum += val;
            kv_iter_next(&iter);
        }
        assert(asum == bsum);
    });

    return 0;
}
