#include "header.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>
#include <memory.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

#define DISP 1
#define SHIFT_ROW 0
#define SHIFT_COL 1
#define MSG_SHUTDOWN 88
#define MSG_SHUTODWN_BASE_STATION_THREAD 2
#define MSG_ALERT_BASE_STATION 3
#define MSG_REQ_NEIGHBOUR_NODE 4
#define MSG_RES_NEIGHBOUR_NODE 5

// struct to store necessary information for this node, and to be sent to the base station
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


// global struct for POSIX threads to retrieve the node's rank and moving average for that iteration
struct arg_struct_thread {
    // float node_mAvg;
    // int node_rank;
    // MPI_Comm node_comm;
    // float* recv_node_mAvg;
    float node_mAvg;
    int end;
    int rank;
    int updated_neighbour_ma;
    MPI_Comm world_comm;
    MPI_Comm comm;
    float* recv_node_ma_arr;
} *node_thread_args;


/* This is the slave; each slave/process simulates one tsunameter sensor node */
int node_io(MPI_Comm world_comm, MPI_Comm comm, int dims[], int threshold, int inputIterBaseStation){
    printf("in node\n");
    int ndims=2, size, my_rank, reorder, my_cart_rank, ierr, masterSize, i;
    float randNum, mAvg;
	MPI_Comm comm2D;
	int coord[ndims];
	int wrap_around[ndims];
    int nbr_i_lo, nbr_i_hi, nbr_j_lo, nbr_j_hi; // stores rank of top, bottom, left, right neighbour respectively
    float* ma_arr=NULL;

    MPI_Comm_size(world_comm, &masterSize); // size of the master communicator
  	MPI_Comm_size(comm, &size); // size of the slave communicator
	MPI_Comm_rank(comm, &my_rank);  // rank of the slave communicator

    char* pOutputFileName = (char*) malloc(20 * sizeof(char));
    FILE *pFile;
    snprintf(pOutputFileName, 20, "node_%d.txt", my_rank);

    MPI_Dims_create(size, ndims, dims);
    printf("Slave Rank: %d. Comm Size: %d: Grid Dimension = [%d x %d] \n",my_rank,size,dims[0],dims[1]);

    // create cartesian mapping
	wrap_around[0] = 0;
	wrap_around[1] = 0; // periodic shift is false
	reorder = 0; 
	ierr = 0;
    // use 'comm' instead of 'world_comm' to only include the slaves communicators
	ierr = MPI_Cart_create(comm, ndims, dims, wrap_around, reorder, &comm2D);
	if(ierr != 0) 
        printf("ERROR[%d] creating CART\n",ierr);

    // find my coordinates in the cartesian communicator group 
	MPI_Cart_coords(comm2D, my_rank, ndims, coord); 
	// use my cartesian coordinates to find my rank in cartesian group
	MPI_Cart_rank(comm2D, coord, &my_cart_rank);

    // Returns the shifted source and destination ranks, given a shift direction and displacement of 1
	MPI_Cart_shift( comm2D, SHIFT_ROW, DISP, &nbr_i_lo, &nbr_i_hi );
	MPI_Cart_shift( comm2D, SHIFT_COL, DISP, &nbr_j_lo, &nbr_j_hi );

    // create a custom MPI Datatype for that struct to be sent over to base station
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

    // array to store the values generated to calculate moving average (MA)
    // ####################!!!!!!!!!!!!!!!!!!!!****************************%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    // TODO: gotta edit this cuz nodes wont know the total num of iteration 
    ma_arr = (float*)malloc(100 * sizeof(float));
    memset(ma_arr, 0, 100 * sizeof(float));

    //  -------------------- test pthread -------------------------------------

    // initialise the struct using an array in the heap
    node_thread_args = malloc(sizeof(struct arg_struct_thread) * 1);
    node_thread_args->end = 0;
    node_thread_args->rank = my_cart_rank;
    node_thread_args->updated_neighbour_ma = 0;
    node_thread_args->comm = comm;
    node_thread_args->world_comm = world_comm;
    node_thread_args->recv_node_ma_arr = malloc(sizeof(float) * size);
    for(i = 0; i < size; i++) node_thread_args->recv_node_ma_arr[i] = -1;

    // each node would have a POSIX thread to wait for and receive requests for their MA
    pthread_t tid;
    // create the thread
    int thread_init = pthread_create(&tid, NULL, node_recv, node_thread_args);
    if (thread_init != 0)
        printf("Error creating thread in node %d", my_rank);

    // -------------------------------------------------------------------------
    int counter = 0;
    double startTime, endTime;
    printf("master size is %d", masterSize);
    do{
        printf("nbr_i_lo is %d of rank %d\n", nbr_i_lo, my_cart_rank);
        printf("nbr_i_hi is %d of rank %d\n", nbr_i_hi, my_cart_rank);
        printf("nbr_j_lo is %d of rank %d\n", nbr_j_lo, my_cart_rank);
        printf("nbr_j_hi is %d of rank %d\n", nbr_j_hi, my_cart_rank);
        
        srand ( time(NULL)+my_cart_rank );
        startTime = MPI_Wtime();

        // reference to generate a random float between two floats: 
        // https://stackoverflow.com/a/13409005/16454185
        // Here, we generate a value within [5000, threshold+500]
        randNum = ((threshold+500 - 5000) * ((float)rand() / RAND_MAX)) + 5000;
        ma_arr[counter] = randNum;

        // calculate MA
        float sum = 0;
        for (i=0; i < (counter+1); i++)
            sum += ma_arr[i];
        mAvg = (float) sum / (counter+1);
        // printf("%f\n", mAvg);

        // wait for all nodes to complete computing their MA, then only we check if MA > threshold
        // MPI_Barrier(comm); 

        // if (mAvg > threshold){
        //     printf("%f, hi\n", mAvg);
        // }

        printf("Rank %d generated %f\n", my_cart_rank, mAvg);
        node_thread_args->node_mAvg = mAvg;
        
        pFile = fopen(pOutputFileName, "a");

        fprintf(pFile, "\nCounter: %d of node rank %d (world rank %d)\n", counter, my_rank, my_rank+1);
        fprintf(pFile, "Generated: %f\n", randNum);
        fprintf(pFile, "Moving average is: %f\n", mAvg);
        

        // ------------------ test send to neighbour node ----------------------
        // MPI_Request send_request[4];
        // if (my_cart_rank == 0){
        
        int arr[4] = {nbr_i_lo, nbr_i_hi, nbr_j_lo, nbr_j_hi};  
        char* arr_char[] = {"top neighbour", "bottom neighbour", "left neighbour", "right neighbour"};

        // if MA > threshold, request MA of neighbours
        int temp_counter = 0;
        if (mAvg > threshold){
            for (i=0; i<4; i++){
                // only send to neighbours that exist
                if ( arr[i] >=0 ){
                    printf("in %s of rank %d and sending to %d\n", arr_char[i], my_rank, nbr_i_hi+1);
                    MPI_Send(&my_cart_rank, 1, MPI_INT, arr[i]+1, MSG_REQ_NEIGHBOUR_NODE, world_comm);

                    printf("in %s of rank %d and send done\n", arr_char[i], my_rank);
                    printf("stored in array value of nbr_i_hi is %f\n", node_thread_args->recv_node_ma_arr[nbr_i_hi]);

                    fprintf(pFile, "value of %s stored in array is %f\n", 
                                    arr_char[i], node_thread_args->recv_node_ma_arr[arr[i]]);
                    // don't calculate neighbours' MA until received the latest updated one
                    while (node_thread_args->updated_neighbour_ma==1){
                        float neighbour_ma = node_thread_args->recv_node_ma_arr[arr[i]];
                        if ( neighbour_ma >= (threshold-100) && neighbour_ma <= (threshold+100)){
                            fprintf(pFile, "value of %s stored in array is %f\n", 
                                    arr_char[i], node_thread_args->recv_node_ma_arr[arr[i]]);
                            temp_counter+=1;
                        }
                        node_thread_args->updated_neighbour_ma = 0;
                    }
                    node_thread_args->updated_neighbour_ma = 0;
                }
            }
        }
        fprintf(pFile, "counter where neighbours' MA within threshold range: %d\n", temp_counter);

        fclose(pFile);
     
        // if >=2 neighbours' MA match this node's MA, send a report to the base station
        if (temp_counter >=2 ){
            // initialise the struct values
            base_station_args.iteration = counter; 
            time_t rawtime;
            struct tm * timeinfo;
            time(&rawtime);
            timeinfo = localtime (&rawtime);
            sprintf(base_station_args.timestamp, "%s", (asctime(timeinfo)));
            // printf ("Current local time and date in node is: %s\n", (asctime(timeinfo)));  
            // printf ("Current local time and date in node but saved in struct is: %s\n", 
            //         base_station_args.timestamp);  

            base_station_args.reporting_node_rank = my_cart_rank; 
            base_station_args.reporting_node_ma = mAvg;
            base_station_args.reporting_node_coord[0] = coord[0];
            base_station_args.reporting_node_coord[1] = coord[1];
            for (i=0; i<4; i++){
                base_station_args.recv_node_rank_arr[i] = arr[i];
                base_station_args.recv_node_ma_arr[i] = node_thread_args->recv_node_ma_arr[arr[i]];
                base_station_args.recv_node_coord[i][0] = arr[i]%dims[0];
                base_station_args.recv_node_coord[i][1] = arr[i]%dims[1];
            }

            // send a report to base station
            MPI_Send(&base_station_args, 8, Valuetype, 0, MSG_ALERT_BASE_STATION, world_comm);
        }


        endTime = MPI_Wtime();
        // if whole operation in that iteration is less than 10 seconds, delay it to 10 seconds before next iteration
        if((endTime - startTime) <=10){
            sleep(10 - (endTime-startTime));
        }

        // node_thread_args->end = counter;
        // printf("%d", node_thread_args->end);
        // MPI_Barrier(comm); 
        printf("------- END OF COUNTER %d --------\n", counter);
        counter++;
    }
    while (node_thread_args->end >=0 );
    
    MPI_Barrier(comm);

    // use node 0 to send a msg to base station to terminate the base station's thread
    if (my_cart_rank == 0)
        MPI_Send(&my_cart_rank, 1, MPI_INT, 0, MSG_SHUTODWN_BASE_STATION_THREAD, world_comm);

    pthread_join(tid, NULL);

    free(node_thread_args);
    free(node_thread_args->recv_node_ma_arr);
    free(ma_arr);

    MPI_Type_free(&Valuetype);

    MPI_Comm_free( &comm2D );
	return 0;
}


void* node_recv(void *arguments){
    struct arg_struct_thread *node_thread_args = arguments;
    int end = node_thread_args->end;
    float mAvg = node_thread_args->node_mAvg;
    float* recv_node_ma_arr = node_thread_args->recv_node_ma_arr;
    int rank = node_thread_args->rank;
    MPI_Comm comm = node_thread_args->comm;
    MPI_Comm world_comm = node_thread_args->world_comm;

    MPI_Status status;
    float recv;

    while (node_thread_args->end >= 0){
        MPI_Recv(&recv, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        // if (status.MPI_TAG == MSG_BASE_TO_NODE_SHUTDOWN_THREAD){
        //     printf("received termination msg from base_station to thread %d", rank);
        //     break;
        // }
        if (status.MPI_TAG == MSG_REQ_NEIGHBOUR_NODE){
            printf("im node %d (world_rank %d), and i received request msg from node %d (world_rank %d)\n", 
                    rank, rank+1, status.MPI_SOURCE-1, status.MPI_SOURCE);
            // send this node's MA back to the source which requested it    
            MPI_Send(&node_thread_args->node_mAvg, 1, MPI_INT, status.MPI_SOURCE, MSG_RES_NEIGHBOUR_NODE, MPI_COMM_WORLD);
            // printf("received request msg from node %d\n", status.MPI_SOURCE);
        }
        else if (status.MPI_TAG == MSG_RES_NEIGHBOUR_NODE){
            printf("im node %d (world_rank %d), and i received response msg of %f from node %d (world_rank %d)\n", 
                    rank, rank+1, recv, status.MPI_SOURCE-1, status.MPI_SOURCE);
            recv_node_ma_arr[status.MPI_SOURCE-1] = recv;
            printf("put into array: %f\n", recv_node_ma_arr[status.MPI_SOURCE-1]);
            node_thread_args->updated_neighbour_ma = 1;
        }
        else if (status.MPI_TAG == MSG_SHUTDOWN){
            printf("Node %d received termination signal in thread, will stop now\n", rank);
            node_thread_args->end = -1;
            break;
        }
        else{
            printf("thread received from source %d", status.MPI_SOURCE);
        }
    }

    printf("bye from thread of rank %d\n", rank);
    pthread_exit(NULL);
}