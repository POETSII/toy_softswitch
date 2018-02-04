#ifndef pmpi_hpp
#define pmpi_hpp

#include "pmpi_context.hpp"

extern uint32_t pmpi_world_rank_to_address[]; // Maps rank to global id. There is an entry for all ranks, including on other chips
extern int pmpi_world_size;

extern uint32_t pmpi_local_address_to_world_rank[];      // Maps local id (i.e. intra-chip) to rank in world
extern pmpi_context_t pmpi_contexts[];

pmpi_context_t *pmpi_get_context()
{
    return pmpi_contexts+tinsel_myId();
}

int MPI_Init(int */*argc*/, char ***/*argv*/)
{
    return pmpi_get_context()->MPI_Init(
        pmpi_world_size,
        pmpi_local_address_to_world_rank[tinsel_myId()],
        pmpi_world_rank_to_address
    );
}

int MPI_Finalize()
{
    return pmpi_get_context()->MPI_Finalize();
}

int MPI_Comm_rank(MPI_Comm comm, int *rank)
{
    return pmpi_get_context()->MPI_Comm_rank(comm,rank);
}

int MPI_Comm_size( MPI_Comm comm, int *size ) 
{
    return pmpi_get_context()->MPI_Comm_size(comm,size);
}

int MPI_Send(
    const void *buf,
    int count,
    MPI_Datatype datatype, // Always byte
    int dest,
    int tag,
    MPI_Comm comm
){
    return pmpi_get_context()->MPI_Send(buf, count, datatype, dest, tag, comm);
}

int MPI_Recv(
    void *buf,
    int count,
    MPI_Datatype datatype,
    int source,
    int tag,
    MPI_Comm comm,
    MPI_Status *status
){
    return pmpi_get_context()->MPI_Recv(buf, count, datatype, source, tag, comm, status);
}

#endif
