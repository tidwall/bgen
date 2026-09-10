# btree.h

[![api reference](https://img.shields.io/badge/api-reference-blue.svg)](docs/API.md)

A [B-tree](https://en.wikipedia.org/wiki/B-tree) generator for C.
It's small & fast and includes a variety of options for creating custom
in-memory btree based collections.

## Features

- Compile-time generation using preprocessor templates
- Type-safe generic data structure
- Single-file header with no dependencies
- [Namespaces](#namespaces)
- Support for [custom allocators](#custom-allocators)
- Callback and loop-based [iteration](#iterators)
- [Copy-on-write](#copy-on-write) with O(1) cloning.
- Loads of useful [toggles and options](#options)
- Enable specialized btrees
  - [Counted B-tree](#counted-b-tree)
  - [Vector B-tree](#vector-b-tree)
- Supports most C compilers (C99+). Clang, gcc, tcc, etc
- Webassembly support with Emscripten (emcc)
- Exhaustively [tested](tests/README.md) with 100% coverage
- [Very fast](#performance) 🚀

## Goals

- Give C programs high performance in-memory btrees
- Provide a template system for optimized code generation
- Allow for sane customizations and options
- Make it possible to use one btree library for a variety of collection types,
  such as maps, sets, stacks, queues, lists, and vectors. 
  See the [examples](examples).

It's a non-goal for btree.h to provide disk-based functionality or a B+tree
implementation.

## Using

Just drop the "btree.h" into your project and create your btree using the 
C preprocessor.

## Example 1 (Insert items)

Insert items into a simple btree that only stores ints.

```c
#include <stdio.h>

#define BTREE_NAME bt            // The namespace for the btree structure.
#define BTREE_TYPE int           // The data type for all items in the btree
#define BTREE_LESS return a < b; // A code fragment for comparing items
#include "btree.h"               // Include "btree.h" to generate the btree

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
// Output:
// 2 3 5 8
// 2 5 8
```

## Example 2 (Key-value map)

Create a key-value map where the key is a string and value is an int.

```c
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

struct pair {
    const char *key;
    int value;
};

#define BTREE_NAME map
#define BTREE_TYPE struct pair
#define BTREE_COMPARE return strcmp(a.key, b.key);
#include "btree.h"

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
```

## Example 3 (Priority queue)

Create two [priority queues](https://en.wikipedia.org/wiki/Priority_queue).
One ordered by the maximum value and the other by the minimum value.

```c
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#define BTREE_NAME max_priority_queue
#define BTREE_TYPE int
#define BTREE_LESS return a < b;
#include "btree.h"

#define BTREE_NAME min_priority_queue
#define BTREE_TYPE int
#define BTREE_LESS return b < a;
#include "btree.h"

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
```

Check out the [examples](examples) directory for more examples, and
the [API reference](docs/API.md) for the full list of operations.

## Options

btree.h provides a bunch of options for customizing your btree. All options are
set using the C preprocessor.

| Option                        | Description |
| :---------------------------- | :---------- |
| BTREE_NAME `<kv>`             | The [Namespace](#namespaces) |
| BTREE_TYPE `<type>`           | The btree item type |
| BTREE_FANOUT `<int>`          | Set the [fanout](#fanout) (max number of children per node) |
| BTREE_LESS `<code>`           | Define a "less" [comparator](#comparators). Such as "a<b" |
| BTREE_COMPARE `<code>`        | Define a "compare" [comparator](#comparators). Such as "a<b?-1:a>b" |
| BTREE_MAYBELESSEQUAL `<code>` | Define a [less-equal hint](#less-equal-hint) for complex compares (advanced) |
| BTREE_MALLOC `<code>`         | Define [custom malloc](#custom-allocators) function |
| BTREE_FREE `<code>`           | Define [custom free](#custom-allocators) function |
| BTREE_BSEARCH                 | Enable [binary searching](#binary-search-or-linear-search) (otherwise [linear](#binary-search-or-linear-search)) |
| BTREE_COW                     | Enable [copy-on-write](#copy-on-write) support |
| BTREE_COUNTED                 | Enable [counted btree](#counted-b-tree) support |
| BTREE_NOORDER                 | Disable all ordering. (btree becomes a [dynamic array](#vector-b-tree)) |
| BTREE_NOATOMICS               | Disable atomics for [copy-on-write](#copy-on-write) (single threaded only) |
| BTREE_PATHHINT                 | Enable path hints ([path hints](#path-hints)) |
| BTREE_ITEMCOPY `<code>`       | Define operation for [internally copying items](#item-copying-and-freeing) |
| BTREE_ITEMFREE `<code>`       | Define operation for [internally freeing items](#item-copying-and-freeing) |
| BTREE_HEADER                  | Generate header declaration only. See [Header and source](#header-and-source) |
| BTREE_SOURCE                  | Generate source declaration only. See [Header and source](#header-and-source) |

## Namespaces

Each btree.h btree will have its own namespace using the `BTREE_NAME` define.

For example, the following will create a btree using the `users` namespace.

```c
#define BTREE_NAME users
#define BTREE_TYPE struct user
#define BTREE_LESS return a.id < b.id;
#include "btree.h"
```

This will generate all the functions and types using the `users` prefix, such as:

```c
struct users; // The btree type
int users_get(struct users **root, struct user key, struct user *item, void *udata);
int users_insert(struct users **root, struct user item, struct user *old, void *udata);
int users_delete(struct users **root, struct user key, struct user *old, void *udata);
```

Many more functions will also be generated, see the [API](docs/API.md) for a complete list.

It's also possible to generate multiple btrees in the same source file.

```c
#define BTREE_NAME users
#define BTREE_TYPE struct user
#define BTREE_LESS return a.id < b.id;
#include "btree.h"

#define BTREE_NAME orders
#define BTREE_TYPE struct order
#define BTREE_LESS return a.id < b.id;
#include "btree.h"

#define BTREE_NAME events
#define BTREE_TYPE struct event
#define BTREE_LESS return a.id < b.id;
#include "btree.h"
```

For the remainder of this README, and unless otherwise specified, the prefix
`bt` will be used as the namespace.

## Comparators

Every btree requires one comparator, which is a code fragment that compares two
items, using BTREE_LESS or BTREE_COMPARE. 

btree.h provides three variables to the code fragment `a`, `b`, and `udata`.
The `a` and `b` variables are the items that need to be compared, and `udata` is
optional [user data](#the-udata-parameter) that may be provided to any btree.h
operation.

```c
#define BTREE_LESS    return a < b;               /* return true or false */
#define BTREE_COMPARE return a < b ? -1 : a > b;  /* return -1, 0, 1 */
```

It's up to the developer to choose which of the two is most appropriate. 
But in general, BTREE_LESS is a good choice for numeric comparisons and
BTREE_COMPARE may be better suited for strings and more complex keys. 

## Binary search or Linear search

btree.h defaults to linear searching. This means that btree operations will 
perform internal searches by scanning the items one-by-one. This is often very
cache-efficient, providing excellent performance for [small nodes](#fanout).

Optionally the BTREE_BSEARCH may be used to enable binary searches instead of
linear. This may be better for large nodes or where comparing items may be slow.

## Less-equal hint

The BTREE_MAYBELESSEQUAL is a code fragment option that may be provided as an
optimization to speed up linear searches for complex comparisons.
More specifically for tuple-like items with composite keys, where the leading
field in the tuple is numeric and the other fields are indirect such as a
pointer to a string.

btree.h provides three variables to the code fragment `a`, `b`, and `udata`.

For example, let's say you have a btree index "status_users" btree that orders
on the composite key (status,name).

```c
struct status_user {
    int status;
    char *name;
    char *desc;
};

int user_compare(struct user a, struct user b) {
    return a.status < b.status ? -1 : a.status > b.status ? 1 : 
           strcmp(a.name, b.name);
}

#define BTREE_NAME            status_users
#define BTREE_TYPE            struct status_user
#define BTREE_COMPARE         return user_compare(a, b);
#define BTREE_MAYBELESSEQUAL  return a.status <= b.status;
#include "btree.h"
```

With the BTREE_MAYBELESSEQUAL option, the btree will perform a quick linear
search on status and fallback to the slower user_compare function when needed.

Note that BTREE_MAYBELESSEQUAL is only for linear searches cannot be used in 
combination with BTREE_BSEARCH. 

## Copy-on-write

btree.h provides [copy-on-write](#copy-on-write) support when BTREE_COW is provided.
If enabled, the `bt_clone()` function can make an instant O(1) copy of the
btree.
This implementation uses atomic reference counters to monitor the shared state
of each node and preforms just-in-time copies of nodes for mutable operations,
such as `bt_insert()` and `bt_delete()`. 

The `BTREE_NOATOMIC` option may be provided to disable atomics, instead using
normal integers as reference counters. This may be needed for single-threaded
programs, embedded environments, or webassembly.

With BTREE_COW; while all mutable operations will perform copy-on-write
internally, immutable operations such as `bt_get()` will not.
It is possible to force the btree to perform copy-on-write for otherwise
immutable operations by using the their `_mut()` alternatives. 
For example, `bt_get() / bt_get_mut()` and 
`bt_iter_init() / bt_iter_init_mut()`. 

## Fanout

The fanout is the maximum number of children an internal btree node may have.
btree.h allows for setting the fanout using the BTREE_FANOUT option.
The default is 16.

Choosing the best fanout is dependent on a number of factors such as item size,
key types, and system architecture. 
In general, 8, 16, or 32 are typically pretty good choices.

## Custom allocators

The BTREE_MALLOC and BTREE_FREE can be used to provide a custom allocator for 
all btree operations. By default, the built-in `malloc()` and `free()`
functions from `<stdlib.h>` are used. 

BTREE_MALLOC provides the `size` and `udata` variables.  
BTREE_FREE provides the `ptr`, the original `size`, and `udata` variables.

```c
#define BTREE_MALLOC return mymalloc(size);
#define BTREE_FREE   myfree(ptr);
```

btree.h is designed for graceful error handling when malloc fails.
All mutable btree operations such as `bt_insert()` may fail when attempting to
allocate memory. It's generally a good idea to check for the `bt_NOMEM` 
[status code](#status-codes). 

## Item copying and freeing

When the `bt_copy()`, `bt_clone()`, and `bt_clear()` functions are 
used, the btree will internally copy and free nodes. 
With BTREE_ITEMFREE and BTREE_ITEMCOPY, it's possible to also have the btree copy 
and free items.

This may be needed when items have internal memory allocations, such as strings
or other heap-based fields, that require isolation per btree instance and to
avoid memory corruptions such as double free errors.

BTREE_ITEMCOPY provides the `item`, `copy`, and `udata` variables.
BTREE_ITEMFREE provides the `item` and `udata` variables.

For example:

```c
struct user {
    int id;
    char *name;
};

bool copy_user(struct user item, struct user *copy) {
    copy->name = malloc(strlen(item.name)+1);
    if (!copy->name) {
        return false;
    }
    strcpy(copy->name, item.name);
    copy->id = item->id;
    return true;
}

void free_user(struct user item) {
    free(item.name);
}

#define BTREE_NAME users
#define BTREE_TYPE struct user
#define BTREE_LESS a.id < b.id
#define BTREE_ITEMCOPY return copy_user(item, copy);
#define BTREE_ITEMFREE free_user(item);
#include "btree.h"
```

Now when `users_clear()` is called all items will also be freed with 
`free_user()`, and when `users_clone()` or `users_copy()` are called items will
automatically be copied with `copy_user()`.

The BTREE_ITEMCOPY expects a return value of `true` or `false`, where `false`
means that there was an error such as out of memory. 

## Path hints

btree.h allows for path hints using the BTREE_PATHHINT option.

It's an automatic search optimization which causes the btree to track the
search path of every operation, using that path as a hint for the next
operation.

It can lead to better performance for common access patterns, where subsequent 
operations work on items that are typically nearby each other in the btree.

For more information see the 
[original document](https://github.com/tidwall/btree/blob/master/PATH_HINT.md).

This implementation uses a thread-local variable to manage the hint.
Other than providing BTREE_PATHHINT, there are no additional requirements to make this feature work.

## Iterators

Iteration comes in two flavors, callback and loop-based. 

Callback iteration requires a callback function that will be called for each
item in the iteration. 

For example, let's say you have a `users` btree that orders users on
(last,first).

```c
struct user {
    char *last;
    char *first;
    int age;
};

int user_compare(struct user a, struct user b) {
    int cmp = strcmp(a.last, b.last);
    if (cmp == 0) {
        cmp = strcmp(a.first, b.first);
    }
    return cmp;
}

bool user_iter(struct user user, void *udata) {
    printf("%s %s (age=%d)\n", user.first, user.last, user.age);
    return true;
}

#define BTREE_NAME users
#define BTREE_TYPE struct user
#define BTREE_COMPARE { return user_compare(a, b); }
#include "btree.h"
```

Callback iterators such as `bt_scan()` and `bt_seek()` are available.

```c
bt_scan(&tree, user_iter, 0);
```

Loop iteration allows for keeping the iterator from leaving the current 
function. It takes a little more work to set up but is sometimes easier t
manage the context of operation.

```c
struct users_iter *iter;
users_iter_init(&users, &iter, 0);
users_iter_scan(iter);
while (users_iter_valid(iter)) {
    users_iter_item(iter, &user);
    printf("%s %s (age=%d)\n", user.first, user.last, user.age);
    users_iter_next(iter);
}
users_iter_release(iter);
```

It's usually not safe to modify the btree while iterating. 
If you need to filter data then it's best to reset the iterator after
each modification.

```c
struct users_iter *iter;
users_iter_init(&users, &iter, 0);
users_iter_scan(iter);
while (users_iter_valid(iter)) {
    users_iter_item(iter, &user);
    if (user.age >= 30 && user.age < 40) {
        users_delete(&users, user, 0, 0);
        users_iter_seek(iter, user);
        continue;
    } 
    users_iter_next(iter);
}
users_iter_release(iter);
```

Make sure to call `bt_iter_release()` when you are done iterating;

## Status codes 

Most btree operations, such as `bt_get()` and `bt_insert()` return status
codes that indicate the success of the operation. All status codes are prefixed
with the same namespace as specified with BTREE_NAME. 

| Status         | Description |
| :------------- | :--- |
| bt_INSERTED    | New item was inserted |
| bt_REPLACED    | Item replaced an existing item |
| bt_DELETED     | Item was successfully deleted |
| bt_FOUND       | Item was successfully found |
| bt_NOTFOUND    | Item was not found |
| bt_OUTOFORDER  | Item cannot be inserted due to out of order |
| bt_FINISHED    | Callback iterator returned all items |
| bt_STOPPED     | Callback iterator was stopped early |
| bt_COPIED      | Tree was copied: `bt_clone()`, `bt_copy()` |
| bt_NOMEM       | Out of memory error |
| bt_UNSUPPORTED | Operation not supported |

It's always a good idea to check the return value of mutable btree operations to 
ensure it doesn't return an error.

## The udata parameter

All btree.h functions provide an optional `udata` parameter that may be used for
user-defined data. What this data is used for is up to the developer.

All operations, callbacks, and code fragments (such as BTREE_COMPARE and 
BTREE_LESS) provide a `udata` variable that is the same as what is passed to 
original btree function.

## Counted B-tree

A [counted btree](https://www.chiark.greenend.org.uk/~sgtatham/algorithms/cbtree.html) 
allows for random access and modifications with O(log n) complexity.

Adding the BTREE_COUNTED option enables this feature.

This is pretty nice for programs that need to make changes using an index, 
rather than a key. It basically allows for functions like `bt_insert_at()`, 
`bt_delete_at()`, and `bt_get_at()` to modify and access items at any position.

But it's worth noting that the `bt_insert_at()` operation still requires that
items inserted at specific positions are in the correct order.
The `bt_OUTOFORDER` error will be returned otherwise.

## Vector B-tree

When the BTREE_COUNTED and BTREE_NOORDER options are both provided, btree.h will
generate a specialized btree that allows for both random access and storing
items in any order.
This effectively treats the btree like a dynamic array, aka a vector.

Those familiar with vectors in other languages, such a Rust and C++, may know 
that appending and accessing items is fast but modifying is slow.

With a btree.h vector all operations have the same
[time complexity](https://en.wikipedia.org/wiki/Time_complexity).

| Operation  | btree.h     | Others       |
| :--------  | :------- | :----------- |
| push_back  | O(log n) | O(1)         |
| pop_back   | O(log n) | O(1)         |
| get_at     | O(log n) | O(1)         |
| push_front | O(log n) | O(n)         |
| pop_front  | O(log n) | O(n)         |
| insert_at  | O(log n) | O(n)         |
| delete_at  | O(log n) | O(n)         |

Here's how to create a vector that stores ints.

```c
#define BTREE_NAME vector
#define BTREE_TYPE int
#define BTREE_COUNTED
#define BTREE_NOORDER
#include "btree.h"
```

Now `vector_insert_at()`, `vector_delete_at()`, and `vector_get_at()` can be
used to modify and access items at any position, in any order. 

For a more detailed example, check out the [examples](examples) directory.

## Header and source

By default, btree.h generates all the code as a static unit for the current source
file that includes "btree.h".

This is great if all you need to access the btree from that one file.
But if you want other c source files to access those same btree functions too
then you'll use the `BTREE_HEADER` and `BTREE_SOURCE` options.

For example, here we'll create a "users.h" and "users.c" where one generates
only the header declarations and the other generates the code.

```c
// users.h
#ifndef USERS_H
#define USERS_H

struct user {
    int id;
    char *name;
};

#define BTREE_NAME users
#define BTREE_TYPE struct user
#define BTREE_HEADER
#include "../deps/btree.h"

#endif
```

```c
// users.c
#include "users.h"

#define BTREE_NAME users
#define BTREE_TYPE struct user
#define BTREE_LESS return a.id < b.id;
#define BTREE_SOURCE
#include "../deps/btree.h"
```


## Performance

The following benchmarks compare the performance of btree.h to the very fast
[frozenca/btree](https://github.com/frozenca/BTree) for C++ and the built-in
Rust B-tree.

*See the [tidwall/btree-bench](https://github.com/tidwall/btree-bench) project
for more information*

### Details 

- Linux, AMD Ryzen 9 5950X 16-Core processor
- CC=clang-17 CFLAGS=-ljemalloc
- Items are simple 4-byte ints. 

Benchmarking 1000000 items, 50 times, taking the average result

## btree.h B-tree

```
insert(seq)         1,000,000 ops in   0.042 secs     41.8 ns/op    23,933,327 op/sec
insert(rand)        1,000,000 ops in   0.087 secs     86.7 ns/op    11,539,702 op/sec
get(seq)            1,000,000 ops in   0.030 secs     30.3 ns/op    32,989,495 op/sec
get(rand)           1,000,000 ops in   0.078 secs     78.0 ns/op    12,814,152 op/sec
delete(seq)         1,000,000 ops in   0.018 secs     17.7 ns/op    56,342,904 op/sec
delete(rand)        1,000,000 ops in   0.096 secs     96.4 ns/op    10,369,073 op/sec
reinsert(rand)      1,000,000 ops in   0.082 secs     82.4 ns/op    12,138,316 op/sec
push_first          1,000,000 ops in   0.009 secs      8.6 ns/op   116,842,897 op/sec
push_last           1,000,000 ops in   0.010 secs      9.8 ns/op   101,998,378 op/sec
pop_first           1,000,000 ops in   0.012 secs     12.3 ns/op    81,491,602 op/sec
pop_last            1,000,000 ops in   0.012 secs     12.1 ns/op    82,480,762 op/sec
scan                1,000,000 ops in   0.002 secs      1.5 ns/op   665,448,960 op/sec
scan_desc           1,000,000 ops in   0.002 secs      1.8 ns/op   561,393,712 op/sec
iter_scan           1,000,000 ops in   0.004 secs      3.6 ns/op   280,244,979 op/sec
iter_scan_desc      1,000,000 ops in   0.004 secs      4.0 ns/op   248,567,689 op/sec
```

## Rust B-tree

```
insert(seq)         1,000,000 ops in   0.049 secs     48.6 ns/op    20,574,261 op/sec
insert(rand)        1,000,000 ops in   0.105 secs    105.4 ns/op     9,489,152 op/sec
get(seq)            1,000,000 ops in   0.034 secs     33.7 ns/op    29,706,515 op/sec
get(rand)           1,000,000 ops in   0.095 secs     94.6 ns/op    10,568,904 op/sec
delete(seq)         1,000,000 ops in   0.023 secs     22.6 ns/op    44,236,754 op/sec
delete(rand)        1,000,000 ops in   0.116 secs    115.8 ns/op     8,635,239 op/sec
reinsert(rand)      1,000,000 ops in   0.097 secs     97.1 ns/op    10,299,834 op/sec
```

## C++ B-tree ([frozenca/btree](https://github.com/frozenca/BTree))

```
insert(seq)         1,000,000 ops in   0.054 secs     54.2 ns/op    18,435,446 op/sec
insert(rand)        1,000,000 ops in   0.088 secs     88.0 ns/op    11,369,690 op/sec
get(seq)            1,000,000 ops in   0.030 secs     29.5 ns/op    33,894,683 op/sec
get(rand)           1,000,000 ops in   0.080 secs     79.5 ns/op    12,573,739 op/sec
delete(seq)         1,000,000 ops in   0.023 secs     23.2 ns/op    43,042,237 op/sec
delete(rand)        1,000,000 ops in   0.113 secs    113.4 ns/op     8,815,550 op/sec
reinsert(rand)      1,000,000 ops in   0.101 secs    100.9 ns/op     9,909,315 op/sec
```

## Contributing

Read [CONTRIBUTING.md](.github/CONTRIBUTING.md), but in general please
do not open a PR without talking to me first.

