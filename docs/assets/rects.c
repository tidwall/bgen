#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../../tests/cities.h"
#include "../../tests/curve.h"

// #define ZORDER

const char *order = "none";

void city_fillrect(struct city_entry city, double min[], double max[]) {
    min[0] = city.lon;
    min[1] = city.lat;
    max[0] = city.lon;
    max[1] = city.lat;
}

int city_compare(struct city_entry a, struct city_entry b) {
    double window[] = { -180, -90, 180, 90 };
    uint32_t ac = 0;
    uint32_t bc = 0;
    if (strcmp(order, "hilbert") == 0) {
        ac = curve_hilbert(a.lon, a.lat, window);
        bc = curve_hilbert(b.lon, b.lat, window);
    } else if (strcmp(order, "zorder") == 0) {
        ac = curve_z(a.lon, a.lat, window);
        bc = curve_z(b.lon, b.lat, window);
    }
    return ac < bc ? -1 : ac > bc ? 1 : 
           a.id < b.id ? -1 : a.id > b.id;
}

#define BGEN_NAME       cities
#define BGEN_TYPE       struct city_entry
#define BGEN_FANOUT     16
#define BGEN_SPATIAL
#define BGEN_ITEMRECT   city_fillrect(item, min, max);
#define BGEN_COMPARE    return city_compare(a, b);
#include "../../bgen.h"

void print_rects(struct cities *node, int depth) {
    if (node->isleaf) {
        for (int i = 0; i < node->len; i++) {
            printf("  {\"depth\":%d,\"rect\":[%f,%f,%f,%f]},\n", depth, 
                node->items[i].lon, node->items[i].lat, node->items[i].lon, 
                node->items[i].lat);
        }
    } else {
        for (int i = 0; i <= node->len; i++) {
            double xmin = node->rects[i].min[0];
            double ymin = node->rects[i].min[1];
            double xmax = node->rects[i].max[0];
            double ymax = node->rects[i].max[1];
            printf("  {\"depth\":%d,\"rect\":[%f,%f,%f,%f]},\n", depth, xmin, ymin, 
                xmax, ymax);
            print_rects(node->children[i], depth+1);
            if (i < node->len) {
                printf("  {\"depth\":%d,\"rect\":[%f,%f,%f,%f]},\n", depth, 
                    node->items[i].lon, node->items[i].lat, node->items[i].lon, 
                    node->items[i].lat);
            }
        }
    }
}

int main(int nargs, char *args[]) {
    if (nargs > 1) {
        order = args[1];
    }
    struct cities *cities = 0;
    for (int i = 0; i < NCITIES; i++) {
        assert(cities_insert(&cities, all_cities[i], 0, 0) == cities_INSERTED);
    }
    printf("var _rects = [\n");
    print_rects(cities, 0);
    printf("]\n");

    return 0;
}
