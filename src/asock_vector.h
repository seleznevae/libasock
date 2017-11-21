#ifndef ASOCK_VECTOR_H
#define ASOCK_VECTOR_H

#ifdef __cplusplus
extern "C"
{
#endif


#include <stddef.h>
#include "asock_defs.h"


struct asock_vector;
typedef struct asock_vector asock_vector_t;

#define ASOCK_INVALID_VEC_INDEX ((size_t) -1)

asock_vector_t* asock_vector_create(size_t item_size, size_t capacity);
void asock_vector_destroy(asock_vector_t*);

size_t asock_vector_size      (const asock_vector_t*);
size_t asock_vector_capacity  (const asock_vector_t*);
size_t asock_vector_index_of  (const asock_vector_t*, const void *item);

int    asock_vector_push      (asock_vector_t*, const void *item);
int    asock_vector_erase     (asock_vector_t*, size_t index);
void   asock_vector_clear     (asock_vector_t*);
void*  asock_vector_at        (asock_vector_t*, size_t index);


#ifdef __cplusplus
}
#endif

#endif /* ASOCK_VECTOR_H */
