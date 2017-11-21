#ifndef ASOCK_DIRECTIVE_H
#define ASOCK_DIRECTIVE_H

#include "asock_defs.h"
#include <pthread.h>
#include "asock.h"

struct asock_directive {
    enum DirectiveType m_direct_type;

    pthread_mutex_t m_direct_mutex;
    pthread_cond_t m_direct_avail_condition;
    pthread_cond_t m_direct_result_condition;
    AsockReturnCode m_direct_result;

    void *m_arg1;
    void *m_arg2;
    void *m_arg3;
    void *m_arg4;
};

typedef struct asock_directive asock_directive_t;


void asock_init_directive(asock_directive_t *direct);

AsockReturnCode asock_send_directive(asock_directive_t *direct,
                                     enum DirectiveType type,
                                     void *arg1,
                                     void *arg2,
                                     void *arg3,
                                     void *arg4);

void asock_wait_for_directive(asock_directive_t *direct);

enum DirectiveType asock_get_directive_type(asock_directive_t *direct);
void asock_set_directive_result(asock_directive_t *direct, AsockReturnCode result);



#endif // ASOCK_DIRECTIVE_H
