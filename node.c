#include "header.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>

#define DISP 1
#define SHIFT_ROW 0
#define SHIFT_COL 1
#define MSG_SHUTDOWN 0


/* This is the slave; each slave/process simulates one tsunameter sensor node */
int node_io(MPI_Comm world_comm, MPI_Comm comm, int dims[], int threshold, int inputIterBaseStation){
    printf("in node\n");
    int ndims=2, size, my_rank, reorder, my_cart_rank, ierr, masterSize;
    float randNum, mAvg;
	MPI_Comm comm2D;
	int coord[ndims];
	int wrap_around[ndims];
    int nbr_i_lo, nbr_i_hi, nbr_j_lo, nbr_j_hi; // stores rank of top, bottom, left, right neighbour respectively
    float* ma_arr=NULL;

    MPI_Comm_size(world_comm, &masterSize); // size of the master communicator
  	MPI_Comm_size(comm, &size); // size of the slave communicator
	MPI_Comm_rank(comm, &my_rank);  // rank of the slave communicator

    MPI_Dims_create(size, ndims, dims);
    // printf("Slave Rank: %d. Comm Size: %d: Grid Dimension = [%d x %d] \n",my_rank,size,dims[0],dims[1]);

    // create cartesian mapping
	wrap_around[0] = 0;
	wrap_around[1] = 0; // periodic shift is false
	reorder = 0; 
	ierr = 0;
    // use 'comm' instead of 'world_comm' to only include the slaves communicators
	ierr = MPI_Cart_create(comm, ndims, dims, wrap_around, reorder, &comm2D);
	if(ierr != 0) 
        printf("ERROR[%d] creating CART\n",ierr);

    // find my coordinates in the cartesian communicator group 
	MPI_Cart_coords(comm2D, my_rank, ndims, coord); 
	// use my cartesian coordinates to find my rank in cartesian group
	MPI_Cart_rank(comm2D, coord, &my_cart_rank);

    // Returns the shifted source and destination ranks, given a shift direction and displacement of 1
	MPI_Cart_shift( comm2D, SHIFT_ROW, DISP, &nbr_i_lo, &nbr_i_hi );
	MPI_Cart_shift( comm2D, SHIFT_COL, DISP, &nbr_j_lo, &nbr_j_hi );

    printf("before do and my rank is: %d\n", my_rank);
    printf("before do and my cart rank is: %d\n", my_cart_rank);

    // array to store the values generated to calculate moving average (MA)
    ma_arr = (float*)malloc(inputIterBaseStation * sizeof(float));
    memset(ma_arr, 0, inputIterBaseStation * sizeof(float));

    // double *pX4Buff = NULL;
    // pX4Buff = (double*)malloc(fileElementCount * sizeof(double));
	// memset(pX4Buff, 0, fileElementCount * sizeof(double)); // Optional


    

    int counter = 0, i =0;
    double startTime, endTime;
    
    do{
        
        startTime = MPI_Wtime();

        // reference to generate a random float between two floats: https://stackoverflow.com/a/13409005/16454185
        // Here, we generate a value within [5000, threshold+500]
        randNum = ((threshold+500 - 5000) * ((float)rand() / RAND_MAX)) + 5000;
        ma_arr[counter] = randNum;

        // calculate MA
        float sum = 0;
        for (i=0; i < (counter+1); i++)
            sum += ma_arr[i];
        mAvg = (float) sum / (counter+1);
        // printf("%f\n", mAvg);

        // wait for all nodes to complete computing their MA, then only we check if MA > threshold
        MPI_Barrier(comm); //or 'comm' not sure

        if (mAvg > threshold){
            printf("%f, hi\n", mAvg);
        }


        endTime = MPI_Wtime();
        // if whole operation in that iteration is less than 10 seconds, delay it to 10 seconds before next iteration
        if((endTime - startTime) <=10){
            sleep(10 - (endTime-startTime));
        }

        counter++;
    }
    while (counter<inputIterBaseStation);



    // receive termination signal
    char buf[256]; // temporary
    MPI_Status status;
    MPI_Recv( buf, 256, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, world_comm, &status );
    if (status.MPI_TAG == MSG_SHUTDOWN){
        printf("node %d stop now\n",my_rank);
    }





    free(ma_arr);
    MPI_Comm_free( &comm2D );
	return 0;



}