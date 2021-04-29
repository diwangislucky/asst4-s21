#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <error.h>
#include <limits.h>
#include <pthread.h>

// TODO: You may find this helpful.
#include <mpi.h>

#include "wsp.h"

/*
________________________________________________

 FRIENDLY ADVICE FROM YOUR TAs:
________________________________________________

0) Read over the file, familiarizing yourself
with the provided functions. You are free to
change structs and add additional helper 
functions, but should not modify the
argument parsing or printing mechanisms. 

1) Start by developing a sequential solution.
A parallelized approach is useless here if it 
sacrifices correctness for speed.

2) Run the verify.py script to verify the
correctness of your outputs compared to
our reference solution on smaller test cases.
We also encourage you to compare your results
against the reference manually on larger
test cases. 

NOTE: there may be multiple correct paths 
for the same distance file. We are only
looking for you to produce the correct
distance cost.

3) Start with simple parallelization
techniques that do not require large
restructuring of your code (Hint: openMP
may come in handy at this stage). Be mindful
of not accidentally introducing uninetntional
dependencies within parallel sections of code
that will unwantingly serialize code.

4) If you still do not have enough of a 
performance increase, you may need to 
introduce larger structural changes to
your code to help make it more amendable
to parallelism. Attempt small iterative
changes while still ensuring the correctness
of your code.

we introduced static granularity limits.
also we need to control granularity of dynamic heuristics 
based on number of processors.

Let us now introduce heuristics 
A simple heuristic.

let us now introduce another thing: not a simple heuristic
________________________________________________
*/ 

#define SYSEXPECT(expr) do { if(!(expr)) { perror(__func__); exit(1); } } while(0)
#define error_exit(fmt, ...) do { fprintf(stderr, "%s error: " fmt, __func__, ##__VA_ARGS__); exit(1); } while(0);

typedef int8_t city_t;

int NCORES = -1;  // TODO: this isn't being used anywhere, this needs to be used somewhere.
int NCITIES = -1; // number of cities in the file.
int *DIST = NULL; // one dimensional array used as a matrix of size (NCITIES * NCITIES).


typedef struct path_struct_t {
  int cost;         // path cost.
  city_t *path;     // order of city visits (you may start from any city).
} path_t;
path_t *bestPath = NULL;

// set DIST[i,j] to value
// what's cache line size?
inline static void set_dist(int i, int j, int value) {
  assert(value > 0);
  int offset = i * NCITIES + j;
  DIST[offset] = value;
  return;
}

// returns value at DIST[i,j]
inline static int get_dist(int i, int j) {
  int offset = i * NCITIES + j;
  return DIST[offset];
}

// prints results
void wsp_print_result() {
  printf("========== Solution ==========\n");
  printf("Cost: %d\n", bestPath->cost);
  printf("Path: ");
  for(int i = 0; i < NCITIES; i++) {
    if(i == NCITIES-1) printf("%d", bestPath->path[i]);
    else printf("%d -> ", bestPath->path[i]);
  }
  putchar('\n');
  putchar('\n');
  return;
}

static inline void swap(city_t *ptr1, city_t *ptr2) {
    city_t temp = *ptr1;
    *ptr1 = *ptr2;
    *ptr2 = temp;
    return;
}

// how can we make a threadlocal copy of scratch?
// without making it seem like a dick move
void wsp_print_scratch(city_t *scratch) {
  printf("Path: ");
  for(int i = 0; i < NCITIES; i++) {
    if(i == NCITIES-1) printf("%d", scratch[i]);
    else printf("%d -> ", scratch[i]);
  }
  putchar('\n');
  return;
}

// granularity limit
int gran_limit = 6;

static inline int wsp_heuristic(int idx, city_t *scratch) {
  // what heuristic?
  // how about two minimums
  if (NCITIES == idx + 1) {
    return 0;
  }
  if (NCITIES == idx + 2) {
    city_t pIdx1 = scratch[idx];
    city_t pIdx2 = scratch[idx + 1];
    return get_dist(pIdx1, pIdx2);
  }
  int first_min = INT_MAX, second_min = INT_MAX;
  for (; idx < NCITIES - 1; ++idx) {
    city_t pIdx1 = scratch[idx];
    city_t pIdx2 = scratch[idx + 1];
    int current_dist = get_dist(pIdx1, pIdx2);
    if (second_min > current_dist) {
      if (first_min > current_dist) {
        second_min = first_min;
        first_min = current_dist;
      }
    }
  }
  assert(first_min != INT_MAX && second_min != INT_MAX);
  return (first_min + second_min) / 2 * (NCITIES - idx - 1);
}

void wsp_recursion_seq(int idx, int sum_dist, city_t *scratch,
		int *min_dist, city_t *best_path) {
  // base case
  // be careful about NCITIES, that might be a bit of a doozy w.r.t. 
  //
  // cache bouncing
  if (sum_dist + wsp_heuristic(idx, scratch) > *min_dist) {
    return;
  }
  if (idx == NCITIES) {
    // wsp_print_scratch(scratch);
    if (*min_dist > sum_dist) {
      memcpy(best_path, scratch, NCITIES * sizeof(city_t));
      *min_dist = sum_dist;
    }
    return;
  }
  for (int i = idx; i < NCITIES; ++i) {
    // let's try making this properly
    swap(scratch + idx, scratch + i);
    if (idx > 0) {
      city_t pIdx1 = scratch[idx - 1];
      city_t pIdx2 = scratch[idx];
      wsp_recursion_seq(idx + 1, sum_dist + get_dist(pIdx1, pIdx2), 
		      scratch, min_dist, best_path);
    }
    else {
      wsp_recursion_seq(idx + 1, sum_dist, scratch, min_dist, best_path);
    }
    swap(scratch + idx, scratch + i);
  }
  return;
}

// why is 8 optimal
#define GRAN_LIMIT 2

// best path is initialized to idxs
// let's avoid using heuristics.
//
// trying to make everything local now to ease parallelization
// min_dist is the minimum distance.
// best_path is a pointer to an array containing the best path
// scratch is a pointer to an array containing the scratchwork at that level
void wsp_recursion(int idx, int sum_dist, city_t *top_scratch,
		int *min_dist, city_t *best_path) {
  // base case
  // be careful about NCITIES, that might be a bit of a doozy w.r.t. 
  //
  // cache bouncing
  if (sum_dist + wsp_heuristic(idx, scratch) > *min_dist) {
    return;
  }
  if (idx == NCITIES) {
    // wsp_print_scratch(scratch);
    if (*min_dist > sum_dist) {
      memcpy(best_path, scratch, NCITIES * sizeof(city_t));
      *min_dist = sum_dist;
    }
    return;
  }
  for (int i = idx; i < NCITIES; ++i) {
    // let's try making this properly
    swap(scratch + idx, scratch + i);
    if (idx > 0) {
      city_t pIdx1 = scratch[idx - 1];
      city_t pIdx2 = scratch[idx];
      wsp_recursion_seq(idx + 1, sum_dist + get_dist(pIdx1, pIdx2), 
		      scratch, min_dist, best_path);
    }
    else {
      wsp_recursion_seq(idx + 1, sum_dist, scratch, min_dist, best_path);
    }
    swap(scratch + idx, scratch + i);
  }
  return;
}

// We can have the other threads just wait. Locking is costly`
void wsp_start() {
  int cityID = 0;
  // bestPath->cost = 0;
  // bestPath->path = (city_t*)calloc(NCITIES, sizeof(city_t));
  assert(NCORES > 0);
  city_t scratch[32];
  for(cityID=0; cityID < NCITIES; cityID++) {
    scratch[cityID] = cityID;
  }

  // printf("NCITIES: %d\n", NCITIES);
  omp_set_num_threads(NCORES);
  if (NCORES == 1) {
    wsp_recursion_seq(0, 0, scratch, &(bestPath->cost), bestPath->path);
    return;
  }

  double start, end;
  start = omp_get_wtime();

  wsp_recursion(0, 0, scratch, &(bestPath->cost), bestPath->path);
  end = omp_get_wtime();

  double delta_s = end - start;
  printf("thread num: %d, time: %.3f ms (%.3f s)\n", 
          omp_get_thread_num(), delta_s * 1000.0, delta_s);

  return;
}
