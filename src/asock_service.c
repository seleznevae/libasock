#define _GNU_SOURCE
#include <stdlib.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sched.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



#include "asock_service.h"
#include "asock_tsqueue.h"
#include "asock_connections.h"
#include "asock_logger.h"
#include "asock_utils.h"
#include "asock_directive.h"



#define MAX_EVENTS            1024
#define CON_INIT_CAPACITY     ((size_t)10)
#define ASOCK_MES_BUFFER_SIZE 4096

struct service_config {
    char *m_net_ns;
    int m_net_ns_fd;
};

typedef struct service_config service_config_t;

static const service_config_t SERVICE_CONFIG_INITIALIZER = {
    .m_net_ns = NULL,
    .m_net_ns_fd = -1
};

struct asock_service {
    asock_connections_t *m_connections;
    asock_connections_t *m_new_connections;

    uint32_t m_created_connections;
    int m_epoll_desc;

    enum ServiceStatus m_service_status;


    struct epoll_event m_events[MAX_EVENTS];
    int32_t m_service_id;

    pthread_t m_thread;
    pthread_mutex_t m_new_connections_mutex;

    asock_directive_t m_directive;
    service_config_t m_config;
};



void* asock_service_thread_loop(void *);//asock_service_t *service);
int   asock_service_start_service(asock_service_t *service);
void  asock_service_update_state(asock_service_t *service);
void  asock_service_check_for_connections_to_close(asock_service_t *service);


ssize_t asock_service_try_send_data(asock_service_t *service,
                                   const asock_t *con,
                                   const uint8_t *data,
                                   uint32_t data_size);
void asock_service_send_unsent_data(asock_service_t *service);



AsockReturnCode asock_service_close_connection(asock_service_t *service,
                                  int sockd);
ssize_t asock_service_index_of_con(asock_service_t *service,
                                  const asock_t *con);
ssize_t asock_service_index_of_con_by_sock_desc(asock_service_t *server,
                                               int conn_sock);

void asock_service_try_connect_to_peer(asock_service_t *service, asock_t *conn);


asock_service_t* asock_service_create()
{
    return asock_service_create_ns("");
}

asock_service_t* asock_service_create_ns(const char *net_ns)
{
    asock_service_t *service = malloc(sizeof(asock_service_t));
    if (service == NULL)
        return NULL;

    if (pthread_mutex_init(&service->m_new_connections_mutex, NULL) != 0) {
        SYS_LOG_ERROR("Failed to initialize asock_service mutex");
        free(service);
        return NULL;
    }

    if ((service->m_epoll_desc = epoll_create1(0)) == -1) {
        free(service);
        return NULL;
    }

    memset(service->m_events, '\0', sizeof(service->m_events));

    service->m_connections = asock_connections_create(CON_INIT_CAPACITY);
    if (service->m_connections == NULL) {
        free(service);
        return NULL;
    }
    service->m_new_connections = asock_connections_create(CON_INIT_CAPACITY);
    if (service->m_new_connections == NULL) {
        asock_connections_destroy(service->m_connections);
        free(service);
        return NULL;
    }

    service->m_config = SERVICE_CONFIG_INITIALIZER;
    if (net_ns != NULL) {
        service->m_config.m_net_ns = strdup(net_ns);
        if (service->m_config.m_net_ns == NULL) {
            service->m_service_status = Aborted;
            asock_connections_destroy(service->m_new_connections);
            asock_connections_destroy(service->m_connections);
            free(service);
            return NULL;
        }
    }

    asock_init_directive(&service->m_directive);
    service->m_directive.m_direct_type = DirectNone;

    service->m_service_id = asock_get_unique_service_id();

    service->m_service_status = Init;

    int thread_start_status = pthread_create(&service->m_thread, NULL,
                                      &asock_service_thread_loop, service);
    if (thread_start_status != 0) {
        SYS_LOG_ERROR("service thread creating failed");
        service->m_service_status = Aborted;
        asock_connections_destroy(service->m_new_connections);
        asock_connections_destroy(service->m_connections);
        close(service->m_epoll_desc);
        free(service);
        return NULL;
    }

    /// @todo: do normal implementation
    while (service->m_service_status == Init) {
        sleep(1);
    }

    if (service->m_service_status == Aborted) {
        asock_connections_destroy(service->m_new_connections);
        asock_connections_destroy(service->m_connections);
        close(service->m_epoll_desc);
        free(service);
        return NULL;
    }

    return service;
}

void asock_service_destroy(asock_service_t *service)
{
    assert(service);

    if (asock_service_is_running(service))
        asock_service_stop(service);


    asock_connections_destroy(service->m_connections);
    asock_connections_destroy(service->m_new_connections);

    if (close(service->m_epoll_desc) != 0) {
        SYS_LOG_ERROR("Epoll descriptor closing failed");
    }

    if (service->m_config.m_net_ns_fd != -1) {
        close(service->m_config.m_net_ns_fd);
        service->m_config.m_net_ns_fd = -1;
    }
    if (service->m_config.m_net_ns) {
        free(service->m_config.m_net_ns);
        service->m_config.m_net_ns = NULL;
    }
    free(service);
}


AsockReturnCode asock_service_start(asock_service_t *service)
{
    if (service == NULL)
        return ASOCK_ERR_INVAL;

    if (asock_service_is_running(service))
        return ASOCK_ERR_BADSTATE;

    return asock_send_directive(&service->m_directive, DirectStart, NULL, NULL, NULL, NULL);



/*
//    if (asock_service_is_running(service)) {
//        if (strcmp(net_ns,  service->m_net_ns) == 0) {
//            return ASOCK_SUCCESS;
//        } else {
//            return ASOCK_FAILURE;
//        }
//    }

//    char *tmp = service->m_net_ns;
//    if (net_ns) {
//        service->m_net_ns = strdup(net_ns);
//        if (service->m_net_ns == NULL)
//            return ASOCK_ERR_NOMEM;
//    } else {
//        service->m_net_ns = NULL;
//    }
//    free(tmp);


//    service->m_service_status = Configuration;

//    int thread_start_status = pthread_create(&service->m_thread, NULL,
//                                      &asock_service_thread_loop, service);
//    if (thread_start_status != 0) {
//        SYS_LOG_ERROR("service thread creating failed");
//        free(service->m_net_ns);
//        service->m_net_ns = NULL;
//        service->m_service_status = Aborted;
//        return ASOCK_FAILURE;
//    }


//    while (service->m_service_status == Configuration)
//        ;//WAITING

//    if (service->m_service_status == Aborted) {
//        pthread_join(service->m_thread, NULL);
//        return ASOCK_FAILURE;
//    }

//    return ASOCK_SUCCESS;
*/
}


void asock_service_stop(asock_service_t *service)
{
    if (!asock_service_is_running(service))
        return;

    asock_send_directive(&service->m_directive, DirectStop, NULL, NULL, NULL, NULL);
    pthread_join(service->m_thread, NULL);
    service->m_service_status = Aborted;
    LOG_INFO("Service was stopped");


//    if (!asock_service_is_running(service))
//        return;
//    service->m_stop_service = DirectStop;
//    pthread_join(service->m_thread, NULL);
//    service->m_service_status = Configuration;
//    LOG_INFO("Asock service was stopped");
}

int asock_service_is_running(const asock_service_t *service)
{
    assert(service);
    return service->m_service_status == Running;
}

//asock_t *asock_service_connect(asock_service_t *service, const struct sockaddr *addr, socklen_t addrlen)
//{
//    if (addr == NULL)
//        return NULL;


//    asock_t *new_con = (asock_t*)asock_create();
//    if (new_con == NULL)
//        return NULL;


//    new_con->m_master_id = service->m_service_id;
//    new_con->m_state = InProgress;

//    new_con->m_peer_addr = malloc(addrlen);
//    if (new_con->m_peer_addr == NULL) {
//        SYS_LOG_ERROR("Failed to allocate memory for asock address");
//        asock_destroy(new_con);
//        return NULL;
//    }
//    memcpy(new_con->m_peer_addr, addr, addrlen);
//    new_con->m_peer_addrlen = addrlen;

//    asock_con_counter_incr(new_con);


//    // Closing client connections
//    pthread_mutex_lock(&service->m_new_connections_mutex);
//    asock_connections_push(service->m_new_connections, new_con);
//    pthread_mutex_unlock(&service->m_new_connections_mutex);

//    return new_con;

//}

AsockReturnCode asock_service_connect(asock_service_t *service, asock_t *socket, const struct sockaddr *addr, socklen_t addrlen)
{
    if (addr == NULL)
        return ASOCK_ERR_INVAL;
    if (socket == NULL)
        return ASOCK_ERR_INVAL;


    struct sockaddr *peer_addr = malloc(addrlen);
    if (peer_addr == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for asock address");
        return ASOCK_ERR_NOMEM;
    }
    socket->m_peer_addr = peer_addr;
    memcpy(socket->m_peer_addr, addr, addrlen);
    socket->m_peer_addrlen = addrlen;

    socket->m_master_id = service->m_service_id;
    socket->m_state = InProgress;

    asock_con_counter_incr(socket);


    pthread_mutex_lock(&service->m_new_connections_mutex);
    asock_connections_push(service->m_new_connections, socket);
    pthread_mutex_unlock(&service->m_new_connections_mutex);

    return ASOCK_SUCCESS;
}

asock_t * asock_service_create_asock(asock_service_t *service, int domain, int type, int protocol)
{
    if (service == NULL)
        return NULL;

    if (service->m_service_status == Aborted || service->m_service_status == Init)
        return NULL;

    asock_t *result = NULL;
    int status = asock_send_directive(&service->m_directive, DirectCreateAsock, &domain, &type, &protocol, &result);
    if (result && status != ASOCK_SUCCESS) {
        LOG_ERROR("Unexpected situation");
    }
    return result;
}

AsockReturnCode asock_service_process_create_asock_directive(asock_service_t *service)
{
    assert(service);

    asock_directive_t *direct = &service->m_directive;
    AsockReturnCode result = ASOCK_SUCCESS;

    pthread_mutex_lock(&direct->m_direct_mutex);

    int domain = *(int*)(direct->m_arg1);
    int type = *(int*)(direct->m_arg2);
    int protocol = *(int*)(direct->m_arg3);
    asock_t **dst = (asock_t**)(direct->m_arg4);

    *dst = asock_create_asock(domain, type, protocol);
    if (*dst == NULL)
        result = ASOCK_FAILURE;

    pthread_mutex_unlock(&direct->m_direct_mutex);

    return result;
}


void* asock_service_thread_loop(void *p_service)//asock_service_t *service);
{
    asock_service_t *service = (asock_service_t *)p_service;

    service->m_service_status = Configuration;

    int continue_configuration = 1;
    while (continue_configuration) {
        asock_wait_for_directive(&service->m_directive);

        enum DirectiveType type = asock_get_directive_type(&service->m_directive);
        switch (type) {
            case DirectStart:
                continue_configuration = 0;
                break;
            case DirectStop:
                service->m_service_status = Aborted;
                asock_set_directive_result(&service->m_directive, ASOCK_SUCCESS);
                return NULL;
            case DirectCreateAsock:
                asock_set_directive_result(&service->m_directive,
                      asock_service_process_create_asock_directive(service));
                break;
            case DirectSetns:
                //todo
                break;

            case DirectBind:
            case DirectNone:
            default:
                LOG_ERROR("Undefined Directive type in server thread loop");
                assert(0);
                asock_set_directive_result(&service->m_directive, ASOCK_FAILURE);
                service->m_service_status = Aborted;
                return NULL;
        }
    }




//    int fd_nms = -1;
//    if (service->m_net_ns && service->m_net_ns[0] != '\0') {
//        fd_nms = open(service->m_net_ns, O_RDONLY); /* Get descriptor for namespace */
//        if (fd_nms == -1) {
//            SYS_LOG_ERROR("Failed to open nms file %s", service->m_net_ns);
//            service->m_service_status = Aborted;
//            return NULL;
//        }
//        if (setns(fd_nms, CLONE_NEWNET) == -1) {
//            SYS_LOG_ERROR("Failed setns");
//            close(fd_nms);
//            service->m_service_status = Aborted;
//            return NULL;
//        }
//    }

    service->m_service_status = Running;
    asock_set_directive_result(&service->m_directive, ASOCK_SUCCESS);

    // main loop
    while (1) {
        enum DirectiveType type = asock_get_directive_type(&service->m_directive);
        int stop_is_needed = 0;
        switch (type) {
            case DirectNone:
                break;
            case DirectStop:
                stop_is_needed = 1;
                break;

            case DirectCreateAsock:
                asock_set_directive_result(&service->m_directive,
                      asock_service_process_create_asock_directive(service));
                break;

            case DirectStart:
            case DirectSetns:
                asock_set_directive_result(&service->m_directive, ASOCK_ERR_BADSTATE);
                //todo
                break;

            case DirectBind:
            default:
                LOG_ERROR("Undefined Directive type in server thread loop");
                assert(0);
                asock_set_directive_result(&service->m_directive, ASOCK_FAILURE);
                return NULL;
                break;
        }

        if (stop_is_needed) {
            break;
        }


//        if (service->m_stop_service == DirectStop) {
//            break;
//        }
        asock_service_update_state(service);
    }


    // Closing client connections
    pthread_mutex_lock(&service->m_new_connections_mutex);
    size_t sz = asock_connections_size(service->m_new_connections);
    for (size_t i = sz - 1; i < sz; --i) {
        asock_t *con = asock_connections_at(service->m_new_connections, i);
        asock_destroy_asock(con);
    }
    pthread_mutex_unlock(&service->m_new_connections_mutex);

    sz = asock_connections_size(service->m_connections);
    for (size_t i = sz - 1; i < sz; --i) {
        asock_t *con = asock_connections_at(service->m_connections, i);
        asock_destroy_asock(con);
    }

//    if (service->m_net_ns && service->m_net_ns[0] != '\0') {
//        if (close(fd_nms) == -1) {
//            SYS_LOG_ERROR("Failed closing net namespace.");
//        }
//    }

    if (service->m_config.m_net_ns_fd != -1) {
        if (close(service->m_config.m_net_ns_fd) == -1) {
            SYS_LOG_ERROR("Failed closing net namespace.");
        }
        service->m_config.m_net_ns_fd = -1;
    }
    if (service->m_config.m_net_ns) {
        free(service->m_config.m_net_ns);
        service->m_config.m_net_ns = NULL;
    }

    // sending result for stop directive
    asock_set_directive_result(&service->m_directive, ASOCK_SUCCESS);

    return NULL;
}


void asock_service_update_state(asock_service_t *service)
{
    // Closing client connections
    pthread_mutex_lock(&service->m_new_connections_mutex);
    size_t sz = asock_connections_size(service->m_new_connections);
    for (size_t i = 0; i < sz; i++) {
        asock_t *con = asock_connections_at(service->m_new_connections, i);

        asock_service_try_connect_to_peer(service, con);
        if (con->m_state == Failed) {
            asock_destroy_asock(con);
        } else {
            asock_connections_push(service->m_connections, con);
        }
    }
    asock_vector_clear(service->m_new_connections);
    pthread_mutex_unlock(&service->m_new_connections_mutex);


    asock_service_send_unsent_data(service);
    asock_service_check_for_connections_to_close(service);

    int events_number = epoll_wait(service->m_epoll_desc, service->m_events, MAX_EVENTS, EPOLL_WAIT_DEFAULT_TIMEOUT);
    if (events_number == -1)
        SYS_LOG_ERROR("epoll_wait failed");
    for (int i = 0; i < events_number; ++i) {

        uint32_t event = service->m_events[i].events;
        int event_fd = service->m_events[i].data.fd;

        if (event & EPOLLOUT) {
            int index = asock_service_index_of_con_by_sock_desc(service, event_fd);
            if (index != -1) {
                int error;
                socklen_t sz = sizeof(error);
                if (getsockopt(event_fd, SOL_SOCKET, SO_ERROR, &error, &sz) == -1)
                    asock_service_close_connection(service, event_fd);
                else {
                    if (error == 0)
                        asock_connections_at(service->m_connections, index)->m_state = Established;
                    else {
                        LOG_WARNING("EPOLLOUT: %s", strerror(errno));
                        asock_service_close_connection(service, event_fd);
                        continue;
                    }
                }
            }
        }

        if (event & EPOLLERR || event & EPOLLHUP) {
            SYS_LOG_ERROR("epoll EPOLLERR");
            asock_service_close_connection(service, event_fd);
            continue;
        }

        if (event & EPOLLIN || event & EPOLLPRI) {
            /* We have data on the fd waiting to be read. Read and
            display it. We must read whatever data is available
            completely, as we are running in edge-triggered mode
            and won't get a notification again for the same
            data. */
            while (1) {
                ssize_t count;
                const int BUF_SIZE = 4096;
                uint8_t buf[BUF_SIZE];

                count = read(event_fd, buf, sizeof buf);
                if (count == -1) {
                    if (errno != EAGAIN)  //errno == EAGAIN means we read all data
                        asock_service_close_connection(service, event_fd);
                    break;
                } else if (count == 0) { /* End of file. The remote has closed the connection. */
                    asock_service_close_connection(service, event_fd);
                    break;
                }

                ssize_t index = asock_service_index_of_con_by_sock_desc(service, event_fd);
                if (index != -1) {
                    asock_t *con = asock_connections_at(service->m_connections, index);
                    asock_tsqueue_enqueue(con->m_in_queue, buf, count);
                }
            }
        }
    } //for
}



AsockReturnCode asock_service_close_connection(asock_service_t *service, int sockd)
{
    assert(service);
    assert(sockd >= 0);

    pthread_mutex_lock(&service->m_new_connections_mutex);
    size_t new_cons_sz = asock_connections_size(service->m_new_connections);
    for (size_t i = 0; i < new_cons_sz; ++i) {
        asock_t *con = asock_connections_at(service->m_new_connections, i);
        if (con->m_sock_desc == sockd) {
            asock_connections_erase(service->m_new_connections, i);
            break;
        }
    }
    pthread_mutex_unlock(&service->m_new_connections_mutex);


    ssize_t index = asock_service_index_of_con_by_sock_desc(service, sockd);
    if (index == -1)
        return ASOCK_ERR_INVAL;

    asock_t *con = asock_connections_at(service->m_connections, index);
    con->m_state = Failed;

    /* Closing the descriptor will make epoll remove it
       from the set of descriptors which are monitored. */
    if (close(sockd) == -1)
        SYS_LOG_ERROR("socket close failed");
    asock_destroy_asock(con);
    asock_connections_erase(service->m_connections, index);
    return ASOCK_SUCCESS;

}


ssize_t asock_service_index_of_con(asock_service_t *service,
                                  const asock_t *connection)
{
    if (connection->m_master_id != service->m_service_id)
        return -1;

    size_t cons_sz = asock_connections_size(service->m_connections);
    for (size_t i = 0; i < cons_sz; ++i) {
        asock_t *con = asock_connections_at(service->m_connections, i);
        if (connection->m_connection_id == con->m_connection_id) {
            return i;
        }
    }
    return -1;
}



ssize_t asock_service_index_of_con_by_sock_desc(asock_service_t *service,
                                               int sockd)
{
    size_t cons_sz = asock_connections_size(service->m_connections);
    for (size_t i = 0; i < cons_sz; ++i) {
        asock_t *con = asock_connections_at(service->m_connections, i);
        if (con->m_sock_desc == sockd) {
            return i;
        }
    }
    return -1;
}


void asock_service_send_unsent_data(asock_service_t *service)
{
    size_t con_sz = asock_connections_size(service->m_connections);
    // starting from the end,  because while writing some connections
    // may be closed and deleted from m_connections
    for (size_t i = con_sz - 1; i < con_sz; --i) {

        asock_t *con = asock_connections_at(service->m_connections, i);
        assert(con);

        asock_tsqueue_head_lock(con->m_out_queue);
        size_t out_data_sz = asock_tsqueue_head_size(con->m_out_queue);
        if (out_data_sz != 0) {
            uint8_t *data_to_send = asock_tsqueue_head(con->m_out_queue);
            assert(data_to_send);

            ssize_t send_data_sz = asock_service_try_send_data(service, con, data_to_send, out_data_sz);
            asock_tsqueue_head_unlock(con->m_out_queue);
            if (send_data_sz < 0)
                continue;
            assert((size_t)send_data_sz <= out_data_sz);

            asock_tsqueue_dequeue(con->m_out_queue, NULL, send_data_sz);
        } else {
            asock_tsqueue_head_unlock(con->m_out_queue);
        }
    }
}


ssize_t asock_service_try_send_data(asock_service_t *service,
                                   const asock_t *con,
                                   const uint8_t *data,
                                   uint32_t data_size)
{
    if (!asock_service_is_running(service))
        return -1;


    int index = asock_service_index_of_con(service, con);
    if (index == -1)
        return -1;

    switch (con->m_state) {
        case Idle:
        case Failed:
        case InProgress:
            return -1;
            break;
        case Established:
            {
                ssize_t n = 0;
                if ( (n = send(con->m_sock_desc, data, data_size, MSG_NOSIGNAL)) == -1) {
                    if (errno == EWOULDBLOCK || errno == EAGAIN)
                        return 0;
                    LOG_ERROR("Error occured when writing to socket %d. "
                              "Closing connection.\n", con->m_sock_desc);
                    asock_service_close_connection(service, con->m_sock_desc);
                    return -1;
                }
                return n;
            }
    }

    return -1;
}


void asock_service_check_for_connections_to_close(asock_service_t *service)
{
    size_t sz = asock_connections_size(service->m_connections);
    for (size_t i = sz - 1; i < sz; --i) {
        asock_t *con = asock_connections_at(service->m_connections, i);
        if (con->m_disconnect) {
            asock_service_close_connection(service, con->m_sock_desc);
        }
    }
}

void asock_service_try_connect_to_peer(asock_service_t *service, asock_t *con)
{
    assert(service);
    assert(con);
    assert(con->m_peer_addr);

    con->m_state = InProgress;

    if ((con->m_sock_desc = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        SYS_LOG_ERROR("Failed socket");
        con->m_state = Failed;
        con->m_sock_desc = -1;
        return;
    }

    if (make_socket_non_blocking(con->m_sock_desc) != 0) {
        SYS_LOG_ERROR("Make_socket_non_blocking failed.");
        if (close(con->m_sock_desc) == -1)
            SYS_LOG_ERROR("Socket close failed");
        con->m_state = Failed;
        con->m_sock_desc = -1;
        return;
    }

    if (connect(con->m_sock_desc, con->m_peer_addr,
                con->m_peer_addrlen) == -1) {
        if (errno != EINPROGRESS) {
            LOG_WARNING("Connection failed");
            if (close(con->m_sock_desc) == -1)
                SYS_LOG_ERROR("Socket close failed");
            con->m_state = Failed;
            con->m_sock_desc = -1;
            return;
        }
    } else {
        con->m_state = Established;
    }


    if (add_socket_to_epoll(service->m_epoll_desc, con->m_sock_desc) == -1) {
        if (close(con->m_sock_desc) == -1)
            SYS_LOG_ERROR("Socket close failed");
        con->m_state = Failed;
        con->m_sock_desc = -1;
        return;
    }
    return;
}

//AsockReturnCode asock_service_setns(asock_service_t *server, const char *net_ns)
//{
//    //tmp
//    (void)server;
//    (void)net_ns;
//    return ASOCK_SUCCESS;
//}


