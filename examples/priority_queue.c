// cc examples/priority_queue.c && ./a.out
// Adapted from https://en.cppreference.com/w/cpp/container/priority_queue

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define BGEN_NAME max_priority_queue
#define BGEN_TYPE int
#define BGEN_LESS return a < b;
#include "../bgen.h"

#define BGEN_NAME min_priority_queue
#define BGEN_TYPE int
#define BGEN_LESS return b < a;
#include "../bgen.h"

int main() {
    int data[] = { 1, 8, 5, 6, 3, 4, 0, 9, 7, 2 };
    int n = sizeof(data)/sizeof(int);
    printf("data: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", data[i]);
    }
    printf("\n");

    struct max_priority_queue *max_priority_queue = 0;

    // Fill the priority queue.
    for (int i = 0; i < n; i++) {
        max_priority_queue_insert(&max_priority_queue, data[i], 0, 0);
    }

    printf("max_priority_queue: ");
    while (max_priority_queue_count(&max_priority_queue, 0) > 0) {
        int val;
        max_priority_queue_pop_front(&max_priority_queue, &val, 0);
        printf("%d ", val);
    }
    printf("\n");

    struct min_priority_queue *min_priority_queue = 0;

    // Fill the priority queue.
    for (int i = 0; i < n; i++) {
        min_priority_queue_insert(&min_priority_queue, data[i], 0, 0);
    }

    printf("min_priority_queue: ");
    while (min_priority_queue_count(&min_priority_queue, 0) > 0) {
        int val;
        min_priority_queue_pop_front(&min_priority_queue, &val, 0);
        printf("%d ", val);
    }
    printf("\n");


    return 0;
}

// Output:
// data: 1 8 5 6 3 4 0 9 7 2
// max_priority_queue: 0 1 2 3 4 5 6 7 8 9
// min_priority_queue: 9 8 7 6 5 4 3 2 1 0
