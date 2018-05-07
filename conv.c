// conv.c
// Name: Tanay Agarwal, Nirmal Krishnan
// JHED: tagarwa2, nkrishn9

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "mpi.h"

#define DEFAULT_ITERATIONS 1

// documentation: http://mpitutorial.com/tutorials/mpi-send-and-receive/
// http://mpitutorial.com/tutorials/dynamic-receiving-with-mpi-probe-and-mpi-status/

void update_global(int *, int, int, int);
void update_global(int * main_grid, int nrows, int num_procs, int DIM) {
  for(int i = 1; i < num_procs; i++) {
    MPI_Send(main_grid, DIM * DIM, MPI_INT, i, 10, MPI_COMM_WORLD);
  }
}

int conv(int *, int, int, int, int *);
int conv(int * sub_grid, int i, int nrows, int DIM, int * kernel) {
  int counter = 0;

  if (i % DIM != 0) {
    counter = counter + sub_grid[i - DIM - 1] * kernel[0];
    counter = counter + sub_grid[i - 1] * kernel[3];
    counter = counter + sub_grid[i + DIM - 1] * kernel[6];
  }

  if ((i + 1) % DIM != 0) {
    counter = counter + sub_grid[i - DIM + 1] * kernel[2];
    counter = counter + sub_grid[i + 1] * kernel[5];
    counter = counter + sub_grid[i + DIM + 1] * kernel[8];
  }
  
  counter = counter + sub_grid[i + DIM] * kernel[7];
  counter = counter + sub_grid[i - DIM] * kernel[1];
  counter = counter + sub_grid[i] * kernel[4];
  
  return counter;
}


int * check(int *, int, int, int *);
int * check(int * sub_grid, int nrows, int DIM, int * kernel) {
  int val;
  int * new_grid = calloc(DIM * nrows, sizeof(int));
  for(int i = DIM; i < (DIM * (1 + nrows)); i++) {
    val = conv(sub_grid, i, nrows, DIM, kernel);
    new_grid[i - DIM] = val;
  }
  return new_grid;
}

int main ( int argc, char** argv ) {

  // MPI Standard variable
  int num_procs;
  int ID, j;
  int iters = 0;
  int num_iterations;
  int DIM;
  int GRID_WIDTH;

  num_iterations = DEFAULT_ITERATIONS;
  if (argc == 2) {
    DIM = atoi(argv[1]);
    GRID_WIDTH = DIM * DIM;
  }
  int main_grid[GRID_WIDTH];
  memset(main_grid, 1, GRID_WIDTH*sizeof(int));

  int kernel[9];
  memset(kernel, 5, 9*sizeof(int));
  
  // Messaging variables
  MPI_Status stat;
  // TODO add other variables as necessary

  // MPI Setup
  if ( MPI_Init( &argc, &argv ) != MPI_SUCCESS )
  {
    printf ( "MPI_Init error\n" );
  }

  MPI_Comm_size ( MPI_COMM_WORLD, &num_procs ); // Set the num_procs
  MPI_Comm_rank ( MPI_COMM_WORLD, &ID );

  assert ( DIM % num_procs == 0 );

  // TODO Setup your environment as necessary
  int upper[DIM];
  int lower[DIM];
  
  int * pad_row_upper;
  int * pad_row_lower;
  
  int start = (DIM / num_procs) * ID;
  int end = (DIM / num_procs) - 1 + start;
  int nrows = end + 1 - start;
  int next = (ID + 1) % num_procs;
  int prev = ID != 0 ? ID - 1 : num_procs - 1;
  
  for ( iters = 0; iters < num_iterations; iters++ ) {
    // TODO: Add Code here or a function call to you MPI code
    if(ID != 0 && iters > 0) {
      MPI_Status status;
      MPI_Recv(&main_grid[0], DIM * DIM, MPI_INT, 0, 10, MPI_COMM_WORLD, &status);
    }
    memcpy(lower, &main_grid[DIM * end], sizeof(int) * DIM);
    pad_row_lower = malloc(sizeof(int) * DIM);
    
    memcpy(upper, &main_grid[DIM * start], sizeof(int) * DIM);
    pad_row_upper = malloc(sizeof(int) * DIM);
    
    if(num_procs > 1) {
      if(ID % 2 == 1) {
        MPI_Recv(pad_row_lower, DIM, MPI_INT, next, 1, MPI_COMM_WORLD, &stat);
        MPI_Recv(pad_row_upper, DIM, MPI_INT, prev, 1, MPI_COMM_WORLD, &stat);
      } else {
        MPI_Send(upper, DIM, MPI_INT, prev, 1, MPI_COMM_WORLD);
        MPI_Send(lower, DIM, MPI_INT, next, 1, MPI_COMM_WORLD);
      }  
      if(ID % 2 == 1) {
        MPI_Send(upper, DIM, MPI_INT, prev, 0, MPI_COMM_WORLD);
        MPI_Send(lower, DIM, MPI_INT, next, 0, MPI_COMM_WORLD);
      } else {
        MPI_Recv(pad_row_lower, DIM, MPI_INT, next, 0, MPI_COMM_WORLD, &stat);
        MPI_Recv(pad_row_upper, DIM, MPI_INT, prev, 0, MPI_COMM_WORLD, &stat);
      }
    } else {
      pad_row_lower = upper;
      pad_row_upper = lower;
    }
    int sub_grid[(2 * DIM) + (nrows * DIM)];
    if (ID == 0) {
      memset(pad_row_upper, 0, DIM*sizeof(int));
    }
    if (ID == (num_procs - 1)) {
      memset(pad_row_lower, 0, DIM*sizeof(int));
    }
    memcpy(sub_grid, pad_row_upper, sizeof(int) * DIM); 
    memcpy(&sub_grid[DIM], &main_grid[DIM * start], sizeof(int) * DIM * nrows);    
    memcpy(&sub_grid[DIM + (DIM * nrows)], pad_row_lower, sizeof(int) * DIM);
    int * changed_subgrid = check(sub_grid, nrows, DIM, kernel);

    if(ID != 0) {
      MPI_Send(changed_subgrid, nrows * DIM, MPI_INT, 0, 11, MPI_COMM_WORLD);
    } else {
      for(int i = 0; i < nrows * DIM; i++) {
        main_grid[i] = changed_subgrid[i];
      }
      for(int k = 1; k < num_procs; k++) {
        MPI_Status status;
        MPI_Recv(&main_grid[DIM * (DIM / num_procs) * k], nrows * DIM, MPI_INT, k, 11, MPI_COMM_WORLD, &status);
      }

      update_global(main_grid, nrows, num_procs, DIM);
    }
    
    // Output the updated grid state
    if ( ID == 0 ) {
      printf ( "\nIteration %d: final grid:\n", iters );
      for ( j = 0; j < GRID_WIDTH; j++ ) {
        if ( j % DIM == 0 ) {
          printf( "\n" );
        }
        printf ( "%d  ", main_grid[j] );
      }
      printf( "\n" );
    }
  }

  // TODO: Clean up memory
  if(num_procs >= 2) {
    free(pad_row_upper);
    free(pad_row_lower);
  }
  MPI_Finalize(); // finalize so I can exit
}
