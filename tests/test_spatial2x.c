#define TESTNAME "spatial2x"
#define NOCOV
#include "testutils.h"
// #include "cities.h"
#include "curve.h"

struct point {
    int id;
    uint32_t curve;
    double x;
    double y;
};

void point_rect(struct point point, double min[], double max[]) {
    min[0] = point.x;
    max[0] = point.y;
    min[1] = point.x;
    max[1] = point.y;
}

int point_compare(struct point a, struct point b) {
    return a.curve < b.curve ? -1 : a.curve > b.curve ? 1 :
           a.id < b.id ? -1 : a.id > b.id;
}

static int compare_points(const void *a, const void *b) {
    return point_compare(*(struct point*)a, *(struct point*)b); 
}

static void sort_points(struct point *array, size_t numels) {
    qsort(array, numels, sizeof(struct point), compare_points);
}

static void shuffle_points(struct point *array, size_t numels) {
    shuffle0(array, numels, sizeof(struct point));
}

#define BGEN_NAME       kv
#define BGEN_TYPE       struct point
#define BGEN_FANOUT     4
#define BGEN_COUNTED
#define BGEN_SPATIAL
#define BGEN_ITEMRECT   { point_rect(item, min, max); }
#define BGEN_COMPARE    { return point_compare(a, b); }
#include "../bgen.h"


void pitem(struct point item, FILE *file, void *udata) {
    (void)udata;
    fprintf(file, "(%d %f %f)", item.id, item.x, item.y);
}

void prtype(double rtype, FILE *file, void *udata) {
    (void)udata;
    fprintf(file, "%.0f", rtype);
}

void tree_print(struct kv **root) {
    _kv_internal_print(root, stdout, pitem, prtype, 0);
}


bool intersects(double amin[], double amax[], double bmin[], double bmax[]) {
    int bits = 0;
    for (int i = 0; i < 2; i++) {
        bits |= bmin[i] > amax[i];
        bits |= bmax[i] < amin[i];
    }
    return bits == 0;
}

struct iiter_ctx {
    double *min;
    double *max;
    struct point *results;
    int count;
};

bool iiter(struct point point, void *udata) {
    struct iiter_ctx *ctx = udata;
    // printf("%d: id=%d point=( %f %f )\n", ctx->count, point.id, point.x, point.y);
    ctx->results[ctx->count++] = point;
    return true;
}

bool siter(struct point point, void *udata) {
    struct iiter_ctx *ctx = udata;
    double min[2], max[2];
    point_rect(point, min, max);
    if (intersects(min, max, ctx->min, ctx->max)) {
        if (!iiter(point, udata)) {
            return false;
        }
    }
    return true;
}

bool iiter_ctx_equal(struct iiter_ctx a, struct iiter_ctx b) {
    if (a.count != b.count) {
        return false;
    }
    for (int i = 0; i < a.count; i++) {
        if (point_compare(a.results[i], b.results[i]) != 0) {
            // printf("%d %d\n", a.results[i].id, b.results[i].id);
            return false;
        }
    }
    return true;
}

void test_intersects(void) {
    testinit();
    struct kv *tree = 0;
    double start = now();
    // int run = 0;
    while (now() - start < 1.0) {
        int npoints;
        switch (rand()%10) {
        case 0:
            npoints = rand_double()*10;
            break;
        case 1:
            npoints = rand_double()*1000;
            break;
        default:
            npoints = rand_double()*500;
        }

        // printf("\033[1mRUN %d\033[0m\n", run);
        struct point *points = malloc(sizeof(struct point)*npoints);
        assert(points);
        double window[4] = { -180.0, -90.0, 180.0, 90.0 };
        for (int i = 0; i < npoints; i++) {
            points[i].id = i;
            points[i].x = rand_double() * 360.0 - 180.0;
            points[i].y = rand_double() * 180.0 - 90.0;
            points[i].curve = curve_hilbert(points[i].x, points[i].y, window);
        }
        shuffle_points(points, npoints);
        struct point val;
        for (int i = 0; i < npoints; i++) {
            val.id = -1;
            assert(kv_insert(&tree, points[i], &val, 0) == kv_INSERTED);
        }
        struct point *results1 = malloc(sizeof(struct point)*npoints);
        assert(results1);
        struct point *results2 = malloc(sizeof(struct point)*npoints);
        assert(results2);
        struct point *results3 = malloc(sizeof(struct point)*npoints);
        assert(results3);
        for (int i = 0; i < 100; i++) {
            // printf("\033[1;33m== %d ==\033[0m\n", i);
            double min[] = {
                rand_double() * 360.0 - 180.0,
                rand_double() * 180.0 - 90.0
            };
            double max[] = {
                min[0] + rand_double() * 10.0,
                min[1] + rand_double() * 10.0
            };
            // printf("\033[1;33m( %f %f %f %f )\033[0m\n", min[0], min[1], max[0], max[1]);
            // printf("\033[1;34m>>> scan\033[0m\n");
            struct iiter_ctx ctx1 = { .min = min, .max = max, .results = results1 };
            kv_scan(&tree, siter, &ctx1);
            // printf("\033[1;34m>>> intersects\033[0m\n");
            struct iiter_ctx ctx2 = { .min = min, .max = max, .results = results2 };
            kv_intersects(&tree, min, max, iiter, &ctx2);
            // printf("\033[1;34m>>> iter_intersects\033[0m\n");
            struct iiter_ctx ctx3 = { .min = min, .max = max, .results = results3 };
            struct kv_iter *iter;
            kv_iter_init(&tree, &iter, 0);
            kv_iter_intersects(iter, min, max);
            for (; kv_iter_valid(iter); kv_iter_next(iter)) {
                struct point point;
                kv_iter_item(iter, &point);
                siter(point, &ctx3);
            }
            kv_iter_release(iter);
            assert(iiter_ctx_equal(ctx1, ctx2));
            if (!iiter_ctx_equal(ctx2, ctx3)) {
                tree_print(&tree);
            }
            assert(iiter_ctx_equal(ctx2, ctx3));
        }
        free(results1);
        free(results2);
        free(results3);
        free(points);
        kv_clear(&tree, 0);
        // run++;
    }
    checkmem();
}

// void city_fillrect(struct city_entry city, double min[], double max[]) {
//     min[0] = city.lon;
//     min[1] = city.lat;
//     max[0] = city.lon;
//     max[1] = city.lat;
// }

// #define BGEN_NAME       cities
// #define BGEN_TYPE       struct city_entry
// #define BGEN_FANOUT     16
// #define BGEN_SPATIAL
// #define BGEN_ITEMRECT   city_fillrect(item, min, max);
// #define BGEN_LESS       return a.id < b.id;
// #include "../bgen.h"


// void print_rects(struct cities *node, int depth) {
//     if (node->isleaf) {
//         return;
//     }
//     for (int i = 0; i <= node->len; i++) {
//         double xmin = node->rects[i].min[0];
//         double ymin = node->rects[i].min[1];
//         double xmax = node->rects[i].max[0];
//         double ymax = node->rects[i].max[1];
//         for (int j = 0; j < depth; j++) {
//             printf("  ");
//         }
//         printf("(%f %f %f %f)\n", xmin, ymin, xmax, ymax);
//         print_rects(node->children[i], depth+1);
//     }
// }

// void test_svg(void) {
//     testinit();
//     struct cities *cities = 0;
//     for (int i = 0; i < NCITIES; i++) {
//         assert(cities_insert(&cities, all_cities[i], 0, 0) == cities_INSERTED);
//     }
    
//     print_rects(cities, 0);

//     // all_cities
//     checkmem();

// }

int main(void) {
    initrand();
    test_intersects();
    // test_svg();
    return 0;
}
