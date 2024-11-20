#include <stdio.h>

#define BGEN_NAME bt            // The namespace for the btree structure.
#define BGEN_TYPE int           // The data type for all items in the btree
#define BGEN_LESS return a < b; // A code fragment for comparing items
#include "../bgen.h"            // Include "bgen.h" to generate the btree

int main() {
    // Create an empty btree instance.
    struct bt *tree = 0;

    // Insert some items into the btree
    bt_insert(&tree, 3, 0, 0);
    bt_insert(&tree, 8, 0, 0);
    bt_insert(&tree, 2, 0, 0);
    bt_insert(&tree, 5, 0, 0);

    // Print items in tree
    struct bt_iter *iter;
    bt_iter_init(&tree, &iter, 0);
    for (bt_iter_scan(iter); bt_iter_valid(iter); bt_iter_next(iter)) {
        int item;
        bt_iter_item(iter, &item);
        printf("%d ", item);
    } 
    printf("\n");

    // Delete an item
    bt_delete(&tree, 3, 0, 0);

    // Print again
    for (bt_iter_scan(iter); bt_iter_valid(iter); bt_iter_next(iter)) {
        int item;
        bt_iter_item(iter, &item);
        printf("%d ", item);
    } 
    printf("\n");

    bt_iter_release(iter);

    bt_clear(&tree, 0);
    return 0;
}
