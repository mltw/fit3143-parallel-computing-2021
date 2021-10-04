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

#define DISP = 1
#define SHIFT_ROW = 0
#define SHIFT_COL = 1

// function prototype
int master_io(MPI_Comm world_comm, MPI_Comm comm);
int slave_io(MPI_Comm world_comm, MPI_Comm comm, int[] dims);


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
			if( my_rank ==0) printf("ERROR: (nrows*ncols)+1)=%d * %d = %d != %d\n", 
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
	    master_io( MPI_COMM_WORLD, new_comm );
    else
        // pass in the dims array to the slaves for creating of cartesian topology
	    slave_io( MPI_COMM_WORLD, new_comm, dims);
    
    MPI_Finalize();
    return 0;
}

/* This is the master, which is also the root rank; it acts as the base station */
int master_io(MPI_Comm world_comm, MPI_Comm comm){

}

/* This is the slave; each slave/process simulates one tsunameter sensor node */
int slave_io(MPI_Comm world_comm, MPI_Comm comm, int[] dims){

    int ndims=2, size, my_rank, reorder, my_cart_rank, ierr, masterSize;
	MPI_Comm comm2D;
	int coord[ndims];
	int wrap_around[ndims];
    int nbr_i_lo, nbr_i_hi, nbr_j_lo, nbr_j_hi; // stores rank of top, bottom, left, right neighbour respectively

    MPI_Comm_size(world_comm, &masterSize); // size of the master communicator
  	MPI_Comm_size(comm, &size); // size of the slave communicator
	MPI_Comm_rank(comm, &my_rank);  // rank of the slave communicator

    MPI_Dims_create(size, ndims, dims);

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