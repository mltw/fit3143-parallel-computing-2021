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
			if( my_rank ==0) printf("ERROR: (nrows*ncols)+1)=%d * %d = %d != %d\n", nrows, ncols, nrows*ncols+1,size);
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

    int size;

    MPI_Comm_size(world_comm, &masterSize); // size of the master communicator
  	MPI_Comm_size(comm, &size); // size of the slave communicator
	MPI_Comm_rank(comm, &my_rank);  // rank of the slave communicator

    MPI_Dims_create(size, 2, dims);


}