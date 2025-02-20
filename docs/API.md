## API

C API for the [Bgen B-tree generator](https://github.com/tidwall/bgen).

This document provides a description of the functions and types in the bgen.h source file.

It's recommended to first read the more general overview in the project [README](https://github.com/tidwall/bgen).

Below is a complete list of all function generated by bgen. 
The "bt" namespace and "bitem" item type are used to as a placeholder.
These are both configurable by the developer using BGEN_NAME and BGEN_TYPE.

```c
#define BGEN_NAME bt
#define BGEN_TYPE bitem
```

Every btree is provided the following functions.

### Basic operations

```c
/// Get an item
/// Returns bt_FOUND or bt_NOTFOUND
/// Returns bt_UNSUPPORTED when BGEN_NOORDER
int bt_get(struct bt **root, bitem key, bitem *item_out, void *udata);

/// Insert or replace an item
/// Returns bt_INSERTED, bt_REPLACED
/// Returns bt_UNSUPPORTED when BGEN_NOORDER
/// Returns bt_NOMEM when out of memory
int bt_insert(struct bt **root, bitem item, bitem *item_out, void *udata);

/// Delete an item
/// Returns bt_DELETED, bt_NOTFOUND
/// Returns bt_UNSUPPORTED when BGEN_NOORDER
/// Returns bt_NOMEM when out of memory
int bt_delete(struct bt **root, bitem key, bitem *item_out, void *udata);

/// Returns true if the item exists
bool bt_contains(struct bt **root, bitem key, void *udata);

/// Remove all items and free all btree resources.
int bt_clear(struct bt **root, void *udata);
```

### Queues &amp; stack

```c
/// Get the first (minumum) item in the btree
/// Returns bt_FOUND or bt_NOTFOUND
int bt_front(struct bt **root, bitem *item_out, void *udata);

/// Get the last (maxiumum) item in the btree
/// Returns bt_FOUND or bt_NOTFOUND
int bt_back(struct bt **root, bitem *item_out, void *udata);

/// Delete the first (minimum) item from the btree
/// Returns bt_DELETED, bt_NOTFOUND
/// Returns bt_NOMEM when out of memory
int bt_pop_front(struct bt **root, bitem *item_out, void *udata);

/// Delete the last (maximum) item from the btree
/// Returns bt_DELETED, bt_NOTFOUND
/// Returns bt_NOMEM when out of memory
int bt_pop_back(struct bt **root, bitem *item_out, void *udata);

/// Insert as the first (minimum) item of the btree
/// Returns bt_INSERTED
/// Returns bt_OUTOFORDER when item is not the minimum
/// Returns bt_NOMEM when out of memory
int bt_push_front(struct bt **root, bitem item, void *udata);

/// Insert as the last (maximum) item of the btree
///
/// This operation is optimized for bulk-loading.
///
/// Returns bt_INSERTED
/// Returns bt_OUTOFORDER when item is not the maximum
/// Returns bt_NOMEM when out of memory
int bt_push_back(struct bt **root, bitem item, void *udata);
```

### Counted B-tree operations

The following operations are available when BGEN_COUNTED is provided to the 
generator. See [Counted B-tree](#counted-b-tree) for more information. Also,
when BGEN_NOORDER is provided, the btree effectively becomes a 
[Vector B-tree](#vector-b-tree).

```c
/// Insert an item at index
///
/// Unless BGEN_NOORDER is begin used, it's an error to attempt to insert an
/// item that is out of order at the specified index. 
///
/// Returns bt_INSERTED
/// Returns bt_OUTOFORDER when item is out of order for the index
/// Returns bt_NOTFOUND when index is > btree count
/// Returns bt_NOMEM when out of memory
int bt_insert_at(struct bt **root, size_t index, int item, void *udata);

/// Replace an item at index
///
/// Unless BGEN_NOORDER is begin used, it's an error to attempt to replace an
/// item with another that is out of order at the specified index. 
///
/// Returns bt_REPLACED
/// Returns bt_OUTOFORDER when item is out of order for the index
/// Returns bt_NOTFOUND when index is >= btree count
/// Returns bt_NOMEM when out of memory
int bt_replace_at(struct bt **root, size_t index, int item, int *item_out, void *udata);

/// Delete an item at index
/// Returns bt_DELETED
/// Returns bt_NOTFOUND when index is >= btree count
/// Returns bt_NOMEM when out of memory
int bt_delete_at(struct bt **root, size_t index, int *item_out, void *udata);

/// Get item at index
/// Returns bt_FOUND or bt_NOTFOUND
int bt_get_at(struct bt **root, size_t index, int *item_out, void *udata);

/// Get the index for a key
/// Returns bt_FOUND or bt_NOTFOUND
/// Returns bt_UNSUPPORTED when BGEN_NOORDER
int bt_index_of(struct bt **root, int key, size_t *index, void *udata);

/// Returns the number of items in btree
size_t bt_count(struct bt **root, void *udata);

/// Seek to an position in the btree and iterate over each subsequent item.
///
/// Each item is returned in the "iter" callback.
/// Returning "false" from "iter" will stop the iteration.
///
/// Returns bt_STOPPED or bt_FINISHED
int bt_seek_at(struct bt **root, size_t index, 
    bool(*iter)(bitem item, void *udata), void *udata);

/// Seek to an position in the btree and iterate over each subsequent item, but
/// in reverse order.
///
/// Each item is returned in the "iter" callback.
/// Returning "false" from "iter" will stop the iteration.
///
/// Returns bt_STOPPED or bt_FINISHED
int bt_seek_at_desc(struct bt **root, size_t index, 
    bool(*iter)(bitem item, void *udata), void *udata);
```

### Spatial B-tree operations

The following operations are available when BGEN_SPATIAL is provided to the 
generator. See [Spatial B-tree](#spatial-b-tree) for more information.

```c
/// Search the btree for items that intersect the provided rectangle and
/// iterator over each item
///
/// Each intersecting item is returned in the "iter" callback. 
/// Returning "false" from "iter" will stop the iteration.
///
/// Returns bt_STOPPED or bt_FINISHED
int bt_intersects(struct bt **root, double min[], double max[], 
    bool(*iter)(bitem item, void *udata), void *udata);

/// Performs a kNN operation on the btree
///
/// It's expected that the caller provides their own the `dist` function, 
/// which is used to calculate a distance to rectangles and data. 
/// The "iter" callback will return all items from the minimum distance to 
/// maximum distance.
///
/// Each item is returned to the "iter" callback.
/// Returning "false" from "iter" will stop the iteration.
///
/// Returns bt_STOPPED, bt_FINISHED
/// Returns bt_NOMEM when out of memory
///
/// There's an example showing how to use this with geospatial data included 
/// with the project repository.
/// See https://github.com/tidwall/bgen/main/examples
int bt_nearby(struct bt **root, void *target, 
    double(*dist)(double min[], double max[], void *target, void *udata), 
    bool(*iter)(bitem item, void *udata), void *udata);

/// Get the minimum bounding rectangle of the btree
///
/// This fills the "min" and "max" params. It's important that min/max have
/// enough room to store the coordinates for all dimensions.
void bt_rect(struct bt **root, double min[], double max[], void *udata);
```

### Copying and cloning

```c
/// Copy a btree
/// This creates duplicate of the btree (deep copy).
/// Returns bt_COPIED
/// Returns bt_NOMEM when out of memory
int bt_copy(struct bt **root, struct bt **newroot, void *udata);

/// Copy a btree using copy-on-write
/// This operation creates an instant snapshot of the btree and requires
/// the BGEN_COW option.
/// Returns bt_COPIED
/// Returns bt_NOMEM when out of memory
int bt_clone(struct bt **root, struct bt **newroot, void *udata);
```

### Callback iteration

```c
/// Iterate over every item in the btree.
///
/// Each item is returned in the "iter" callback.
/// Returning "false" from "iter" will stop the iteration.
///
/// Returns bt_STOPPED or bt_FINISHED
int bt_scan(struct bt **root, bool(*iter)(bitem item, void *udata), void *udata);

/// Iterate over every item in the btree, but in reverse order
///
/// Each item is returned in the "iter" callback.
/// Returning "false" from "iter" will stop the iteration.
///
/// Returns bt_STOPPED or bt_FINISHED
int bt_scan_desc(struct bt **root, bool(*iter)(bitem item, void *udata), void *udata);

/// Seek to a key in the btree and iterate over each subsequent item.
///
/// Each item is returned in the "iter" callback.
/// Returning "false" from "iter" will stop the iteration.
///
/// Returns bt_STOPPED or bt_FINISHED
int bt_seek(struct bt **root, bitem key, bool(*iter)(bitem item, void *udata), void *udata);

/// Seek to a key in the btree and iterate over each subsequent item, but in 
/// reverse order.
///
/// Each item is returned in the "iter" callback.
/// Returning "false" from "iter" will stop the iteration.
///
/// Returns bt_STOPPED or bt_FINISHED
int bt_seek_desc(struct bt **root, bitem key, bool(*iter)(bitem item, void *udata), void *udata);

/// Counted B-tree iterators. See their descriptions above.
int bt_seek_at(struct bt **root, size_t index, 
    bool(*iter)(bitem item, void *udata), void *udata);
int bt_seek_at_desc(struct bt **root, size_t index, 
    bool(*iter)(bitem item, void *udata), void *udata);

/// Spatial B-tree iterators. See their descriptions above.
int bt_intersects(struct bt **root, double min[], double max[], 
    bool(*iter)(bitem item, void *udata), void *udata);
int bt_nearby(struct bt **root, void *target, 
    double(*dist)(double min[], double max[], void *target, void *udata), 
    bool(*iter)(bitem item, void *udata), void *udata);
```

### Loop iteration

```c
/// Initialize an iterator
/// Make sure to call bt_iter_release() when done iterating.
void bt_iter_init(struct bt **root, struct bt_iter **iter, void *udata);

/// Release the iterator when it's no longer needed
void bt_iter_release(struct bt_iter *iter);

/// Returns an error status code of the iterator, or zero if no error.
int bt_iter_status(struct bt_iter *iter);

/// Returns true if the iterator is valid and an item, using bt_iter_item() is 
/// available.
bool bt_iter_valid(struct bt_iter *iter);

/// Get the current iterator item. 
/// REQUIRED: iter_valid() and item != NULL
void bt_iter_item(struct bt_iter *iter, bitem *item);

/// Move to the next item
/// REQUIRED: iter_valid()
void bt_iter_next(struct bt_iter *iter);

/// Seek to a key in the btree and iterate over each subsequent item.
void bt_iter_seek(struct bt_iter *iter, bitem key);

/// Seek to a key in the btree iterates over each subsequent item in reverse
/// order.
void bt_iter_seek_desc(struct bt_iter *iter, bitem key);

/// Iterates over every item in the btree.
void bt_iter_scan(struct bt_iter *iter);

/// Iterates over every item in the btree in reverse order.
void bt_iter_scan_desc(struct bt_iter *iter);

/// Search the btree for items that intersect the provided rectangle and
/// iterator over each item.
void bt_iter_intersects(struct bt_iter *iter, double min[], double max[]);

/// Performs a kNN operation on the btree
///
/// It's expected that the caller provides their own the `dist` function, 
/// which is used to calculate a distance to rectangles and data. 
///
/// This operation will allocate memory, so make sure to check the iter_status()
/// when done. And, *always* use iter_release().
///
/// There's an example showing how to use this with geospatial data included 
/// with the project repository.
/// See https://github.com/tidwall/bgen/main/examples
void bt_iter_nearby(struct bt_iter *iter, void *target, 
    double(*dist)(double min[], double max[double], void *target, void *udata));

/// Seek to an position in the btree and iterate over each subsequent item.
void bt_iter_seek_at(struct bt_iter *iter, size_t index);

/// Seek to an position in the btree and iterate over each subsequent item, but
/// in reverse order.
void bt_iter_seek_at_desc(struct bt_iter *iter, size_t index);
```

See the iteration example in the [examples](examples) directory for usage.

### Utilties

```c
/// Compares two item
/// Returns -1  - "a" is less than "b"
/// Returns  0  - "a" and "b" are equal
/// Returns +1  - "a" is greater than "b"
int bt_compare(bitem a, bitem b, void *udata);

/// Returns true if "a" is less than "b"
bool bt_less(bitem a, bitem b, void *udata);

/// Returns the height of the btree
size_t bt_height(struct bt **root, void *udata);

/// Returns true if the btree is "sane"
/// This operation should always return true.
bool bt_sane(struct bt **root, void *udata);
```

### General info

```c
/// Returns the maximum number of items in a node.
int bt_feat_maxitems();

/// Returns the minimum number of items in a node.
int bt_feat_minitems();

/// Returns the maximum height of btree.
int bt_feat_maxheight(); 

/// Returns the max number of children.
int bt_feat_fanout();

/// Returns true if the btree is a Counted B-tree.
bool bt_feat_counted();

/// Returns true if the btree is a Spatial B-tree.
bool bt_feat_spatial();

/// Returns the number of dimensions for Spatial B-tree.
int bt_feat_dims();

/// Returns true if the btree is ordered.
bool bt_feat_ordered();

/// Returns true if copy-on-write is enabled.
bool bt_feat_cow();

/// Returns true if atomic reference counters are enabled.
bool bt_feat_atomics();

/// Returns true if binary-searching is enabled.
bool bt_feat_bsearch();

/// Returns true if path hints are enabled.
bool bt_feat_pathhint();
```

### Mutable read operations

The following operations are available when BGEN_COW is used.
See [Copy-on-write](#copy-on-write) for more information.

```c
int bt_get_mut( ... );
int bt_get_at_mut( ... );
int bt_front_mut( ... );
int bt_back_mut( ... );
void bt_iter_init_mut( ... );
int bt_scan_mut( ... );
int bt_scan_desc_mut( ... );
int bt_seek_mut( ... );
int bt_seek_desc_mut( ... );
int bt_intersects_mut( ... );
int bt_nearby_mut( ... );
int bt_seek_at_mut( ... );
int bt_seek_at_desc_mut( ... );
```
