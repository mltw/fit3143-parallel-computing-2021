#include "header.h" 
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>


/* This is the base station, which is also the root rank; it acts as the master */
int base_station_io(MPI_Comm world_comm, MPI_Comm comm){
    /* TODO:
    - run for a fixed number of iterations which is set during compiled time 
    - when startup, user is able to specify no of iterations for base station to run 
    - once completed all iterations, send termination signals to node & altimeter to shutdown
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
    return 0;
}