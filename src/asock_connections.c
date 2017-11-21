#include "asock_connections.h"



asock_connections_t* asock_connections_create(size_t capacity)
{
    return asock_vector_create(sizeof(asock_t*), capacity);
}

void asock_connections_destroy(asock_connections_t *connections)
{
    return asock_vector_destroy(connections);
}

size_t asock_connections_size(const asock_connections_t *connections)
{
    return asock_vector_size(connections);
}




int asock_connections_push(asock_connections_t *connections,
                            const asock_t *asock)
{
    return asock_vector_push(connections, &asock);
}

int asock_connections_erase(asock_connections_t *connections, size_t index)
{
    return asock_vector_erase(connections, index);
}

asock_t*  asock_connections_at(asock_connections_t *connections, size_t index)
{
    return *(asock_t**)asock_vector_at(connections, index);
}
