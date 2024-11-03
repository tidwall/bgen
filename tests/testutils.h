#ifndef TESTUTILS_H
#define TESTUTILS_H


#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

int64_t crand(void) {
    uint64_t seed = 0;
    FILE *urandom = fopen("/dev/urandom", "r");
    assert(urandom);
    assert(fread(&seed, sizeof(uint64_t), 1, urandom));
    fclose(urandom);
    return (int64_t)(seed>>1);
}

static void seedrand(void) {
    srand(crand());
}

static void shuffle0(void *array, size_t numels, size_t elsize) {
    if (numels < 2) return;
    char tmp[512];
    assert(sizeof(tmp) >= elsize);
    char *arr = (char*)array;
    for (size_t i = 0; i < numels - 1; i++) {
        int j = i + rand() / (RAND_MAX / (numels - i) + 1);
        memcpy(tmp, arr + j * elsize, elsize);
        memcpy(arr + j * elsize, arr + i * elsize, elsize);
        memcpy(arr + i * elsize, tmp, elsize);
    }
}

static double rand_double(void) {
    return (double)rand() / (double)RAND_MAX;
}

static int compare_ints(const void *a, const void *b) {
    return *(int*)a < *(int*)b ? -1 : *(int*)a > *(int*)b;
}

static void sort(int *array, size_t numels) {
    qsort(array, numels, sizeof(int), compare_ints);
}

static void shuffle(int *array, size_t numels) {
    shuffle0(array, numels, sizeof(int));
}

static double now(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec*1e9 + now.tv_nsec) / 1e9;
}

static char *commaize(unsigned long long n) {
    char s1[64];
    char *s2 = (char*)malloc(64);
    assert(s2);
    memset(s2, 0, sizeof(64));
    snprintf(s1, sizeof(s1), "%llu", n);
    int i = strlen(s1)-1; 
    int j = 0;
    while (i >= 0) {
        if (j%3 == 0 && j != 0) {
            memmove(s2+1, s2, strlen(s2)+1);
            s2[0] = ',';
    }
        memmove(s2+1, s2, strlen(s2)+1);
        s2[0] = s1[i];
        i--;
        j++;
    }
    return s2;
}

#define bench_print_mem_rounds_scale(n, start, end, mstart, mend, rounds, scale) { \
    double fscale = (double)(n) / (double)(scale); \
    double elapsed = ((end) - (start)) * fscale; \
    double nsop = elapsed/(double)(n)/(double)(rounds)*1e9; \
    char *pops = commaize((n)); \
    char *psec = commaize((double)(n)*(double)(rounds)/elapsed); \
    printf("%10s ops in %7.3f secs %8.1f ns/op %13s op/sec", \
        pops, elapsed, nsop, psec); \
    if ((mstart) < (mend)) { \
        printf(" %7.2f bytes/op", (double)((mend)-(mstart))/(double)(n)); \
    } \
    if ((n) != (scale)) { \
        printf(" *"); \
    } \
    printf("\n"); \
}

#define bench_print_mem(n, start, end, mstart, mend) { \
    bench_print_mem_rounds_scale(n, start, end, mstart, mend, 1, n); \
}

#define bench_print_rounds(n, start, end, rounds) { \
    bench_print_mem_rounds_scale(n, start, end, 0, 0, rounds, n); \
}

#define bench_print(n, start, end) { \
    bench_print_mem_rounds_scale(n, start, end, 0, 0, 1, n); \
}

#define bench_print_scale(n, start, end, scale) { \
    bench_print_mem_rounds_scale(n, start, end, 0, 0, 1, scale); \
}


static atomic_size_t nallocs = 0;
static atomic_size_t mtotal = 0;

static void *malloc0(size_t size) {
    char *cptr = (char*)malloc(16+size);
    if (!cptr) {
        return 0;
    }
    *(size_t*)cptr = size;
    atomic_fetch_add(&mtotal, size);
    atomic_fetch_add(&nallocs, 1);
    return cptr+16;
}

static void *realloc0(void *ptr, size_t size) {
    if (!ptr) {
        return malloc0(size);
    }
    char *cptr = (char*)realloc(((char*)ptr)-16, 16+size);
    if (!cptr) {
        return 0;
    }
    atomic_fetch_sub(&mtotal, *(size_t*)cptr);
    atomic_fetch_sub(&mtotal, size);
    *(size_t*)cptr = size;
    return cptr+16;
}

static void free0(void *ptr) {
    if (!ptr) {
        return;
    }
    char *cptr = ((char*)ptr)-16;
    atomic_fetch_sub(&mtotal, *(size_t*)cptr);
    atomic_fetch_sub(&nallocs, 1);
    free(((char*)ptr)-16);
}

static void *calloc0(size_t n, size_t size) {
    // return calloc(n, size);
    void *ptr = malloc0(size*n);
    if (!ptr) {
        return 0;
    }
    memset(ptr, 0, size*n);
    return ptr;
}

static void checkmem(void) {
    if (atomic_load(&nallocs) > 0 || atomic_load(&mtotal) > 0) {
        fprintf(stderr, "test failed: %d unfreed allocations, %d bytes\n",
            (int)atomic_load(&nallocs), (int)atomic_load(&mtotal));
        exit(1);
    }
}

static void initrand(void) {
    uint64_t seed;
    if (getenv("SEED")) {
        seed = strtoull(getenv("SEED"), 0, 10);
    } else {
        seed = crand();
        printf("SEED=%llu\n", (unsigned long long)seed);
    }
    // printf("SEED=%llu\n", (unsigned long long)seed);
    srand(seed);
}

#ifdef TESTNAME
static const char *testname = TESTNAME;
#else
static const char *testname = "";
#endif

#define testinit() { \
    (void)now, (void)commaize, (void)realloc0, (void)calloc0, (void)free0; \
    (void)seedrand, (void)sort, (void)shuffle, (void)checkmem; \
    (void)rand_double; \
    if (strstr(__func__, "test_") == __func__) { \
        char name[250]; \
        snprintf(name, sizeof(name), "test_%s_%s", testname, \
            strstr(__func__, "test_")+5); \
        fprintf(stderr, "%s\n", name); \
    } else { \
        fprintf(stderr, "%s\n", __func__); \
    } \
}

#endif
