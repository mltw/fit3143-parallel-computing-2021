/* 
FIT3143 S2 2021 Assignment 2 
Topic: Tsunami Detection in a Distributed Wireless Sensor Network (WSN)
Group: MA_LAB-04-Team-05
Authors: Tan Ke Xin, Marcus Lim


This is just a header file to store function declarations.
Reference: https://riptutorial.com/c/example/3250/calling-a-function-from-another-c-file
*/

#include <mpi.h>

#ifndef HEADER_H_
#define HEADER_H_

void* node_recv(void *arguments);
void* base_station_recv(void *arguments);
void* altimeter(void *pArg);
void processFunc(int counter, int recvRows, int recvCols);
int base_station_io(MPI_Comm world_comm, MPI_Comm comm, int inputIterBaseStation, int threshold, int nrows, int ncols);
int node_io(MPI_Comm world_comm, MPI_Comm comm, int dims[], int threshold);

#endif /* HEADER_H_ */