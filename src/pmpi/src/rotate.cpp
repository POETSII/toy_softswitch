#include "mpi.h"

#include <cstdio>

const unsigned PMPI_N=4;

unsigned softswitch_pthread_count=PMPI_N;
PThreadContext softswitch_pthread_contexts[PMPI_N];

pmpi_context_t pmpi_contexts[PMPI_N];
int pmpi_world_size=PMPI_N;
uint32_t pmpi_local_address_to_world_rank[8]={0,1,2,3,4,5,6,7};
uint32_t pmpi_world_rank_to_address[8]={0,1,2,3,4,5,6,7};

extern "C" void softswitch_main() {
    MPI_Init(NULL, NULL);

    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    
    int size;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    
    
    
    int left=rank-1;
    if(left<0) left+=size;
    
    int right=rank+1;
    if(right>=size) right-=size;
    
    uint32_t msg=rank;
    
    printf("(%d) : left=%d, right=%d\n", rank, left, right);
    
    MPI_Send(
        &msg,
        sizeof(msg),
        MPI_BYTE,
        right, // dest
        rank*2, // tag
        MPI_COMM_WORLD
    );
    
    printf("(%d) : left=%d, right=%d\n", rank, left, right);
    
    MPI_Status status;
    MPI_Recv(
        &msg,
        sizeof(msg),
        MPI_BYTE,
        MPI_ANY_SOURCE,
        MPI_ANY_TAG,
        MPI_COMM_WORLD,
        &status
    );
    
    printf("(%d) : left=%d, right=%d\n", rank, left, right);
    
    printf("(%d) Received: left=%d, size=%d, source=%d, tag=%d\n", rank, left, size, status.MPI_SOURCE, status.MPI_TAG);
    
    assert(status.MPI_SOURCE==left);
    assert(status.MPI_TAG==left*2);
    
    MPI_Finalize();
}
