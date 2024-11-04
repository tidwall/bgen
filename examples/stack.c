// cc examples/stack.c && ./a.out
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define BGEN_NAME stack
#define BGEN_TYPE int
#define BGEN_NOORDER
#include "../bgen.h"

int main() {
    struct stack *stack = 0;
 
    stack_push_back(&stack, 0, 0); // pushes 0
    stack_push_back(&stack, 2, 0); // q = 0 2
    stack_push_back(&stack, 1, 0); // q = 0 2 1
    stack_push_back(&stack, 3, 0); // q = 0 2 1 3

    int val;

    stack_front(&stack, &val, 0);
    assert(val == 0);
    stack_back(&stack, &val, 0);
    assert(val == 3);
    assert(stack_count(&stack, 0) == 4);

    // Remove the back element, 3
    stack_pop_back(&stack, &val, 0); 
    assert(val == 3);
    assert(stack_count(&stack, 0) == 3);

    // Print and remove all elements.
    printf("stack: ");
    for (; stack_count(&stack, 0) > 0; stack_pop_back(&stack, 0, 0)) {
            stack_back(&stack, &val, 0);
            printf("%d ", val);
    }
    printf("\n");

    assert(stack_count(&stack, 0) == 0);

    return 0;
}

// Output:
// stack: 1 2 0
