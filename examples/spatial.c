// cc examples/spatial.c && ./a.out
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

// Support functions found in the tests directory
#include "../tests/cities.h"
#include "../tests/curve.h"
#include "../tests/dist.h"

struct city {
    uint32_t curve;
    const char *city;
    double lat;
    double lon;
};

int city_compare(struct city a, struct city b) {
    return a.curve < b.curve ? -1 : a.curve > b.curve ? 1 : 
           strcmp(a.city, b.city);
}

void city_rect(struct city city, double min[], double max[]) {
    min[0] = city.lon, min[1] = city.lat;
    max[0] = city.lon, max[1] = city.lat;
} 

#define BGEN_NAME cities
#define BGEN_TYPE struct city
#define BGEN_SPATIAL
#define BGEN_ITEMRECT city_rect(item, min, max);
#define BGEN_COMPARE return city_compare(a, b);
#include "../bgen.h"

struct point {
    double lat;
    double lon;
};

double calcdist(double min[], double max[], void *target, void *udata) {
    struct point *point = target;
    return point_rect_dist(point->lat, point->lon, min[1], min[0], max[1], 
        max[0]);
}

int main() {
    // Load a bunch of city entries into a spatial B-tree.
    // Use a hilbert curve for spatial ordering.
    struct cities *cities = 0;
    double window[] = { -180, -90, 180, 90 };
    for (int i = 0; i < NCITIES; i++) {
        struct city city = {
            .curve = curve_hilbert(all_cities[i].lon, all_cities[i].lat, window),
            .city = all_cities[i].city,
            .lat = all_cities[i].lat,
            .lon = all_cities[i].lon,
        };
        cities_insert(&cities, city, 0, 0);
    }

    assert(cities_count(&cities, 0) == NCITIES);
    printf("Inserted %zu cities\n", cities_count(&cities, 0));

    // Find all cities in rectangle
    double min[] = { -113, 33 };
    double max[] = { -111, 34 };
    printf("Cities inside rectangle ((%.0f %0.f) (%.0f %0.f)):\n", 
        min[0], min[1], max[0], max[1]);
    struct cities_iter *iter;
    cities_iter_init(&cities, &iter, 0);
    cities_iter_intersects(iter, min, max);
    while (cities_iter_valid(iter)) {
        struct city city;
        cities_iter_item(iter, &city);
        printf("- %s\n", city.city);
        cities_iter_next(iter);
    }
    cities_iter_release(iter);
    printf("\n");

    // Find nearest 10 cities to (-113, 33)
    // This uses a kNN operation
    struct point point = { .lon = -113, .lat = 33 };
    printf("Top 10 cities nearby point (%.0f %0.f):\n", point.lon, point.lat);
    cities_iter_init(&cities, &iter, 0);
    cities_iter_nearby(iter, &point, calcdist);
    int n = 0;
    while (n < 10 && cities_iter_valid(iter)) {
        struct city city;
        cities_iter_item(iter, &city);
        printf("- %s\n", city.city);
        cities_iter_next(iter);
        n++;
    }
    cities_iter_release(iter);
    printf("\n");

    return 0;
}

// Output:
// Cities inside rectangle ((-113 33) (-111 34)):
// - Chandler
// - Scottsdale
// - Mesa
// - Phoenix
// - Glendale
// 
// Top 10 cities nearby point (-113 33):
// - Glendale
// - Phoenix
// - Chandler
// - Scottsdale
// - Mesa
// - San Luis Rio Colorado
// - Tucson
// - Mexicali
// - Heroica Nogales
// - Ensenada