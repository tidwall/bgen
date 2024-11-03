// cc examples/vector.c && ./a.out
// Adapted from https://en.cppreference.com/w/cpp/container/vector
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define BGEN_NAME vector
#define BGEN_TYPE int
#define BGEN_COUNTED
#define BGEN_NOORDER
#include "../bgen.h"

int main() {
    int data[] = { 8, 4, 5, 9 };
    int n = sizeof(data)/sizeof(int);

    // Create a vector containing integers
    struct vector *vector = 0;
    for (int i = 0; i < n; i++) {
        vector_push_back(&vector, data[i], 0);
    }

    // Add two more integers to vector
    vector_push_back(&vector, 6, 0);
    vector_push_back(&vector, 9, 0);
 
    // Overwrite element at position 2
    vector_replace_at(&vector, 2, -1, 0, 0);

    // Insert an item in the middle of the vector
    vector_insert_at(&vector, 2, 7, 0);
 
    // Delete an item in the middle of the vector
    vector_delete_at(&vector, 1, 0, 0);

    // Print out the vector
    for (int i = 0; i < vector_count(&vector, 0); i++) {
        int item;
        vector_get_at(&vector, i, &item, 0);
        printf("%d ", item);
    }
    printf("\n");
    return 0;
}

// Output:
// 8 7 -1 9 6 9
