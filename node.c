#include "header.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>

#define DISP 1
#define SHIFT_ROW 0
#define SHIFT_COL 1


/* This is the slave; each slave/process simulates one tsunameter sensor node */
int node_io(MPI_Comm world_comm, MPI_Comm comm, int dims[]){

    int ndims=2, size, my_rank, reorder, my_cart_rank, ierr, masterSize;
	MPI_Comm comm2D;
	int coord[ndims];
	int wrap_around[ndims];
    int nbr_i_lo, nbr_i_hi, nbr_j_lo, nbr_j_hi; // stores rank of top, bottom, left, right neighbour respectively

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
	MPI_Cart_coords(comm2D, my_rank, ndims, coord); // coordinates are returned into the 'coord' array
	// use my cartesian coordinates to find my rank in cartesian group
	MPI_Cart_rank(comm2D, coord, &my_cart_rank);

    // Returns the shifted source and destination ranks, given a shift direction and amount
	// Here, DISP=1 --> look at the immediate neighbours only (your direct top/down/left/right neighbours only)
	MPI_Cart_shift( comm2D, SHIFT_ROW, DISP, &nbr_i_lo, &nbr_i_hi );
	MPI_Cart_shift( comm2D, SHIFT_COL, DISP, &nbr_j_lo, &nbr_j_hi );

    





    MPI_Comm_free( &comm2D );
	return 0;



}