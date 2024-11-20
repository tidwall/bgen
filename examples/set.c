// cc examples/set.c && ./a.out
// Adapted from https://en.cppreference.com/w/cpp/container/set
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define BGEN_NAME set
#define BGEN_TYPE int
#define BGEN_LESS return a < b;
#include "../bgen.h"

void print_set(struct set **set) {
    struct set_iter *iter;
    set_iter_init(set, &iter, 0);
    printf("{ ");
    for (set_iter_scan(iter); set_iter_valid(iter); set_iter_next(iter)) {
        int item;
        set_iter_item(iter, &item);
        printf("%d ", item);
    }
    set_iter_release(iter);
    printf("}");
}

int main() {
    int data[] = { 1, 5, 3 };
    int n = sizeof(data)/sizeof(int);

    struct set *set = 0;
    for (int i = 0; i < n; i++) {
        set_insert(&set, data[i], 0, 0);
    }
    print_set(&set);
    printf("\n");

    set_insert(&set, 2, 0, 0);
    print_set(&set);
    printf("\n");

    int keys[] = { 3, 4 };
    for (int i = 0; i < 2; i++) {
        print_set(&set);
        if (set_contains(&set, keys[i], 0)) {
            printf(" does contain %d\n", keys[i]);
        } else {
            printf(" doesn't contain %d\n", keys[i]);
        }
    }
    printf("\n");

    return 0;
}

// Output:
// { 1 3 5 }
// { 1 2 3 5 }
// { 1 2 3 5 } does contain 3
// { 1 2 3 5 } doesn't contain 4
