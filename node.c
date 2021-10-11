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
#define MSG_SHUTDOWN 0
#define MSG_NODE_TO_BASE_SHUTDOWN_THREAD 2
#define MSG_BASE_TO_NODE_SHUTDOWN_THREAD 3
#define MSG_REQ_NEIGHBOUR_NODE 4

// global struct for POSIX threads to retrieve the node's rank and moving average for that iteration
struct arg_struct_thread {
    // float node_mAvg;
    // int node_rank;
    // MPI_Comm node_comm;
    // float* recv_node_mAvg;
    int end;
    int rank;
    MPI_Comm world_comm;
    MPI_Comm comm;
} *node_thread_args;

struct arg_struct_base_station {
    int reporting_node_rank;
    int reporting_node_coord[2];
    float reporting_node_ma;
    
    int recv_node_rank_arr[4];
    int recv_node_coord[4][2];
    float recv_ma_arr[4];

} *node_base_station_args;

/* This is the slave; each slave/process simulates one tsunameter sensor node */
int node_io(MPI_Comm world_comm, MPI_Comm comm, int dims[], int threshold, int inputIterBaseStation){
    printf("in node\n");
    int ndims=2, size, my_rank, reorder, my_cart_rank, ierr, masterSize;
    float randNum, mAvg;
	MPI_Comm comm2D;
	int coord[ndims];
	int wrap_around[ndims];
    int nbr_i_lo, nbr_i_hi, nbr_j_lo, nbr_j_hi; // stores rank of top, bottom, left, right neighbour respectively
    float* ma_arr=NULL;

    MPI_Comm_size(world_comm, &masterSize); // size of the master communicator
  	MPI_Comm_size(comm, &size); // size of the slave communicator
	MPI_Comm_rank(comm, &my_rank);  // rank of the slave communicator

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

    // printf("before do and my rank is: %d\n", my_rank);
    // printf("before do and my cart rank is: %d\n", my_cart_rank);

    // array to store the values generated to calculate moving average (MA)
    ma_arr = (float*)malloc(inputIterBaseStation * sizeof(float));
    memset(ma_arr, 0, inputIterBaseStation * sizeof(float));

    // double *pX4Buff = NULL;
    // pX4Buff = (double*)malloc(fileElementCount * sizeof(double));
	// memset(pX4Buff, 0, fileElementCount * sizeof(double)); // Optional

    //  -------------------- test pthread -------------------------------------

    // initialise the struct using an array in the heap
    node_thread_args = malloc(sizeof(struct arg_struct_thread) * 1);
    node_thread_args->end = 0;
    node_thread_args->rank = my_cart_rank;
    node_thread_args->comm = comm;
    node_thread_args->world_comm = world_comm;

    // each node would have a POSIX thread to wait for and receive requests for their MA
    pthread_t tid;
    // create the thread
    int thread_init = pthread_create(&tid, NULL, node_recv, node_thread_args);
    if (thread_init != 0)
        printf("Error creating thread in node %d", my_rank);

    // -------------------------------------------------------------------------
    int counter = 0, i =0;
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


        // ------------------ test send to neighbour node ----------------------
        // if (my_cart_rank == 0){
        //     if (nbr_i_hi >= 0){
        //         printf("in nbr_i_hi of rank 0 and sending to %d\n", nbr_i_hi+1);
        //         MPI_Send(&my_cart_rank, 1, MPI_INT, nbr_i_hi+1, MSG_REQ_NEIGHBOUR_NODE, world_comm);

        //         // MPI_Isend(&my_cart_rank, 1, MPI_INT, nbr_i_hi, MSG_REQ, comm2D, &send_request[1]);
        //         // MPI_Wait(&send_request[1], MPI_STATUS_IGNORE);
                
        //         printf("in nbr_i_hi of rank 0 and send done\n"); 
                
        // }
        // }
        // ---------------------------------------------------------------------




        endTime = MPI_Wtime();
        // if whole operation in that iteration is less than 10 seconds, delay it to 10 seconds before next iteration
        if((endTime - startTime) <=10){
            sleep(10 - (endTime-startTime));
        }

        node_thread_args->end = counter;
        printf("%d", node_thread_args->end);
        // MPI_Barrier(comm); 
        printf("------- END OF COUNTER %d --------\n", counter);
        counter++;
    }
    while (counter<inputIterBaseStation);

    // MPI_Isend 
    

    // cant do this cuz ifthe other rank/node de thread terminate dy, no one's gonna receive
    // MPI_Send(&my_cart_rank, 1, MPI_INT, 
    //         my_cart_rank == masterSize-2 ? 0 : my_cart_rank+1, 
    //         888, comm2D);

    MPI_Barrier(comm);


    // ------------- nodes send a msg to base_station to help to terminate the nodes' threads --------------
    MPI_Request send_request[4];
    printf("gonna send my rank is %d\n", my_cart_rank);
    // MPI_Isend(&my_cart_rank, 1, MPI_INT,  my_cart_rank == masterSize-2 ? 0 : my_cart_rank+1,
    //             888, comm2D, &send_request[1]);
    // MPI_Wait(&send_request[1], MPI_STATUS_IGNORE);

    //send a msg to base station to send back here a msg to terminate the threads
    MPI_Send(&my_cart_rank, 1, MPI_INT, 0, MSG_NODE_TO_BASE_SHUTDOWN_THREAD, world_comm);

    pthread_join(tid, NULL);
    printf("thread joined back in node %d\n", my_cart_rank);
    // ----------------------------------------------------------------------------------------------------

    // MPI_Barrier(comm);

    // receive termination signal
    char buf[256]; // temporary
    MPI_Status recv_status[my_rank];
    MPI_Request receive_request[256];

    MPI_Irecv(buf, 256, MPI_CHAR, MPI_ANY_SOURCE, MSG_SHUTDOWN, world_comm, &receive_request[my_rank]);
    MPI_Wait(&receive_request[my_rank], MSG_SHUTDOWN);
    printf("Node %d received termination signal, will stop now\n", my_rank);
    
    // check 
    // if (recv_status[my_rank].MPI_TAG == MSG_SHUTDOWN){
    //        printf("Node %d received termination signal, will stop now\n", my_rank);
    //    }



    free(node_thread_args);
    free(ma_arr);
    MPI_Comm_free( &comm2D );
	return 0;



}


void* node_recv(void *arguments){
    struct arg_struct_thread *node_thread_args = arguments;
    // int end = node_thread_args->end;
    int rank = node_thread_args->rank;
    MPI_Comm comm = node_thread_args->comm;
    MPI_Comm world_comm = node_thread_args->world_comm;

    MPI_Status status;
    int recv;
    // while (node_thread_args->end<=2) {
    //     // printf("node mAvg: %f\n", node_mAvg);
    //     printf("node rank: %d\n", rank);
    // }
    while (1){
        MPI_Recv(&recv, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, world_comm, &status);
        if (status.MPI_TAG == MSG_BASE_TO_NODE_SHUTDOWN_THREAD){
            printf("received termination msg from base_station to thread %d", rank);
            break;
        }
        else if (status.MPI_TAG == MSG_REQ_NEIGHBOUR_NODE){
            printf("received request msg from node %d", status.MPI_SOURCE);


        }
        else{
            printf("thread received from source %d", status.MPI_SOURCE);
        }
    }

    printf("bye from thread of rank %d\n", rank);
    pthread_exit(NULL);
}