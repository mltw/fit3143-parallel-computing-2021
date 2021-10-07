/* 
This is just a header file to store function declarations.
Reference: https://riptutorial.com/c/example/3250/calling-a-function-from-another-c-file
*/

#include <mpi.h>

#ifndef HEADER_H_
#define HEADER_H_

void* altimeter(void *pArg);
int base_station_io(MPI_Comm world_comm, MPI_Comm comm,int inputIterBaseStation, int nrows, int ncols);
int node_io(MPI_Comm world_comm, MPI_Comm comm, int dims[]);

#endif /* HEADER_H_ */