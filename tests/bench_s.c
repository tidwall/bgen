#include <stdio.h>
#include "testutils.h"
#include "curve.h"


#define M 32

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

struct point {
    uint32_t curve;
    int id;
    double x;
    double y;
};


static bool item_less(struct point a, struct point b) {
    return a.curve < b.curve ? true :
           a.curve > b.curve ? false :
           a.id < b.id;
}

static int item_compare(struct point a, struct point b) {
    return a.curve < b.curve ? -1 : 
           a.curve > b.curve ? 1 :
           a.id < b.id ? -1 : 
           a.id > b.id;
}

static int compare_points(const void *a, const void *b) {
    const struct point *pa = a;
    const struct point *pb = b;
    return item_less(*pa, *pb) ? -1 : 
           item_less(*pb, *pa) ? 1 : 
           0;
}

static void sort_points(struct point array[], size_t numels) {
    qsort(array, numels, sizeof(struct point), compare_points);
}

static void shuffle_points(struct point array[], size_t numels) {
    shuffle0(array, numels, sizeof(struct point));
}

static void item_rect(double min[], double max[], struct point point) {
    min[0] = point.x;
    min[1] = point.y;
    max[0] = point.x;
    max[1] = point.y;
}


#define BGEN_NAME      kv
#define BGEN_TYPE      struct point
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
#define BGEN_SPATIAL
#define BGEN_ITEMRECT       { item_rect(min, max, item); }
#define BGEN_MAYBELESSEQUAL { return a.curve <= b.curve; }
#define BGEN_COMPARE        { return item_compare(a, b); }
#include "../bgen.h"

static bool iter_scan(int item, void *udata) {
    double *sum = udata;
    (*sum) += item;
    return true;
}

#define reset_tree() { \
    kv_clear(&tree, 0); \
    shuffle_points(keys, N); \
    for (int i = 0; i < N; i++) { \
        kv_insert(&tree, keys[i], &val, 0); \
    } \
}(void)0

#define run_op(label, nops, nruns, preop, op) {\
    double gelapsed = C < 1 ? 0 : 9999; \
    double gstart = now(); \
    double gmstart = mtotal; \
    double gmtotal = 0; \
    for (int g = 0; g < (nruns); g++) { \
        printf("\r%-19s", label); \
        printf(" %d/%d ", g+1, (nruns)); \
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
    bench_print_mem(nops, gstart, gstart+gelapsed, \
        gmstart, gmstart+(gmtotal/(nruns))); \
    assert(kv_sane(&tree, 0)); \
    reset_tree(); \
}(void)0


struct iiter0ctx {
    int id;
    bool found;
};

bool iter_one(struct point point, void *udata) {
    struct iiter0ctx *ctx = udata;
    if (point.id == ctx->id) {
        ctx->found = true;
        return false;
    }
    return true;
}

bool iter_many(struct point point, void *udata) {
    int *count = udata;
    (*count)++;
    return true;
}

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
    int nkeys = N;
    struct point *keys = malloc(nkeys*sizeof(struct point));
    assert(keys);
    for (int i = 0; i < nkeys; i++) {
        struct point point;
        point.id = i;
        point.x = rand_double()*360.0-180.0;
        point.y = rand_double()*180.0-90.0;
        point.curve = curve_hilbert(point.x, point.y, 
            (double[4]){-180, -90, 180, 90});
        // point.curve = curve_z(point.y, point.x);
        keys[i] = point;
    }
    shuffle_points(keys, nkeys);
    
    struct kv *tree = 0;
    struct point val;
    int sum = 0;

    run_op("insert(seq)", N, G, {
        kv_clear(&tree, 0);
        sort_points(keys, nkeys);
    },{
        for (int i = 0; i < N; i++) {
            assert(kv_insert(&tree, keys[i], &val, 0) == kv_INSERTED);
        }
    });
    run_op("insert(rand)", N, G, {
        kv_clear(&tree, 0);
        shuffle_points(keys, nkeys);
    },{
        for (int i = 0; i < N; i++) {
            assert(kv_insert(&tree, keys[i], &val, 0) == kv_INSERTED);
        }
    });

    printf("== using callbacks ==\n");

    double coord[2];
    run_op("search-item(seq)", N, G, {
        sort_points(keys, nkeys);
    }, {
        for (int i = 0; i < nkeys; i++) {
            coord[0] = keys[i].x;
            coord[1] = keys[i].y;
            struct iiter0ctx ctx = { .id = keys[i].id };
            kv_intersects(&tree, coord, coord, iter_one, &ctx); 
            assert(ctx.found);
        }
    });
    run_op("search-item(rand)", N, G, {
        shuffle_points(keys, nkeys);
    }, {
        for (int i = 0; i < nkeys; i++) {
            coord[0] = keys[i].x;
            coord[1] = keys[i].y;
            struct iiter0ctx ctx = { .id = keys[i].id };
            kv_intersects(&tree, coord, coord, iter_one, &ctx); 
            assert(ctx.found);
        }
    });
    
    sum = 0;
    run_op("search-1%%", 1000, G, {}, {
        for (int i = 0; i < 1000; i++) {
            const double p = 0.01;
            double min[2];
            double max[2];
            min[0] = rand_double() * 360.0 - 180.0;
            min[1] = rand_double() * 180.0 - 90.0;
            max[0] = min[0] + 360.0*p;
            max[1] = min[1] + 180.0*p;
            int res = 0;
            kv_intersects(&tree, min, max, iter_many, &res);
            sum += res;
        }
    });
    // printf("%d\n", sum);

    sum = 0;
    run_op("search-5%%", 1000, G, {}, {
        for (int i = 0; i < 1000; i++) {
            const double p = 0.05;
            double min[2];
            double max[2];
            min[0] = rand_double() * 360.0 - 180.0;
            min[1] = rand_double() * 180.0 - 90.0;
            max[0] = min[0] + 360.0*p;
            max[1] = min[1] + 180.0*p;
            int res = 0;
            kv_intersects(&tree, min, max, iter_many, &res);
            sum += res;
        }
    });
    // printf("%d\n", sum);

    sum = 0;
    run_op("search-10%%", 1000, G, {}, {
        for (int i = 0; i < 1000; i++) {
            const double p = 0.10;
            double min[2];
            double max[2];
            min[0] = rand_double() * 360.0 - 180.0;
            min[1] = rand_double() * 180.0 - 90.0;
            max[0] = min[0] + 360.0*p;
            max[1] = min[1] + 180.0*p;
            int res = 0;
            kv_intersects(&tree, min, max, iter_many, &res);
            sum += res;
        }
    });
    // printf("%d\n", sum);

    

    ///////////////////////////////////////////////////////////////////////////
    printf("== using iterators ==\n");
    struct kv_iter iter;
    run_op("search-item(seq)", N, G, {
        kv_iter_init(&tree, &iter, 0);
        sort_points(keys, nkeys);
    }, {
        for (int i = 0; i < nkeys; i++) {
            coord[0] = keys[i].x;
            coord[1] = keys[i].y;
            struct iiter0ctx ctx = { .id = keys[i].id };
            kv_iter_intersects(&iter, coord, coord);
            while (kv_iter_valid(&iter)) {
                struct point point;
                kv_iter_item(&iter, &point);
                if (!iter_one(point, &ctx)) {
                    break;
                }
                kv_iter_next(&iter);
            }
            assert(ctx.found);
        }
    });
    run_op("search-item(rand)", N, G, {
        kv_iter_init(&tree, &iter, 0);
        shuffle_points(keys, nkeys);
    }, {
        for (int i = 0; i < nkeys; i++) {
            coord[0] = keys[i].x;
            coord[1] = keys[i].y;
            struct iiter0ctx ctx = { .id = keys[i].id };
            kv_iter_intersects(&iter, coord, coord);
            while (kv_iter_valid(&iter)) {
                struct point point;
                kv_iter_item(&iter, &point);
                if (!iter_one(point, &ctx)) {
                    break;
                }
                kv_iter_next(&iter);
            }
            assert(ctx.found);
        }
    });

    sum = 0;
    run_op("search-1%%", 1000, G, {
        kv_iter_init(&tree, &iter, 0);
    }, {
        for (int i = 0; i < 1000; i++) {
            const double p = 0.01;
            double min[2];
            double max[2];
            min[0] = rand_double() * 360.0 - 180.0;
            min[1] = rand_double() * 180.0 - 90.0;
            max[0] = min[0] + 360.0*p;
            max[1] = min[1] + 180.0*p;
            int res = 0;
            kv_iter_intersects(&iter, min, max);
            while (kv_iter_valid(&iter)) {
                struct point point;
                kv_iter_item(&iter, &point);
                if (!iter_many(point, &res)) {
                    break;
                }
                kv_iter_next(&iter);
            }
            sum += res;
        }
    });
    // printf("%d\n", sum);
    
    sum = 0;
    run_op("search-5%%", 1000, G, {
        kv_iter_init(&tree, &iter, 0);
    }, {
        for (int i = 0; i < 1000; i++) {
            const double p = 0.05;
            double min[2];
            double max[2];
            min[0] = rand_double() * 360.0 - 180.0;
            min[1] = rand_double() * 180.0 - 90.0;
            max[0] = min[0] + 360.0*p;
            max[1] = min[1] + 180.0*p;
            int res = 0;
            kv_iter_intersects(&iter, min, max);
            while (kv_iter_valid(&iter)) {
                struct point point;
                kv_iter_item(&iter, &point);
                if (!iter_many(point, &res)) {
                    break;
                }
                kv_iter_next(&iter);
            }
            sum += res;
        }
    });
    // printf("%d\n", sum);

    sum = 0;
    run_op("search-10%%", 1000, G, {
        kv_iter_init(&tree, &iter, 0);
    }, {
        for (int i = 0; i < 1000; i++) {
            const double p = 0.10;
            double min[2];
            double max[2];
            min[0] = rand_double() * 360.0 - 180.0;
            min[1] = rand_double() * 180.0 - 90.0;
            max[0] = min[0] + 360.0*p;
            max[1] = min[1] + 180.0*p;
            int res = 0;
            kv_iter_intersects(&iter, min, max);
            while (kv_iter_valid(&iter)) {
                struct point point;
                kv_iter_item(&iter, &point);
                if (!iter_many(point, &res)) {
                    break;
                }
                kv_iter_next(&iter);
            }
            sum += res;
        }
    });
    // printf("%d\n", sum);

    return 0;
}
