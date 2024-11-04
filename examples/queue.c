// cc examples/queue.c && ./a.out
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define BGEN_NAME queue
#define BGEN_TYPE int
#define BGEN_NOORDER
#include "../bgen.h"

int main() {
    struct queue *queue = 0;
 
    queue_push_back(&queue, 0, 0); // pushes 0
    queue_push_back(&queue, 2, 0); // q = 0 2
    queue_push_back(&queue, 1, 0); // q = 0 2 1
    queue_push_back(&queue, 3, 0); // q = 0 2 1 3

    int val;

    queue_front(&queue, &val, 0);
    assert(val == 0);
    queue_back(&queue, &val, 0);
    assert(val == 3);
    assert(queue_count(&queue, 0) == 4);
 
    // Remove the first element, 0
    queue_pop_front(&queue, &val, 0); 
    assert(val == 0);
    assert(queue_count(&queue, 0) == 3);
 
    // Print and remove all elements.
    printf("queue: ");
    for (; queue_count(&queue, 0) > 0; queue_pop_front(&queue, 0, 0)) {
            queue_front(&queue, &val, 0);
            printf("%d ", val);
    }
    printf("\n");
    
    assert(queue_count(&queue, 0) == 0);
    return 0;
}

// Output:
// queue: 2 1 3
