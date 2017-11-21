#include "asock_vector.h"
#include "asock_logger.h"
#include "asock.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define MAX(a,b) ((a) > (b) ? (a) : b)

struct asock_vector {
    size_t m_size;
    void  *m_data;
    size_t m_capacity;
    size_t m_item_size;
};


static int reallocate(asock_vector_t *vector, size_t new_capacity)
{
    assert(vector);
    assert(new_capacity > vector->m_capacity);

    size_t new_size = new_capacity * vector->m_item_size;
    vector->m_data = realloc(vector->m_data, new_size);
    if (vector->m_data == NULL)
        return -1;
    return 0;
}

/* ------------ Constructors & Destructors ----------------------------- */

asock_vector_t* asock_vector_create(size_t item_size, size_t capacity)
{
    asock_vector_t *vector = malloc(sizeof(asock_vector_t));
    if (vector == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for asock vector");
        return NULL;
    }

    size_t init_size = MAX(item_size * capacity, 1);
    vector->m_data = malloc(init_size);
    if (vector->m_data == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for asock vector inern. buffer");
        free(vector);
        return NULL;
    }

    vector->m_size      = 0;
    vector->m_capacity  = capacity;
    vector->m_item_size = item_size;

    return vector;
}


void asock_vector_destroy(asock_vector_t* vector)
{
    assert(vector);
    free(vector->m_data);
    free(vector);
}




/* ----------- Nonmodifying functions --------------------------------- */

size_t asock_vector_size(const asock_vector_t* vector)
{
    assert(vector);
    return vector->m_size;
}


size_t asock_vector_capacity(const asock_vector_t* vector)
{
    assert(vector);
    return vector->m_capacity;
}

size_t asock_vector_index_of(const asock_vector_t* vector, const void *item)
{
    assert(vector);
    assert(item);

    for (size_t i = 0; i < vector->m_size; ++i) {
        void *data_pos = vector->m_data + i * vector->m_item_size;
        if (memcmp(data_pos, item, vector->m_item_size) == 0) {
            return i;
        }
    }
    return ASOCK_INVALID_VEC_INDEX;
}


/* ----------- Modifying functions ------------------------------------- */

int asock_vector_push (asock_vector_t* vector, const void* item)
{
    assert(vector);
    assert(item);

    if (vector->m_size == vector->m_capacity) {
        if (reallocate(vector, vector->m_capacity * 2) == -1)
            return ASOCK_FAILURE;
        vector->m_capacity = vector->m_capacity * 2;
    }

    ptrdiff_t deviation = vector->m_size * vector->m_item_size;
    memcpy(vector->m_data + deviation, item, vector->m_item_size);

    ++(vector->m_size);

    return ASOCK_SUCCESS;
}


int asock_vector_erase(asock_vector_t *vector, size_t index)
{
    assert(vector);

    if (vector->m_size == 0 || index >= vector->m_size)
        return ASOCK_FAILURE;

    memmove(vector->m_data + vector->m_item_size * index,
            vector->m_data + vector->m_item_size * (index + 1),
            (vector->m_size - 1 - index) * vector->m_item_size);
    vector->m_size--;
    return ASOCK_SUCCESS;
}


void asock_vector_clear(asock_vector_t *vector)
{
    vector->m_size = 0;
}



void *asock_vector_at(asock_vector_t *vector, size_t index)
{
    if (index >= vector->m_size)
        return NULL;

    return vector->m_data + index * vector->m_item_size;
}
