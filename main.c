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
    int* buf;
    int inputIterBaseStation, threshold;
    MPI_Comm new_comm;
    MPI_Status status;
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
			if( rank ==0) printf("ERROR: (nrows*ncols)+1)= (%d * %d)+1 = %d != %d\n", 
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
    if (rank == 0) {   

        // when startup, user is able to specify no. of iterations for base station to run
        printf("Please enter the number of iterations you wish the base station will run. \n");
        printf("Number of iterations that base station runs:\n");
        scanf("%d", &inputIterBaseStation);

        // then, ask user for input on a sea water column height threshold value
        printf("Please enter a sea water column height threshold value >= 6000:\n");
        scanf("%d", &threshold);

        // after getting user input, send them to the slaves to proceed with node_io
        int i = 1;
        for (i=1; i < size; i++){
            MPI_Send(&threshold, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        }

	    base_station_io( MPI_COMM_WORLD, new_comm, inputIterBaseStation, threshold, nrows, ncols );
	}
    else {
        // slaves only proceed with node_io after receiving the inputs from master 
        MPI_Recv(&threshold, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status );
    
	    node_io( MPI_COMM_WORLD, new_comm, dims, threshold);
    }
    
    
    MPI_Finalize();
    return 0;
}