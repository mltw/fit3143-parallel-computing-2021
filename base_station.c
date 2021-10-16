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

    // char reporting_node_ip_add[256];
    // char recv_node_ip_add[4][256];
};

struct coordinates {
    int x;
    int y;
};

// reference for getting current time: https://www.cplusplus.com/reference/ctime/localtime/
struct globalData {
    char timestamp[256];
    struct coordinates randCoord;
    float randFloat;
};
struct globalData globalArr[5]; 

int sharedGlobalSignal = -1;
int thresholdGlobal;
int nrowsGlobal, ncolsGlobal;
pthread_mutex_t mutex;
pthread_mutex_t mutex2;

/* This is the base station, which is also the root rank; it acts as the master */
int base_station_io(MPI_Comm world_comm, MPI_Comm comm, int inputIterBaseStation, int threshold, int nrows, int ncols){
    /* TODO:      
      - before program terminates, generate summary
         - no of alerts (true & false)
         - avg comm time
         - time taken for base station 
      
    */
    nrowsGlobal = nrows;
    ncolsGlobal = ncols;
    thresholdGlobal = threshold;
    
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
    //pthread_t tid2;
    //thread_init = pthread_create(&tid2,0, userInput, &terminationSignal);
    //if (thread_init != 0)
        //printf("Error creating awaiting user input thread in base station\n");

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
                MPI_Send(buf, 0, MPI_CHAR, j, MSG_SHUTDOWN, world_comm);
                
             }
             
             
             pthread_mutex_lock(&mutex);
             printf("Changing the shared resource now.\n");
             printf("GLOBAL SIGNAL PREVIOUSLY IS %d\n", sharedGlobalSignal);
             sharedGlobalSignal = 1;
             printf("GLOBAL SIGNAL NOW IS %d\n", sharedGlobalSignal);
             pthread_mutex_unlock(&mutex);
             printf("MUTEX UNLOCKED\n");
             
                 
         }
         else{
            sleep(10);
         }
      
    }

    pthread_join(tid, NULL);                                    // Wait for the thread to complete
    //pthread_join(tid2, &resSignal);
    pthread_join(tid3, NULL); 

    //if ((bool)resSignal == true){
        //printf(" I RECEIVED true SIGNAL IN MAIN FUNCTION\n"); 
        //return 0;
        //}
    //else 
        //printf("I DID NOT RECV SIGNAL YET\n");

     
     // last pthread should return received node info
     int length = sizeof(globalArr)/sizeof(globalArr[0]);    
     //printf("length of global Arr is %d\n", length);
     for (int k= length-1;k>=0;k--){
        //printf("arr at index %d\n", k);
        printf("time at index %d is %s\n",k,globalArr[k].timestamp);
        printf("random coordinates at index %d is (%d,%d)\n",k ,globalArr[k].randCoord.x,globalArr[k].randCoord.y);
        printf("random float at index  %d is %.3f\n",k, globalArr[k].randFloat );
 
     }
    
    return 0;
}


void* altimeter(void *pArg) 
{   
    struct sizeGrid *arg = (struct sizeGrid *) pArg;
    // printf("ALTIMETER recv row is %d, recv col is %d\n", arg->recvRows, arg->recvCols);

    char buf[256]; 
    MPI_Status status;
    MPI_Request receive_request;
    double startTime, endTime,elapsed;
    int maxSize = 5, current=0,i=0;
    


    // continue update the global array until we receive termination signal
    while (1){
        startTime = MPI_Wtime();
        
        pthread_mutex_lock(&mutex);
        if (sharedGlobalSignal == 1){
            printf("NOW I WILL STOP ALTIMETER\n");
            pthread_mutex_unlock(&mutex);
            printf("MUTEX UNLOCKED IN ALTIMETER\n");
            break;
        }
        else{
            printf("GLOBAL SIGNAL RECEIVED SIGNAL MUTEX IN ALTIMETER IS STILL%d\n", sharedGlobalSignal);
            }
            pthread_mutex_unlock(&mutex);
            
        
  
       
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

    pthread_mutex_lock(&mutex2);
    printf("INSERTING INTO ARRAY, UPDATING\n");
    int maxLimit = 9000;

    srand(time(NULL));
    
    printf("------ Altimeter Iteration %d ---------\n", counter);
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
     printf("DONE INSERTION, NOW MUTEX UNLOCK FOR SHARED GLOBAL ARR\n");
        
}

// code inspiration: https://w3.cs.jmu.edu/kirkpams/OpenCSF/Books/csf/html/Extended6Input.html
void* userInput(void *pArg){
    bool currentSignal;
    bool* p = (bool*)pArg;
	currentSignal = *p;
	
	char buffer[MAX_LENGTH + 1];
    memset (buffer, 0, MAX_LENGTH + 1);
    
    
      
	
	/* Read a line of input from STDIN */
	//while (1)
    while (fgets (buffer, MAX_LENGTH, stdin) != NULL)
    {
        
           
      /* Try to convert input to integer -1. All other values are wrong. */
        //if (fgets (buffer, MAX_LENGTH, stdin) != NULL){
            long guess = strtol (buffer, NULL, 10);
            if (guess == -1)
            {
          /* Successfully read a -1 from input; exit with true */
            printf ("User wish to terminate!\n");
            currentSignal = true;
            pthread_exit ((void*)currentSignal);
            }
            //}
       
    }

    return 0;
}

// base station POSIX thread to send/receive msg to/from sensor nodes
void* base_station_recv(void *arguments){
    MPI_Status status;
    int recv, i;

    FILE *pFile;

    // create a custom MPI Datatype for the struct that will be sent from nodes, and received here in base station 
    struct arg_struct_base_station base_station_args;
    MPI_Datatype Valuetype;
    //  char reporting_node_ip_add[256];
    // char recv_node_ip_add[4][256];
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
    // MPI_Get_address(&base_station_args.reporting_node_ip_add, &disp[8]);
    // MPI_Get_address(&base_station_args.recv_node_ip_add, &disp[9]);

    for (i=7; i>=1; i--){
        disp[i] = disp[i] - disp[i-1];
    }
    disp[0] = 0;

    MPI_Type_create_struct(8, blocklen, disp, datatype, &Valuetype);
    MPI_Type_commit(&Valuetype);
    double startTime, endTime,elapsed;

    while (1){
        startTime = MPI_Wtime();
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

            fputs( base_station_args.timestamp, stdout ); // for testing

            
            int true_alert = 0, matching_nodes = 0, has_matched_rand_coord = 0;
            int length = sizeof(globalArr)/sizeof(globalArr[0]);  
            pFile = fopen("base_station_log.txt", "a");
            
            // loop through the global array to see if there are matching coordinates
            pthread_mutex_lock(&mutex2);
            printf("NOW COMPARING IN FOR LOOP SECOND MUTEX\n");
            for (int k= length-1; k>=0; k--){
                if ((globalArr[k].randCoord.x == base_station_args.reporting_node_coord[0]) 
                    && (globalArr[k].randCoord.y == base_station_args.reporting_node_coord[1])){
        
                        if (base_station_args.reporting_node_ma >= globalArr[k].randFloat-300 
                            && base_station_args.reporting_node_ma <= globalArr[k].randFloat+300){
                                true_alert = 1;
                                // fprintf(pFile, "haha reporting ma: %f\n", base_station_args.reporting_node_ma);
                                // fprintf(pFile, "haha globalArr de ma: %f\n", globalArr[k].randFloat);
                                // fprintf(pFile, "haha globalArr de coord: (%d,%d)\n",globalArr[k].randCoord.x, globalArr[k].randCoord.y);
                                break;
                            }
                    }
            }
            pthread_mutex_unlock(&mutex2);
            printf("I FINISH COMPARISION FOR MUTEX 2 NOW UNLOCK IT\n");
        
        endTime = MPI_Wtime();
        elapsed = endTime - startTime;
        if( elapsed <= 5){
            // printf("delayedddddd at iteration %d for %.5f seconds\n",i,5-elapsed);
            sleep(5 - elapsed);
            //printf("SLEPT FOR iteration %d\n", i);
        }


            // now, start writing into the file
            fprintf(pFile, "--------------------------------------------------------------\n");
            fprintf(pFile, "Iteration: %d\n", base_station_args.iteration);
            
            char *node_timestamp = base_station_args.timestamp;
            time_t t;
            t = time(NULL);
            char *logged_timestamp = ctime(&t);
            // printf("%s -\n", logged_timestamp);
            // time_t t1;
            // t1 = time(NULL);
            

            // char C[27];
            // ctime_r(&t1, &C);
            
            // time_t rawtime_log;
            // struct tm * timeinfo_log;
            // time(&rawtime_log);
            // timeinfo_log = localtime (&rawtime_log);
            // time still got bug 

            // time and alert type
            fprintf(pFile, "Logged Time: \t\t\t%s", logged_timestamp);
            fprintf(pFile, "Alert reported time: \t%s\n", node_timestamp);
            fprintf(pFile, "Alert type: %s\n", true_alert==1? "Match (True)" : "Mismatch (False)");

            // reporting node
            fprintf(pFile, "Reporting Node\tCoord.\tHeight (m)\tIPv4\n");
            fprintf(pFile,"%d\t\t\t\t(%d,%d)\t%f\t???\n", 
                    base_station_args.reporting_node_rank,
                    (int)floor(base_station_args.reporting_node_rank/ncolsGlobal),
                    base_station_args.reporting_node_rank%nrowsGlobal, // is this %?, will check agn 
                    base_station_args.reporting_node_ma);  

            // adjacent nodes
            fprintf(pFile, "\nAdjacent Nodes\tCoord.\tHeight (m)\tIPv4\n");
            for (i=0; i<4; i++){
                if (base_station_args.recv_node_rank_arr[i] >=0){
                    fprintf(pFile, "%d\t\t\t\t(%d,%d)\t%f\t???\n", 
                    base_station_args.recv_node_rank_arr[i],
                    base_station_args.recv_node_coord[i][0],
                    base_station_args.recv_node_coord[i][1],
                    base_station_args.recv_node_ma_arr[i]);

                    matching_nodes++;
                }
            }

            // satellite altimeter reporting time and height
            for (int k= length-1; k>=0; k--){
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
            fprintf(pFile, "\nCommunication time (seconds): ?? \n");
            fprintf(pFile, "Total Messages send between reporting node and base station: 1\n");
            fprintf(pFile, "Number of adjacent matches to reporting node: %d\n", matching_nodes);
            fprintf(pFile, "Max. tolerance range between nodes readings (m): 300\n");
            fprintf(pFile, "Max. tolerance range between satellite altimeter and reporting node readings (m): 300\n\n");

        
            fclose(pFile);
        }
    }

    printf("bye from base station thread\n");
    MPI_Type_free(&Valuetype);
    pthread_exit(NULL);
    
}
