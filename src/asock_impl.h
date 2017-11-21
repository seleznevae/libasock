#ifndef ASOCK_IMPL_H
#define ASOCK_IMPL_H

#include <stdint.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "asock.h"
#include "asock_tsqueue.h"


struct asock
{
    int m_sock_desc;
    int64_t m_connection_id;
    int32_t m_master_id;
    volatile enum AsockConState m_state;
    volatile int m_disconnect;

    asock_tsqueue_t *m_in_queue;
    asock_tsqueue_t *m_out_queue;

    volatile int m_ref_counter;
    pthread_mutex_t m_ref_counter_mutex;

    struct sockaddr *m_peer_addr;
    socklen_t m_peer_addrlen;

    struct sockaddr *m_host_addr;
    socklen_t m_host_addrlen;

    char * m_net_ns;
};


void asock_con_counter_incr(asock_t *con);
asock_t * asock_fcreate_asock(int fd);


asock_t * asock_create_asock(int domain, int type, int protocol);
void asock_destroy_asock(asock_t *con);

int32_t asock_get_unique_service_id();


#endif // ASOCK_IMPL_H
