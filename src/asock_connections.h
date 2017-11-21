#ifndef ASOCK_CONNECTIONS_H
#define ASOCK_CONNECTIONS_H

#include "asock_vector.h"
#include "asock_impl.h"


typedef asock_vector_t  asock_connections_t;

asock_connections_t* asock_connections_create(size_t capacity);
void asock_connections_destroy(asock_connections_t *connections);


size_t asock_connections_size      (const asock_connections_t *connections);

int    asock_connections_push      (asock_connections_t*, const asock_t *);
int    asock_connections_erase     (asock_connections_t*, size_t index);
asock_t* asock_connections_at      (asock_connections_t*, size_t index);




#endif // ASOCK_CONNECTIONS_H
