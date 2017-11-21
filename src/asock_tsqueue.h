#ifndef ASOCK_TSQUEUE_H
#define ASOCK_TSQUEUE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include "asock.h"

struct asock_tsqueue;
typedef struct asock_tsqueue asock_tsqueue_t;

asock_tsqueue_t* asock_tsqueue_create (size_t item_size, size_t capacity);
void             asock_tsqueue_destroy(asock_tsqueue_t*);

size_t asock_tsqueue_size    (const asock_tsqueue_t*);
size_t asock_tsqueue_capacity(const asock_tsqueue_t*);


void   asock_tsqueue_head_lock  (asock_tsqueue_t*);
void   asock_tsqueue_head_unlock(asock_tsqueue_t*);
size_t asock_tsqueue_head_size  (const asock_tsqueue_t*);
void * asock_tsqueue_head       (asock_tsqueue_t*);




void   asock_tsqueue_clear   (asock_tsqueue_t*);
size_t asock_tsqueue_dequeue (asock_tsqueue_t*,
                              void *dst,
                              size_t dst_capacity);
AsockReturnCode asock_tsqueue_enqueue (asock_tsqueue_t*,
                              const void *src,
                              size_t item_count);




void asock_tsqueue_for_each(const asock_tsqueue_t*,
                            void (*func) (void *item, size_t index));

#ifdef __cplusplus
}
#endif

#endif /* ASOCK_TSQUEUE_H */
