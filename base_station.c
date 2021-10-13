#include "header.h" 
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include<stdbool.h>  

#define MSG_SHUTDOWN 88
#define MAX_LENGTH 40
#define MSG_SHUTODWN_BASE_STATION_THREAD 2
#define MSG_ALERT_BASE_STATION 3

struct sizeGrid
{
    int recvRows;
    int recvCols;
};

struct arg_struct_base_station {
    int iteration;
    char timestamp[256];

    int reporting_node_rank;
    float reporting_node_ma;
    int reporting_node_coord[2];

    int recv_node_rank_arr[4];
    float recv_node_ma_arr[4];
    int recv_node_coord[4][2];
};


/* This is the base station, which is also the root rank; it acts as the master */
int base_station_io(MPI_Comm world_comm, MPI_Comm comm, int inputIterBaseStation, int nrows, int ncols){
    /* TODO:
    
    - EVERY ITERATION:        
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
    
    char buf[256]; 
    int size, nNodes, recv, thread_init;
    bool terminationSignal = false;
    
    MPI_Request send_request[256]; 
    MPI_Comm_size( world_comm, &size );
    nNodes = size-1;
    MPI_Status send_status[nNodes];
    MPI_Status status;
    MPI_Request receive_request;
    void *resSignal;
    
    struct sizeGrid val = { nrows, ncols};
    // printf("received no of rows is %d\n", val.recvRows);
    // printf("received no of col is %d\n", val.recvCols);
    
    // thread for altimeter
    pthread_t tid;
    thread_init = pthread_create(&tid, 0, altimeter, &val); // Create the thread
    if (thread_init != 0)
        printf("Error creating altimeter thread in base station\n");
    
    // thread to concurrently wait for input from user
    pthread_t tid2;
    thread_init = pthread_create(&tid2,0, userInput, &terminationSignal);
    if (thread_init != 0)
        printf("Error creating awaiting user input thread in base station\n");

    // thread to send/receive msg to/from sensor nodes
    pthread_t tid3;
    thread_init = pthread_create(&tid3, NULL, base_station_recv, NULL);
    if (thread_init != 0)
        printf("Error creating base station recv thread in base station\n");

    // run for a fixed number of iterations which is set during compiled time
    for (int i =0; i <= inputIterBaseStation;i++){
         
         if (i == inputIterBaseStation){
             // send termination signal to nodes & altimeter to shutdown gracefully
             for (int j = 1; j <= nNodes; j++){
                MPI_Isend(buf, 0, MPI_CHAR, j, MSG_SHUTDOWN, world_comm, &send_request[j]);
             }
             // send termination signal to altimeter 
             int terminate_altimeter = 1;
             printf("aites base station sending MSG_SHUTDOWN to altimeter at iteration %d\n", i);
             MPI_Isend(&terminate_altimeter, 1, MPI_INT, 0, MSG_SHUTDOWN, world_comm, &send_request[0]);

             MPI_Waitall(size, send_request, send_status);
             printf("ALL SENT SUCCESSFULLYYYYY FROM BASE STATION\n");
         }
         else{
            sleep(10);
         }
      
    }

    pthread_join(tid, NULL);                                    // Wait for the thread to complete
    pthread_join(tid2, &resSignal);
    pthread_join(tid3, NULL); 

    if ((bool)resSignal == true){
        printf(" I RECEIVED true SIGNAL IN MAIN FUNCTION\n"); 
        return 0;
        }
    else 
        printf("I DID NOT RECV SIGNAL YET\n");
    
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

void* altimeter(void *pArg) 
{   
    struct sizeGrid *arg = (struct sizeGrid *) pArg;
    // printf("ALTIMETER recv row is %d, recv col is %d\n", arg->recvRows, arg->recvCols);

    char buf[256]; 
    MPI_Status status;
    MPI_Request receive_request;
    double startTime, endTime,elapsed;
    int maxSize = 5, current=0, recv=-1,i=0,flag=-1;
    int ready;
    
    //MPI_Irecv(&recv, 1, MPI_INT, 0, MSG_SHUTDOWN, MPI_COMM_WORLD, &receive_request);
    //MPI_Irecv(&recv, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &receive_request);

    // continue update the global array until we receive termination signal
    while (1){
        startTime = MPI_Wtime();
        
        if (flag !=0){
            MPI_Irecv(&recv, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &receive_request);
            flag = 0;
        }     
        
        // Receive termination signal
        MPI_Test(&receive_request, &flag, &status);
        if (flag != 0){
            printf("I FINALLY RECEIVE TERMINATION SIGNAL\n");
            flag = -1;
            break;
           }
        else
            printf("STILL WAITING IN ITERATION %d\n", i-1);
     
        if (current <= maxSize-1){
            // still can insert
            printf("in if and i is: %d\n",i);
            processFunc(current, (arg->recvRows), (arg->recvCols));               // call helper func to fill the array
            current++;    
            i++;                                           
        }
        else if (current == maxSize){
            // once we reset, then even the next iter will update in the correct place
            // e.g.: first time array full, then reset current to 0, now current will be 1 (but array is still full)
            // so we next will remove the 2nd earliest data (which is at index 1)
            printf("ARRAY FULL\n");
            current = 0;                                                         // reset, start from index 0 again 
            processFunc(current, (arg->recvRows), (arg->recvCols));
            current++;
            i++;
            
        }

       endTime = MPI_Wtime();
       elapsed = endTime - startTime;
       
        // printf("start time is %.2f\n", startTime);
        // printf("end time is %.2f\n", endTime);
        printf("Time elapsed for iteration %d is %.5f\n", i-1, endTime - startTime);
         // if whole operation in that iteration is less than 5 seconds, delay it to 5 second before next iteration
       if( elapsed <= 5){
            // printf("delayedddddd at iteration %d for %.5f seconds\n",i,5-elapsed);
            sleep(5 - elapsed);
            //printf("SLEPT FOR iteration %d\n", i);
       }
        printf("--------------------------------------\n");
        
    }

    printf("bye from altimeter thread\n");
    pthread_exit (NULL);
    return 0;
}

void processFunc(int counter, int recvRows, int recvCols){
    int threshold = 6000, maxLimit = 9000;

    srand(time(NULL));
    
    printf("------ Altimeter Iteration %d ---------\n", counter);
    // Get time generated
    time(&globalArr[counter].rawtime);
    globalArr[counter].timeinfo = localtime (&globalArr[counter].rawtime);
    printf ("Current local time and date: %s", asctime(globalArr[counter].timeinfo));   
    
    // Generate random coordinates 
    globalArr[counter].randCoord.x = rand() % recvRows;
    globalArr[counter].randCoord.y = rand() % recvCols;
    printf("Random coordinates is (%d,%d)\n", globalArr[counter].randCoord.x,globalArr[counter].randCoord.y);
    
     // Generate random float between a range 
     globalArr[counter].randFloat = ((maxLimit -threshold)*  ((float)rand() / RAND_MAX)) + threshold;
     printf("Random float of iteration %d is %.3f\n",counter, globalArr[counter].randFloat );
        
}

// code inspiration: https://w3.cs.jmu.edu/kirkpams/OpenCSF/Books/csf/html/Extended6Input.html
void* userInput(void *pArg){
    bool currentSignal;
    bool* p = (bool*)pArg;
	currentSignal = *p;
	
	char buffer[MAX_LENGTH + 1];
    memset (buffer, 0, MAX_LENGTH + 1);
	
	/* Read a line of input from STDIN */
  while (fgets (buffer, MAX_LENGTH, stdin) != NULL)
    {
      /* Try to convert input to integer -1. All other values are wrong. */
      long guess = strtol (buffer, NULL, 10);
      if (guess == -1)
        {
          /* Successfully read a -1 from input; exit with true */
          printf ("User wish to terminate!\n");
          currentSignal = true;
          pthread_exit ((void*)currentSignal);
        }
    }

    return 0;
}

// base station POSIX thread to send/receive msg to/from sensor nodes
void* base_station_recv(void *arguments){
    MPI_Status status;
    int recv, i;

    // create a custom MPI Datatype for the struct that will be sent from nodes, and received here in base station 
    struct arg_struct_base_station base_station_args;
    MPI_Datatype Valuetype;
    MPI_Datatype datatype[8] = { MPI_INT, MPI_CHAR, MPI_INT, MPI_FLOAT, MPI_INT, MPI_INT, MPI_FLOAT, MPI_INT };
    int blocklen[8] = {1, 256, 1, 1, 2, 4, 4, 8};
    MPI_Aint disp[8];
    MPI_Get_address(&base_station_args.iteration, &disp[0]);
    MPI_Get_address(&base_station_args.timestamp, &disp[1]);
    MPI_Get_address(&base_station_args.reporting_node_rank, &disp[2]);
    MPI_Get_address(&base_station_args.reporting_node_ma, &disp[3]);
    MPI_Get_address(&base_station_args.reporting_node_coord, &disp[4]);
    MPI_Get_address(&base_station_args.recv_node_rank_arr, &disp[5]);
    MPI_Get_address(&base_station_args.recv_node_ma_arr, &disp[6]);
    MPI_Get_address(&base_station_args.recv_node_coord, &disp[7]);

    for (i=7; i>=1; i--){
        disp[i] = disp[i] - disp[i-1];
    }
    disp[0] = 0;
    
    MPI_Type_create_struct(8, blocklen, disp, datatype, &Valuetype);
    MPI_Type_commit(&Valuetype);

    while (1){
        MPI_Recv(&base_station_args, 8, Valuetype, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        if (status.MPI_TAG == MSG_SHUTODWN_BASE_STATION_THREAD){
            printf(" base station recvied alert from node %d\n", status.MPI_SOURCE-1);
            break;
        }
        else if (status.MPI_TAG == MSG_ALERT_BASE_STATION){
            printf("base sation successfully got the scruct\n");
            printf("values.iteration is %d, x_coord is %d, y_coord is %d, neighbour node rank is %d\n", 
                    base_station_args.iteration, base_station_args.reporting_node_coord[0], 
                    base_station_args.reporting_node_coord[1], base_station_args.recv_node_rank_arr[1]);

            fputs( base_station_args.timestamp, stdout ); // for testing
        }
    }

    printf("bye from base station thread\n");
    MPI_Type_free(&Valuetype);
    pthread_exit(NULL);
    
}
