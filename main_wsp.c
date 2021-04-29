/** @file main_wsp.c
 *
 *  @brief main wrapper for wsp
 *
 *  @author Di Wang (diwang2)
 */
#include <stdio.h>
#include <mpi.h>
#include "wsp.h"

/* 
// TODO: we need to consider modifying this to suit openmpi
int main(int argc, char **argv) {
  if (argc < 4 || strcmp(argv[1], "-p") != 0)\
      error_exit("Expecting two arguments: -p [processor count]"
              " and [file name]\n");
  NCORES = atoi(argv[2]);
  if(NCORES < 1) error_exit("Illegal core count: %d\n", NCORES);
  char *filename = argv[3];
  FILE *fp = fopen(filename, "r");
  if(fp == NULL) error_exit("Failed to open input file \"%s\"\n", filename);
  int scan_ret;
  scan_ret = fscanf(fp, "%d", &NCITIES);
  if(scan_ret != 1) error_exit("Failed to read city count\n");
  if(NCITIES < 2) {
    error_exit("Illegal city count: %d\n", NCITIES);
  } 
  // Allocate memory and read the matrix
  DIST = (int*)calloc(NCITIES * NCITIES, sizeof(int));
  SYSEXPECT(DIST != NULL);
  for(int i = 1;i < NCITIES;i++) {
    for(int j = 0;j < i;j++) {
      int t;
      scan_ret = fscanf(fp, "%d", &t);
      if(scan_ret != 1) error_exit("Failed to read dist(%d, %d)\n", i, j);
      set_dist(i, j, t);
      set_dist(j, i, t);
    }
  }
  fclose(fp);
  bestPath = (path_t*)malloc(sizeof(path_t));
  bestPath->cost = INT_MAX;
  bestPath->path = (city_t*)calloc(NCITIES, sizeof(city_t));
  struct timespec before, after;
  clock_gettime(CLOCK_REALTIME, &before);
  wsp_start();
  clock_gettime(CLOCK_REALTIME, &after);
  double delta_ms = (double)(after.tv_sec - before.tv_sec) * 1000.0
      + (after.tv_nsec - before.tv_nsec) / 1000000.0;
  putchar('\n');
  printf("============ Time ============\n");
  printf("Time: %.3f ms (%.3f s)\n", delta_ms, delta_ms / 1000.0);
  wsp_print_result();
  return 0;
}
*/

int main (int argc, char* argv[])
{
    int procID;
    int nproc;
    double startTime;
    double endTime;

    // Initialize MPI
    MPI_Init(&argc, &argv);

    // Get process rank
    MPI_Comm_rank(MPI_COMM_WORLD, &procID);

    // Get total number of processes specificed at start of run
    MPI_Comm_size(MPI_COMM_WORLD, &nproc);

    // Run computation
    startTime = MPI_Wtime();
    // yeah we need procID and nproc
    // also how many processes are we spawning?
    //
    // Finally we also need processes and to modify accordingly
    compute(procID, nproc);
    endTime = MPI_Wtime();

    // Compute running time
    MPI_Finalize();
    printf("elapsed time for proc %d: %f\n", procID, endTime - startTime);
    return 0;
}
