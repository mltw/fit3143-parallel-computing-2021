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
// header files to get IP address of the system
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>

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
    int iteration; // iteration of the sensor nodes
    char timestamp[256]; // time logged

    // reporting node rank, moving average and coordinates
    int reporting_node_rank;
    float reporting_node_ma;
    int reporting_node_coord[2];

    // receiving nodes (ie neighbour nodes) rank, moving average and coordinates
    int recv_node_rank_arr[4];
    float recv_node_ma_arr[4];
    int recv_node_coord[4][2];
};

// global struct for POSIX threads to retrieve the node's rank and moving average for that iteration
struct arg_struct_thread {
    float node_mAvg; // this node's MA, updated as a new MA is generated in each iteration
    int end; // acts as a "lock", it'll change value once a termination message is received from base station
    int rank; // this node's rank 
    MPI_Comm world_comm; // the world communicator
    MPI_Comm comm; // the slaves/virtual topology communicator
    float* recv_node_ma_arr; // an array to store all neighbour nodes' MA
} *node_thread_args;


/* This is the slave; each slave/process simulates one tsunameter sensor node */
int node_io(MPI_Comm world_comm, MPI_Comm comm, int dims[], int threshold){
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

    // array to store the values generated to calculate moving average (MA)
    ma_arr = (float*)malloc(100 * sizeof(float));
    memset(ma_arr, 0, 100 * sizeof(float));

    // initialise the struct using an array in the heap
    node_thread_args = malloc(sizeof(struct arg_struct_thread) * 1);
    node_thread_args->end = 0;
    node_thread_args->rank = my_cart_rank;
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

    do{
        srand ( time(NULL)+my_cart_rank );
        startTime = MPI_Wtime();

        // reference to generate a random float between two floats: 
        // https://stackoverflow.com/a/13409005/16454185
        // Here, we generate a value within [5500, threshold+500]
        randNum = ((threshold+500 - 5500) * ((float)rand() / RAND_MAX)) + 5500;

        // store randNum into appropriate position in ma_arr
        if (counter>=100){
            // if ma_arr is full, we start replacing values from the start, 
            // ie replace index 0's randNum, then index 1's randNum etc. (FIFO)
            ma_arr[counter%100] = randNum;
        }
        else{
            ma_arr[counter] = randNum;
        }

        // calculate MA
        float sum = 0;
        for (i=0; i< (counter+1 > 100 ? 100 : counter +1 ); i++){
            sum += ma_arr[i];
        }
        mAvg = (float) sum / (counter+1 > 100 ? 100 : counter +1 );

        node_thread_args->node_mAvg = mAvg;
        
        pFile = fopen(pOutputFileName, "a");

        fprintf(pFile, "\nCounter: %d of node rank %d (world rank %d)\n", counter, my_rank, my_rank+1);
        fprintf(pFile, "Generated: %f\n", randNum);
        fprintf(pFile, "Moving average is: %f\n", mAvg);
        
        
        int arr[4] = {nbr_i_lo, nbr_i_hi, nbr_j_lo, nbr_j_hi};  
        char* arr_char[] = {"top neighbour", "bottom neighbour", "left neighbour", "right neighbour"};

        // if MA > threshold, request MA of neighbours
        int temp_counter = 0;
        // first iteration, we don't send, since other nodes may not have any value yet
        if ((mAvg > threshold) && (counter !=0)){

            for (i=0; i<4; i++){
                // only send to neighbours that exist
                if ( arr[i] >=0 ){
                    MPI_Send(&my_cart_rank, 1, MPI_INT, arr[i]+1, MSG_REQ_NEIGHBOUR_NODE, world_comm);   

                    float neighbour_ma = node_thread_args->recv_node_ma_arr[arr[i]];

                    if ((neighbour_ma >= (threshold-300)) && (neighbour_ma <= (threshold+300))){
                        fprintf(pFile, "value of %s stored in array is %f\n", 
                                arr_char[i], node_thread_args->recv_node_ma_arr[arr[i]]);
                        temp_counter+=1;
                    }


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

            base_station_args.reporting_node_rank = my_cart_rank; 
            base_station_args.reporting_node_ma = mAvg;
            base_station_args.reporting_node_coord[0] = coord[0];
            base_station_args.reporting_node_coord[1] = coord[1];

            for (i=0; i<4; i++){
                base_station_args.recv_node_rank_arr[i] = arr[i];
                base_station_args.recv_node_ma_arr[i] = node_thread_args->recv_node_ma_arr[arr[i]];
                base_station_args.recv_node_coord[i][0] = (int)floor(arr[i]/dims[1]);
                base_station_args.recv_node_coord[i][1] = arr[i]%dims[0];
            }

            // send a report to base station
            MPI_Send(&base_station_args, 8, Valuetype, 0, MSG_ALERT_BASE_STATION, world_comm);
            
        }

        endTime = MPI_Wtime();
        // if whole operation in that iteration is less than 10 seconds, delay it to 10 seconds before next iteration
        if((endTime - startTime) <=10){
            sleep(10 - (endTime-startTime));
        }
 
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
    // get the arguments 
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

        if (status.MPI_TAG == MSG_REQ_NEIGHBOUR_NODE){
            // send this node's MA back to the source which requested it    
            MPI_Send(&node_thread_args->node_mAvg, 1, MPI_INT, status.MPI_SOURCE, MSG_RES_NEIGHBOUR_NODE, MPI_COMM_WORLD);
        }

        else if (status.MPI_TAG == MSG_RES_NEIGHBOUR_NODE){
            recv_node_ma_arr[status.MPI_SOURCE-1] = recv;
        }

        else if (status.MPI_TAG == MSG_SHUTDOWN){
            sleep(10);
            node_thread_args->end = -1;
            break;
        }
        else{
            printf("thread received from source %d", status.MPI_SOURCE);
        }
    }

    pthread_exit(NULL);
}