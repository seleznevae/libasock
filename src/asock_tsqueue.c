#include "asock_tsqueue.h"
#include "asock_logger.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))

/*

  queue m_size = 8
  queue m_capacity = 8

    0      1      2        3         4       5      6      7
| junk | junk | DATA_0 | DATA_1 | DATA_2 | junk | junk | junk |
^             ^                          ^
|             |                          |
|             |-m_head_pos = 2           |--m_tail_pos = 5
|- m_data

*/
struct asock_tsqueue {

    void  *m_data;
    volatile size_t m_size;
    volatile size_t m_capacity;
    size_t m_item_size;
    size_t m_tail_pos;
    size_t m_head_pos;
    pthread_mutex_t m_mutex;
};




static AsockReturnCode reallocate(asock_tsqueue_t *queue, size_t new_capacity)
{
    assert(queue);
    assert(new_capacity > queue->m_capacity);

    size_t new_size = new_capacity * queue->m_item_size;
    queue->m_data = realloc(queue->m_data, new_size);
    if (queue->m_data == NULL) {
        SYS_LOG_ERROR("Failed to allocate extra memory for tsqueue.");
        return ASOCK_ERR_NOMEM;
    }
    return ASOCK_SUCCESS;
}


/* ------------ Constructors & Destructors ----------------------------- */

asock_tsqueue_t* asock_tsqueue_create(size_t item_size, size_t capacity)
{
    /* capacity should be power of 2 */
    size_t right_capacity = 1;
/*    size_t capacity_order = 0; */
    while (right_capacity < capacity) {
        right_capacity = right_capacity << 1;
/*        ++capacity_order; */
    }



    asock_tsqueue_t *queue = malloc(sizeof(asock_tsqueue_t));
    if (queue == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for tsqueue structure.");
        return NULL;
    }

    if (pthread_mutex_init(&queue->m_mutex, NULL) != 0) {
        free(queue);
        SYS_LOG_ERROR("Failed to initialize tsqueue mutex");
        return NULL;
    }


    size_t init_size = MAX(item_size * right_capacity, 1);
    queue->m_data = malloc(init_size);
    if (queue->m_data == NULL) {
        free(queue);
        SYS_LOG_ERROR("Failed to allocate memory for tsqueue data.");
        return NULL;
    }


    queue->m_size           = 0;
    queue->m_capacity       = right_capacity;
/*    queue->m_capacity_order = capacity_order; */
    queue->m_item_size      = item_size;
    queue->m_tail_pos       = 0;
    queue->m_head_pos       = 0;

    return queue;
}


void asock_tsqueue_destroy(asock_tsqueue_t* queue)
{
    assert(queue);

    /* this function is supposed to be invoked when asock_tsqueue_t can
       be accessed from one thread, so there is no need in synchronization
    */

    free(queue->m_data);

    if (pthread_mutex_destroy(&queue->m_mutex) != 0) {
        ; /* NOTE: should I check and do somthing ??? */
    }

    free(queue);

}


/* ----------- Nonmodifying functions --------------------------------- */

size_t asock_tsqueue_size(const asock_tsqueue_t *queue)
{
    assert(queue);
    return queue->m_size;
}

size_t asock_tsqueue_capacity(const asock_tsqueue_t *queue)
{
    assert(queue);
    return queue->m_capacity;
}


void asock_tsqueue_head_lock(asock_tsqueue_t *queue)
{
    pthread_mutex_lock(&queue->m_mutex);
}

void asock_tsqueue_head_unlock(asock_tsqueue_t *queue)
{
    pthread_mutex_unlock(&queue->m_mutex);
}

size_t asock_tsqueue_head_size(const asock_tsqueue_t *queue)
{
    assert(queue);

    return MIN(queue->m_size, queue->m_capacity - queue->m_head_pos);
}

void *asock_tsqueue_head(asock_tsqueue_t *queue)
{
    assert(queue);
    if (queue->m_size == 0)
        return NULL;

    return queue->m_data + queue->m_item_size * queue->m_head_pos;
}

void asock_tsqueue_clear(asock_tsqueue_t *queue)
{
    assert(queue);

    pthread_mutex_lock(&queue->m_mutex);
    queue->m_size = 0;
    queue->m_tail_pos = 0;
    queue->m_head_pos = 0;
    pthread_mutex_unlock(&queue->m_mutex);
}


AsockReturnCode asock_tsqueue_enqueue(asock_tsqueue_t *queue,
                          const void *src,
                          size_t item_count)
{
    assert(queue);
    assert(src);

    pthread_mutex_lock(&queue->m_mutex);

    if (queue->m_size + item_count > queue->m_capacity) {
        size_t new_capacity = queue->m_capacity * 2;
        while (new_capacity < (queue->m_size + item_count))
            new_capacity *= 2;

        if (reallocate(queue, new_capacity) != ASOCK_SUCCESS) {
            pthread_mutex_unlock(&queue->m_mutex);
            return ASOCK_ERR_NOMEM;
        }

        /* relocate head if needed */
        if (queue->m_tail_pos <= queue->m_head_pos
                && queue->m_size) {
            memcpy(queue->m_data + queue->m_item_size * queue->m_capacity,
                   queue->m_data,
                   queue->m_tail_pos * queue->m_item_size);
            queue->m_tail_pos = queue->m_capacity + queue->m_tail_pos;
        }

        queue->m_capacity = new_capacity;
    }

    size_t front_space = queue->m_capacity - queue->m_tail_pos;
    if (item_count <= front_space) {
        memcpy(queue->m_data + queue->m_item_size * queue->m_tail_pos,
               src,
               queue->m_item_size * item_count);
        queue->m_tail_pos += item_count;
    } else {
        size_t second_batch = item_count - front_space;

        memcpy(queue->m_data + queue->m_item_size * queue->m_tail_pos,
               src,
               queue->m_item_size * front_space);
        memcpy(queue->m_data,
               src + queue->m_item_size * front_space,
               queue->m_item_size * second_batch);
        queue->m_tail_pos = second_batch;
    }

    if (queue->m_tail_pos == queue->m_capacity)
        queue->m_tail_pos = 0;

    queue->m_size += item_count;

    pthread_mutex_unlock(&queue->m_mutex);

    return ASOCK_SUCCESS;
}

size_t asock_tsqueue_dequeue(asock_tsqueue_t *queue,
                             void *dst,
                             size_t dst_capacity)
{
    /* if dst == NULL, data are simply popped from head */

    assert(queue);

    pthread_mutex_lock(&queue->m_mutex);
    size_t copy_sz = MIN(dst_capacity, queue->m_size);

    if (queue->m_capacity >= queue->m_head_pos + copy_sz) {
        if (dst) {
            memcpy(dst,
                   queue->m_data + queue->m_item_size * queue->m_head_pos,
                   queue->m_item_size * copy_sz);
        }
        queue->m_head_pos += copy_sz;
    } else {
        size_t first_batch  = queue->m_capacity - queue->m_head_pos;
        size_t second_batch = copy_sz - first_batch;

        if (dst) {
            memcpy(dst,
                   queue->m_data + queue->m_item_size * queue->m_head_pos,
                   queue->m_item_size * first_batch);
            memcpy(dst + queue->m_item_size * first_batch,
                   queue->m_data,
                   queue->m_item_size * second_batch);
        }
        queue->m_head_pos = second_batch;
    }

    if (queue->m_head_pos == queue->m_capacity)
        queue->m_head_pos = 0;

    queue->m_size -= copy_sz;

    pthread_mutex_unlock(&queue->m_mutex);

    return copy_sz;
}


void asock_tsqueue_for_each(const asock_tsqueue_t *queue, void (*func)(void *, size_t))
{
    assert(queue);

    pthread_mutex_lock((pthread_mutex_t*)&queue->m_mutex);
    size_t sz = queue->m_size;
    for (size_t i = 0; i < sz; ++i) {
        size_t pos = (queue->m_head_pos + i) % queue->m_capacity;
        func(queue->m_data + pos * queue->m_item_size, i);
    }
    pthread_mutex_unlock((pthread_mutex_t*)&queue->m_mutex);
}



