#include <stdio.h>
#include <stdbool.h>
#include <string.h>

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

#define BGEN_NAME users
#define BGEN_TYPE struct user
#define BGEN_COMPARE return user_compare(a, b);
#include "../bgen.h"

int main() {
    // Create a new btree.
    struct users *users = 0;

    // Load some users into the btree.
    users_insert(&users, (struct user){ .first="Dale", .last="Murphy", .age=44 }, 0, 0);
    users_insert(&users, (struct user){ .first="Roger", .last="Craig", .age=68 }, 0, 0);
    users_insert(&users, (struct user){ .first="Jane", .last="Murphy", .age=47 }, 0, 0);

    struct user user;
    int status;
    printf("\n-- get some users --\n");
    users_get(&users, (struct user){ .first="Jane", .last="Murphy" }, &user, 0);
    printf("%s age=%d\n", user.first, user.age);

    users_get(&users, (struct user){ .first="Roger", .last="Craig" }, &user, 0);
    printf("%s age=%d\n", user.first, user.age);

    users_get(&users, (struct user){ .first="Dale", .last="Murphy" }, &user, 0);
    printf("%s age=%d\n", user.first, user.age);

    status = users_get(&users, (struct user){ .first="Tom", .last="Buffalo" }, &user, 0);
    printf("%s\n", status==users_FOUND?"exists":"not exists");

    printf("\n-- iterate over all users --\n");
    users_scan(&users, user_iter, 0);

    printf("\n-- iterate beginning with last name `Murphy` --\n");
    users_seek(&users, (struct user){ .first="", .last="Murphy" }, user_iter, NULL);

    printf("\n-- loop iterator (same as previous) --\n");
    struct users_iter *iter;
    users_iter_init(&users, &iter, 0);
    users_iter_seek(iter, (struct user){.first="", .last="Murphy"});
    while (users_iter_valid(iter)) {
        users_iter_item(iter, &user);
        printf("%s %s (age=%d)\n", user.first, user.last, user.age);
        users_iter_next(iter);
    }
    users_iter_release(iter);

    return 0;
}

// Output:
//
// -- get some users --
// Jane age=47
// Roger age=68
// Dale age=44
// not exists
// 
// -- iterate over all users --
// Roger Craig (age=68)
// Dale Murphy (age=44)
// Jane Murphy (age=47)
// 
// -- iterate beginning with last name `Murphy` --
// Dale Murphy (age=44)
// Jane Murphy (age=47)
// 
// -- loop iterator (same as previous) --
// Dale Murphy (age=44)
// Jane Murphy (age=47)
