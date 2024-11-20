// cc examples/map.c && ./a.out
// Adapted from https://en.cppreference.com/w/cpp/container/map
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

struct pair {
    const char *key;
    int value;
};

#define BGEN_NAME map
#define BGEN_TYPE struct pair
#define BGEN_COMPARE return strcmp(a.key, b.key);
#include "../bgen.h"

void print_map(const char *comment, struct map **map) {
    printf("%s", comment);
    struct map_iter *iter;
    map_iter_init(map, &iter, 0);
    for (map_iter_scan(iter); map_iter_valid(iter); map_iter_next(iter)) {
        struct pair pair;
        map_iter_item(iter, &pair);
        printf("[%s] = %d; ", pair.key, pair.value);
    }
    map_iter_release(iter);
    printf("\n");
}

int main() {
    // Create a map of three (string, int) pairs
    struct map *map = 0;
    map_insert(&map, (struct pair){"GPU", 15}, 0, 0);
    map_insert(&map, (struct pair){"RAM", 20}, 0, 0);
    map_insert(&map, (struct pair){"CPU", 10}, 0, 0);
    print_map("1) Initial map:  ", &map);

    // Get an existing item
    struct pair item;
    assert(map_get(&map, (struct pair){"GPU"}, &item, 0) == map_FOUND);
    printf("2) Get item:     [%s] = %d;\n", item.key, item.value);

    // Update an existing item
    assert(map_insert(&map, (struct pair){"CPU", 25}, 0, 0) == map_REPLACED);
    // Insert a new item
    assert(map_insert(&map, (struct pair){"SSD", 30}, 0, 0) == map_INSERTED); 
    print_map("3) Updated map:  ", &map);
    assert(map_insert(&map, (struct pair){"UPS"}, 0, 0) == map_INSERTED); 
    print_map("4) Updated map:  ", &map);

    assert(map_delete(&map, (struct pair){.key="GPU"}, 0, 0) == map_DELETED);
    print_map("5) After delete: ", &map);

    return 0;
}

// Output:
// 1) Initial map:  [CPU] = 10; [GPU] = 15; [RAM] = 20;
// 2) Get item:     [GPU] = 15;
// 3) Updated map:  [CPU] = 25; [GPU] = 15; [RAM] = 20; [SSD] = 30;
// 4) Updated map:  [CPU] = 25; [GPU] = 15; [RAM] = 20; [SSD] = 30; [UPS] = 0;
// 5) After delete: [CPU] = 25; [RAM] = 20; [SSD] = 30; [UPS] = 0;
