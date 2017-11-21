#include "asock_directive.h"
#include <assert.h>

static const pthread_mutex_t MUTEX_INITIALIZER = PTHREAD_MUTEX_INITIALIZER;
static const pthread_cond_t COND_INITIALIZER = PTHREAD_COND_INITIALIZER;

void asock_init_directive(asock_directive_t *direct)
{
    assert(direct);
    direct->m_direct_type = DirectNone;
    direct->m_direct_mutex = MUTEX_INITIALIZER;
    direct->m_direct_avail_condition = COND_INITIALIZER;
    direct->m_direct_result_condition = COND_INITIALIZER;
    direct->m_direct_result = ASOCK_SUCCESS;
    direct->m_arg1 = NULL;
    direct->m_arg2 = NULL;
    direct->m_arg3 = NULL;
    direct->m_arg4 = NULL;
}

AsockReturnCode asock_send_directive(asock_directive_t *direct, enum DirectiveType type, void *arg1, void *arg2, void *arg3, void *arg4)
{
    assert(direct);

    pthread_mutex_lock(&direct->m_direct_mutex);
    direct->m_direct_type = type;
    direct->m_arg1 = arg1;
    direct->m_arg2 = arg2;
    direct->m_arg3 = arg3;
    direct->m_arg4 = arg4;
    direct->m_direct_result = ASOCK_UNDEFINED_CODE;

    pthread_cond_signal(&direct->m_direct_avail_condition);
    while (direct->m_direct_result == ASOCK_UNDEFINED_CODE)
        pthread_cond_wait(&direct->m_direct_result_condition, &direct->m_direct_mutex);

    int result = direct->m_direct_result;
    direct->m_direct_result = ASOCK_UNDEFINED_CODE;
    direct->m_direct_type = DirectNone;

    pthread_mutex_unlock(&direct->m_direct_mutex);
    return result;
}

void asock_wait_for_directive(asock_directive_t *direct)
{
    assert(direct);

    pthread_mutex_lock(&direct->m_direct_mutex);

    while (direct->m_direct_type == DirectNone)
        pthread_cond_wait(&direct->m_direct_avail_condition, &direct->m_direct_mutex);

    pthread_mutex_unlock(&direct->m_direct_mutex);
}

enum DirectiveType asock_get_directive_type(asock_directive_t *direct)
{
    assert(direct);

    pthread_mutex_lock(&direct->m_direct_mutex);
    enum DirectiveType type = direct->m_direct_type;
    pthread_mutex_unlock(&direct->m_direct_mutex);

    return type;
}

void asock_set_directive_result(asock_directive_t *direct, AsockReturnCode result)
{
    assert(direct);

    pthread_mutex_lock(&direct->m_direct_mutex);
    direct->m_direct_result = result;
    pthread_cond_signal(&direct->m_direct_result_condition);
    direct->m_direct_type = DirectNone;
    pthread_mutex_unlock(&direct->m_direct_mutex);
}


