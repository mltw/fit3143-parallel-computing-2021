#include "header.h" 
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

#define MSG_SHUTDOWN 0

struct sizeGrid
{
    int recvRows;
    int recvCols;
};

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
    
    MPI_Request send_request[256]; // give it a max size 
    MPI_Comm_size( world_comm, &size );
    nNodes = size-1;
    MPI_Status send_status[nNodes];
    
    struct sizeGrid val = { nrows, ncols};
    // printf("received no of rows is %d\n", val.recvRows);
    // printf("received no of col is %d\n", val.recvCols);
    
    // Altimeter
    pthread_t tid;
    pthread_create(&tid, 0, altimeter, &val); // Create the thread
    
    // run for a fixed number of iterations which is set during compiled time
    for (int i =1; i <= inputIterBaseStation;i++){
         printf("here\n");
         
         if (i == inputIterBaseStation){
            // send termination signal to each node 
             for (int j = 1; j <= nNodes; j++){
                MPI_Isend(buf, 0, MPI_CHAR, j, MSG_SHUTDOWN, world_comm, &send_request[j]);
             }
             // send termination siganl to altimeter 
             MPI_Isend(buf, 0, MPI_CHAR, 0, MSG_SHUTDOWN, world_comm, &send_request[0]);
         }
      
    }

    MPI_Waitall(nNodes, send_request, send_status);
    
    pthread_join(tid, NULL); // Wait for the thread to complete
    return 0;
}
struct coordinates {
    int x;
    int y;
};


// reference for getting current time: https://www.cplusplus.com/reference/ctime/localtime/
struct globalData {
    time_t rawtime;
    struct tm *timeinfo;
    struct coordinates randCoord;
    float randFloat;
};


struct globalData globalArr[5]; 

void* altimeter(void *pArg) // Common function prototype
{   
    struct sizeGrid *arg = (struct sizeGrid *) pArg;
    // printf("ALTIMETER recv row is %d, recv col is %d\n", arg->recvRows, arg->recvCols);

    char buf[256]; // temporary
    MPI_Status status;
    MPI_Request receive_request;
    double startTime, endTime,elapsed;
    int maxSize = 5, pointerHead = 0, pointerTail = 0,current=0;
    

    // printf("BEfore DE SIZE IS %d\n", (int) (sizeof(globalArr)/sizeof(globalArr[0])));
    for (int i = 0;i <=9; i++){
        startTime = MPI_Wtime();
        
        if (current <= maxSize-1){
            // still can insert
            processFunc(current, (arg->recvRows), (arg->recvCols));               // call helper func to fill the array
            current++;                                               
        }
        else if (current == maxSize){
            // once we reset, then even the next iter will update in the correct place
            // e.g.: first time array full, then reset current to 0, now current will be 1 (but array is still full)
            // so we next will remove the 2nd earliest data (which is at index 1)
            printf("ARRAY FULL\n");
            current = 0;                                                         // reset, start from index 0 again 
            processFunc(current, (arg->recvRows), (arg->recvCols));
            current++;
            
        }

        // Receive termination signal
        MPI_Irecv(buf, 256, MPI_CHAR, MPI_ANY_SOURCE, MSG_SHUTDOWN, MPI_COMM_WORLD, &receive_request);
        if (status.MPI_TAG == MSG_SHUTDOWN){
            printf("Altimeter received termination signal, it will be stopped now.\n");
            //break;
        }

       endTime = MPI_Wtime();
       elapsed = endTime - startTime;

        
        // printf("start time is %.2f\n", startTime);
        // printf("end time is %.2f\n", endTime);
        printf("Time elapsed for iteration %d is %.2f\n", i, endTime - startTime);
         // if whole operation in that iteration is less than 1 seconds, delay it to 1 second before next iteration
       if( elapsed <= 1){
            // printf("delayedddddd at iteration %d for %.1f seconds\n",i,1-elapsed);
            sleep((int)1 - elapsed);
            //printf("SLEPT FOR iteration %d\n", i);
       }
        printf("--------------------------------------\n");
        
    }
    return 0;
}

void processFunc(int counter, int recvRows, int recvCols){
    int threshold = 6000, maxLimit = 9000;
    
    printf("------ Altimeter Iteration %d ---------\n", counter);
    // Get time generated
    time(&globalArr[counter].rawtime);
    globalArr[counter].timeinfo = localtime (&globalArr[counter].rawtime);
    printf ("Current local time and date: %s", asctime(globalArr[counter].timeinfo));   
    
    // ssGenerate random coordinates 
    globalArr[counter].randCoord.x = rand() % recvRows;
    globalArr[counter].randCoord.y = rand() % recvCols;
    printf("Random coordinates is (%d,%d)\n", globalArr[counter].randCoord.x,globalArr[counter].randCoord.y);
    
     // Generate random float between a range 
     globalArr[counter].randFloat = ((maxLimit -threshold)*  ((float)rand() / RAND_MAX)) + threshold;
     printf("Random float of iteration %d is %.3f\n",counter, globalArr[counter].randFloat );
        
}
