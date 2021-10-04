/* 
This is just a header file to store function declarations.
Reference: https://riptutorial.com/c/example/3250/calling-a-function-from-another-c-file
*/

#include <mpi.h>

#ifndef HEADER_H_
#define HEADER_H_

int master_io(MPI_Comm world_comm, MPI_Comm comm);
int slave_io(MPI_Comm world_comm, MPI_Comm comm, int dims[]);

#endif /* HEADER_H_ */
