#include "header.h" 
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <pthread.h>
#include <time.h>

#define MSG_SHUTDOWN 0

void* altimeter(void *pArg);
int base_station_io(MPI_Comm world_comm, MPI_Comm comm, int inputIterBaseStation, int nrows, int ncols);

/* This is the base station, which is also the root rank; it acts as the master */
int base_station_io(MPI_Comm world_comm, MPI_Comm comm, int inputIterBaseStation, int nrows, int ncols){
    /* TODO:
    - run for a fixed number of iterations which is set during compiled time (DONE)
    - when startup, user is able to specify no of iterations for base station to run  (DONE)
    - once completed all iterations, send termination signals to node & altimeter to shutdown (DONE)
    - allow user to enter a sentinel value AT RUNTIME to properly shutdown node, atlimeter & base station
    
    
    - EVERY ITERATION:
        - Listen to incoming comm./report from ANY node (if any)
        ** thread(POSIX/OpenMP) will act as a background worker to listen to the message from the sensor nodes
        
        - if received:
            - compare received report & seacrh entire array to determine if there is a match
                - macthing coordinfates report (MUST BE EXACT MATCH)
                - matching bet. timestamp x (+/- x sec)
                - matching bet. sea column heights y (+/- y m)
            - based on timestamp:
                - proper comparisons betw. data from sensor node & data from altimeter to validate an alert
            
    - when reporting an alert: log
         - details of reporting node & adjacent node
         - comparison with altimeter
         - node details (coordinates)
      
      - before program terminates, generate summary
         - no of alerts (true & false)
         - avg comm time
         - time taken for base station 
      
        
        * can check the latest record in the shared global array if there are multiple records with the same coords
        * loop the global array in a reverse manner when comparing the records to the received reports
    */
    
    char buf[256]; // temporary
    char terminationSignal;
    int size, nNodes;
    MPI_Status status;
    MPI_Comm_size( world_comm, &size );
    nNodes = size-1;
    
    // Altimeter
    pthread_t tid;
    pthread_create(&tid, 0, altimeter, &nNodes); // Create the thread
    


    // run for a fixed number of iterations which is set during compiled time
    for (int i =1; i <= inputIterBaseStation;i++){
         printf("here\n");
         
         if (i == inputIterBaseStation){
            // send termination signal to each node 
             for (int j = 1; j <= nNodes; j++){
                MPI_Send(buf, 0, MPI_CHAR, j, MSG_SHUTDOWN, world_comm);
             }
             // send termination siganl to altimeter 
             MPI_Send(buf, 0, MPI_CHAR, 0, MSG_SHUTDOWN, world_comm);
         }
       
    }
    
    pthread_join(tid, NULL); // Wait for the thread to complete
    return 0;
}

//struct globalData {
    //timespec timeExecution;
    //float randFloat;
    // coords
    
//};

// globalData globalArr[50]; // check max size needs to be how big 
void* altimeter(void *pArg) // Common function prototype
{       
    // receive termination signal
    char buf[256]; // temporary
    MPI_Status status;
    MPI_Recv( buf, 256, MPI_CHAR, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status );
    if (status.MPI_TAG == MSG_SHUTDOWN){
        printf("altimeter stop now\n");
    }
    
    // TODO: check iteration need to run how many or we can define 
    for (int i=0; i <= 10;i++){
        // generate random float 
       // globalArr[i].timeExecution = clock_gettime(CLOCK_MONOTONIC, &timeExecution); 
        //globalArr[i].randFloat = 
        break;
    }
    return 0;
}
