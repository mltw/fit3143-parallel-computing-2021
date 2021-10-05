/*
FIT3143 S2 2021 Assignment 2 
Topic: Tsunami Detection in a Distributed Wireless Sensor Network (WSN)
Group: MA_LAB-04-Team-05
Authors: Tan Ke Xin, Marcus Lim
*/

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#include "header.h"

#define DISP 1
#define SHIFT_ROW 0
#define SHIFT_COL 1


int main(int argc, char *argv[]){
    
    int ndims=2, size, dims[ndims];
	int nrows, ncols;
    int rank;
    MPI_Comm new_comm;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // process command line arguments
	if (argc == 3) {
		nrows = atoi (argv[1]);
		ncols = atoi (argv[2]);
		dims[0] = nrows; // number of rows 
		dims[1] = ncols; // number of columns
		if( ((nrows*ncols)+1) != size) {
			if( rank ==0) printf("ERROR: (nrows*ncols)+1)=%d * %d = %d != %d\n", 
                                    nrows, ncols, nrows*ncols+1, size);
			MPI_Finalize(); 
			return 0;
		}
	} 
    else {
		nrows=ncols=(int)sqrt(size-1);
		dims[0]=dims[1]=0;
	}
    
    // root rank (ie rank = 0) is the master and would be in one color;
    // the others are slaves, and thus would be in another color
    MPI_Comm_split( MPI_COMM_WORLD,rank == 0, 0, &new_comm);
    if (rank == 0) 
	    base_station_io( MPI_COMM_WORLD, new_comm );
    else
        // pass in the dims array to the slaves for creating of cartesian topology
	    node_io( MPI_COMM_WORLD, new_comm, dims);
    
    MPI_Finalize();
    return 0;
}