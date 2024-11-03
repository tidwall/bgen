// cc examples/deque.c && ./a.out
// Adapted from https://en.cppreference.com/w/cpp/container/deque
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define BGEN_NAME deque
#define BGEN_TYPE int
#define BGEN_NOORDER
#include "../bgen.h"

int main() {
    
    int data[] = { 7, 5, 16, 8 };
    int n = sizeof(data)/sizeof(int);

    // Create a deque containing integers
    struct deque *deque = 0;
    for (int i = 0; i < n; i++) {
        deque_push_back(&deque, data[i], 0);
    }
    
    // Add an integer to the beginning and end of the deque
    deque_push_front(&deque, 13, 0);
    deque_push_back(&deque, 25, 0);
 
    // Iterate and print values of deque
    struct deque_iter iter;
    deque_iter_init(&deque, &iter, 0);
    deque_iter_scan(&iter);
    for (; deque_iter_valid(&iter); deque_iter_next(&iter)) {
        int item;
        deque_iter_item(&iter, &item);
        printf("%d ", item);
    }
    printf("\n");

    return 0;
}

// Output:
// 13 7 5 16 8 25
