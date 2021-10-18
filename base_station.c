/*
FIT3143 S2 2021 Assignment 2 
Topic: Tsunami Detection in a Distributed Wireless Sensor Network (WSN)
Group: MA_LAB-04-Team-05
Authors: Tan Ke Xin, Marcus Lim
*/

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

// struct to store no of rows & no of cols received from main function
struct sizeGrid
{
    int recvRows;
    int recvCols;
};

// struct to store arguments for reporting alerts 
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

// struct to store randomly generated coordinates 
struct coordinates {
    int x;
    int y;
};

// struct to store neccessary data for altimeter 
struct globalData {
    char timestamp[256];
    struct coordinates randCoord;
    float randFloat;
};

// global shared array 
struct globalData globalArr[5]; 

// initialize global variable 
int sharedGlobalSignal = -1;
int thresholdGlobal;
int nrowsGlobal, ncolsGlobal;
pthread_mutex_t mutex;
pthread_mutex_t mutex2;

/* This is the base station, which is also the root rank; it acts as the master */
int base_station_io(MPI_Comm world_comm, MPI_Comm comm, int inputIterBaseStation, int threshold, int nrows, int ncols){
    // initialize neccessary variables 
    nrowsGlobal = nrows;
    ncolsGlobal = ncols;
    thresholdGlobal = threshold;
    
    char buf[256]; 
    int size, nNodes, thread_init, i, j;
    
    MPI_Comm_size( world_comm, &size );
    nNodes = size-1;

    
    struct sizeGrid val = { nrows, ncols};
    
    // Thread for altimeter
    pthread_t tid;
    thread_init = pthread_create(&tid, 0, altimeter, &val);                     // Create the thread
    if (thread_init != 0)
        printf("Error creating altimeter thread in base station\n");

    // Thread to receive msg to/from sensor nodes
    pthread_t tid3;
    thread_init = pthread_create(&tid3, NULL, base_station_recv, NULL);
    if (thread_init != 0)
        printf("Error creating base station recv thread in base station\n");

    // Run for a fixed number of iterations which is set during compiled time
    for (i =0; i <= inputIterBaseStation;i++){
         
         if (i == inputIterBaseStation){
             // send termination signal to nodes & altimeter to shutdown 
             for (j = 1; j <= nNodes; j++){
                MPI_Send(buf, 0, MPI_CHAR, j, MSG_SHUTDOWN, world_comm);
             }
             
             // update global shared var, so in altimeter will know value being updated to 1, time to terminate
             // pthread_mutex_lock : prevent race condition  
             pthread_mutex_lock(&mutex);
             sharedGlobalSignal = 1;
             pthread_mutex_unlock(&mutex);  
         }
         else{
            sleep(10);
         }
      
    }

    // Wait for the thread to complete
    pthread_join(tid, NULL);                                    
    pthread_join(tid3, NULL); 

    return 0;
}


/*This is a thread spawned by the base station. */
void* altimeter(void *pArg) 
{   
    // initialize neccessary variables 
    struct sizeGrid *arg = (struct sizeGrid *) pArg;
    double startTime, endTime,elapsed;
    int maxSize = 5, current=0,i=0;
    
    // Continue update the global array until we receive termination signal
    while (1){
        startTime = MPI_Wtime();
        
        pthread_mutex_lock(&mutex);
        if (sharedGlobalSignal == 1){
            // signal to stop altimeter received, break for loop & exit 
            pthread_mutex_unlock(&mutex);
            break;
        }
        pthread_mutex_unlock(&mutex);
            
        
        if (current <= maxSize-1){
            // Still have slots to insert
            processFunc(current, (arg->recvRows), (arg->recvCols));               // Call helper func to fill the array
            current++;    
            i++;                                           
        }
        else if (current == maxSize){
            // once we reset, then even the next iter will update in the correct place
            // e.g.: first time array full, then reset current to 0, now current will be 1 (but array is still full)
            // so we next will remove the 2nd earliest data (which is at index 1)
            current = 0;                                                         // reset, start from index 0 again 
            processFunc(current, (arg->recvRows), (arg->recvCols));
            current++;
            i++;
            
        }

       endTime = MPI_Wtime();
       elapsed = endTime - startTime;
       
         // if whole operation in that iteration is less than 5 seconds, delay it to 5 second before next iteration
       if( elapsed <= 5){
            sleep(5 - elapsed);
       }
        printf("--------------------------------------\n");
        
    }

    pthread_exit (NULL);
    return 0;
}

/* This is a helper function to aid in processing neccessary values for altimeter, will be called each iteration*/
void processFunc(int counter, int recvRows, int recvCols){

    // ensure only 1 process can access & modify shared global resource at a time 
    pthread_mutex_lock(&mutex2);
    int maxLimit = 9000;

    srand(time(NULL));
    
    printf("------ Altimeter Iteration %d ---------\n", counter);

    // reference for getting current time: https://www.cplusplus.com/reference/ctime/localtime/
    // Get time generated
    time_t rawtime;
    struct tm * timeinfo;
    time(&rawtime);
    timeinfo = localtime (&rawtime);
    sprintf(globalArr[counter].timestamp, "%s", (asctime(timeinfo)));
    printf("Current date and time is %s\n",globalArr[counter].timestamp);
   
    // Generate random coordinates 
    globalArr[counter].randCoord.x = rand() % recvRows;
    globalArr[counter].randCoord.y = rand() % recvCols;
    printf("Random coordinates is (%d,%d)\n", globalArr[counter].randCoord.x,globalArr[counter].randCoord.y);
    
     // Generate random float between a range 
     globalArr[counter].randFloat = ((maxLimit -thresholdGlobal)*  ((float)rand() / RAND_MAX)) + thresholdGlobal;
     printf("Random float of iteration %d is %.3f\n",counter, globalArr[counter].randFloat );
     pthread_mutex_unlock(&mutex2);
        
}

/* A base station POSIX thread to receive msg from sensor nodes */
void* base_station_recv(void *arguments){
    // initialize neccessary variables 
    MPI_Status status;
    int i,k;
    int total_alert = 0, total_true_alert = 0;
    FILE *pFile;

    // create a custom MPI Datatype for the struct that will be sent from nodes, and received here in base station 
    struct arg_struct_base_station base_station_args;
    MPI_Datatype Valuetype;
    MPI_Datatype datatype[8] = { MPI_INT, MPI_CHAR, MPI_INT, MPI_FLOAT, MPI_INT, MPI_INT, MPI_FLOAT, MPI_INT};
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

        // receive message from ANY_SOURCE & ANY_TAG, perform appropriate actions based on tag received 
        MPI_Recv(&base_station_args, 8, Valuetype, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        if (status.MPI_TAG == MSG_SHUTODWN_BASE_STATION_THREAD){
            printf(" base station recvied alert from node %d\n", status.MPI_SOURCE-1);
            break;
        }
        else if (status.MPI_TAG == MSG_ALERT_BASE_STATION){
            printf("base sation successfully got the scruct from %d\n", status.MPI_SOURCE);
            printf("values.iteration is %d, x_coord is %d, y_coord is %d, neighbour node rank is %d\n", 
                    base_station_args.iteration, base_station_args.reporting_node_coord[0], 
                    base_station_args.reporting_node_coord[1], base_station_args.recv_node_rank_arr[1]);

            
            int true_alert = 0, matching_nodes = 0, has_matched_rand_coord = 0;
            
            int length = sizeof(globalArr)/sizeof(globalArr[0]);  
            pFile = fopen("base_station_log.txt", "a");
            
            // loop through the global array(in reverse manner) to see if there are matching coordinates
            pthread_mutex_lock(&mutex2);
            for (k= length-1; k>=0; k--){
                if ((globalArr[k].randCoord.x == base_station_args.reporting_node_coord[0]) 
                    && (globalArr[k].randCoord.y == base_station_args.reporting_node_coord[1])){
        
                        if (base_station_args.reporting_node_ma >= globalArr[k].randFloat-300 
                            && base_station_args.reporting_node_ma <= globalArr[k].randFloat+300){
                                true_alert = 1;
                                total_true_alert ++;
                                break;
                            }
                    }
            }
            pthread_mutex_unlock(&mutex2);

            total_alert++;

            // now, start writing into the file
            fprintf(pFile, "--------------------------------------------------------------\n");
            fprintf(pFile, "Iteration: %d\n", base_station_args.iteration);
            
            char *node_timestamp = base_station_args.timestamp;
            time_t t;
            t = time(NULL);
            char *logged_timestamp = ctime(&t);

            // time and alert type
            fprintf(pFile, "Logged Time: \t\t\t%s", logged_timestamp);
            fprintf(pFile, "Alert reported time: \t%s\n", node_timestamp);
            fprintf(pFile, "Alert type: %s\n", true_alert==1? "Match (True)" : "Mismatch (False)");

            // reporting node
            fprintf(pFile, "Reporting Node\tCoord.\tHeight (m)\n");
            fprintf(pFile,"%d\t\t\t\t(%d,%d)\t%f\n", 
                    base_station_args.reporting_node_rank,
                    (int)floor(base_station_args.reporting_node_rank/ncolsGlobal),
                    base_station_args.reporting_node_rank%nrowsGlobal,  
                    base_station_args.reporting_node_ma);  

            // adjacent nodes
            fprintf(pFile, "\nAdjacent Nodes\tCoord.\tHeight (m)\n");
            for (i=0; i<4; i++){
                if (base_station_args.recv_node_rank_arr[i] >=0){
                    fprintf(pFile, "%d\t\t\t\t(%d,%d)\t%f\n", 
                    base_station_args.recv_node_rank_arr[i],
                    base_station_args.recv_node_coord[i][0],
                    base_station_args.recv_node_coord[i][1],
                    base_station_args.recv_node_ma_arr[i]);

                    matching_nodes++;
                }
            }

            // satellite altimeter reporting time and height
            for (k= length-1; k>=0; k--){
                if ((globalArr[k].randCoord.x == base_station_args.reporting_node_coord[0]) 
                    && (globalArr[k].randCoord.y == base_station_args.reporting_node_coord[1])){
                        fprintf(pFile, "\nSatellite altimeter reporting time: %s", globalArr[k].timestamp);
                        fprintf(pFile, "Satellite altimeter reporting height (m): %f\n", globalArr[k].randFloat);
                        has_matched_rand_coord = 1;
                        break;
                    }
            }
            // if no matching coordinates from altimeter, just put a "-"
            if (has_matched_rand_coord != 1){
                fprintf(pFile, "\nSatellite altimeter reporting time: -\n");
                fprintf(pFile, "Satellite altimeter reporting height (m): -\n");
            }

            // remaining 
            fprintf(pFile, "Total Messages sent between reporting node and base station: 1\n");
            fprintf(pFile, "Number of adjacent matches to reporting node: %d\n", matching_nodes);
            fprintf(pFile, "Max. tolerance range between nodes readings (m): 300\n");
            fprintf(pFile, "Max. tolerance range between satellite altimeter and reporting node readings (m): 300\n\n");

        
            fclose(pFile);
        }
    }

    pFile = fopen("base_station_log.txt", "a");
    fprintf(pFile, "Total reports: %d\n", total_alert);
    fprintf(pFile, "True reports: %d\n", total_true_alert);
    fprintf(pFile, "False reports: %d\n", total_alert - total_true_alert);
    fclose(pFile);


    MPI_Type_free(&Valuetype);
    pthread_exit(NULL);
    
}
