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


#include <sys/socket.h>



#include "asock_tsqueue.h"
#include "asock_connections.h"
#include "asock_logger.h"
#include "asock_utils.h"
#include "asock_directive.h"



#define MAX_EVENTS            1024
#define CON_INIT_CAPACITY     ((size_t)10)
#define ASOCK_MES_BUFFER_SIZE 4096

struct server_config {
    int m_domain;
    int m_type;
    int m_protocol;

//    uint16_t m_server_port;
    const struct sockaddr *m_serv_addr;
    socklen_t m_serv_addrlen;

    char *m_net_ns;
    int m_net_ns_fd;

//    pthread_mutex_t m_config_mutex;
};
typedef struct server_config server_config_t;

static const server_config_t SERVER_CONFIG_INITIALIZER = {
    .m_domain = 0,
    .m_type = 0,
    .m_protocol = 0,
    .m_serv_addr = NULL,
    .m_serv_addrlen = 0,
    .m_net_ns = NULL,
    .m_net_ns_fd = -1
//    .m_config_mutex = PTHREAD_MUTEX_INITIALIZER
};

enum FunctionLocking {
    ExternLocking,
    InternLocking
};



struct asock_server {
//    char *m_net_ns;
    asock_connections_t *m_connections;
    asock_connections_t *m_new_connections;

    uint32_t m_created_connections;
    int m_epoll_desc;
    int m_server_socket;

    enum ServiceStatus m_server_status;


//    uint16_t m_server_port;
    struct epoll_event m_events[MAX_EVENTS];
    int32_t m_server_id;

    server_config_t m_config;
    pthread_t m_thread;
    pthread_mutex_t m_new_connections_mutex;

    asock_directive_t m_directive;
};




void* asock_server_thread_loop(void *);//asock_server_t *server);
AsockReturnCode asock_server_start_server(asock_server_t *server);
void  asock_server_update_state(asock_server_t *server);
void  asock_server_check_for_connections_to_close(asock_server_t *server);


ssize_t asock_server_try_send_data(asock_server_t *server,
                                   const asock_t *con,
                                   const uint8_t *data,
                                   uint32_t data_size);
void asock_server_send_unsent_data(asock_server_t *server);



AsockReturnCode asock_server_close_connection(asock_server_t *server,
                                  int sockd);
ssize_t asock_server_index_of_con(asock_server_t *server,
                                  const asock_t *con);
ssize_t asock_server_index_of_con_by_sock_desc(asock_server_t *server,
                                               int conn_sock);



AsockReturnCode asock_server_send_directive(asock_server_t *server,
                                            enum DirectiveType directive,
                                            enum FunctionLocking locking);











asock_server_t* asock_server_create (int domain, int type, int protocol)
{
    return asock_server_create_ns(domain, type, protocol, "");
}

asock_server_t* asock_server_create_ns(int domain, int type, int protocol, const char* net_ns)
{
    (void)net_ns;

//    int sock_d;
//    sock_d = socket (domain, type, protocol);
//    if (sock_d == -1) {
//        LOG_WARNING("Failed to create socket");
//        return NULL;
//    }

//    int enable = 1;
//    if (setsockopt(sock_d, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
//        close(sock_d);
//        LOG_WARNING("Setsockopt for  SO_REUSEADDR for server failed\n");
//        return NULL;
//    }


    asock_server_t *server = malloc(sizeof(asock_server_t));
    if (server == NULL)
        return NULL;

    server->m_server_socket = -1;

    if (pthread_mutex_init(&server->m_new_connections_mutex, NULL) != 0) {
        SYS_LOG_ERROR("Failed to initialize asock_server mutex");
        free(server);
        return NULL;
    }

    if ((server->m_epoll_desc = epoll_create1(0)) == -1) {
        free(server);
        return NULL;
    }

    memset(server->m_events, '\0', sizeof(server->m_events));

    server->m_connections = asock_connections_create(CON_INIT_CAPACITY);
    if (server->m_connections == NULL) {
        free(server);
        return NULL;
    }
    server->m_new_connections = asock_connections_create(CON_INIT_CAPACITY);
    if (server->m_new_connections == NULL) {
        asock_connections_destroy(server->m_connections);
        free(server);
        return NULL;
    }

    server->m_config = SERVER_CONFIG_INITIALIZER;
    server->m_config.m_domain = domain;
    server->m_config.m_type = type;
    server->m_config.m_protocol = protocol;
    if (net_ns != NULL) {
        server->m_config.m_net_ns = strdup(net_ns);
        if (server->m_config.m_net_ns == NULL) {
            server->m_server_status = Aborted;
            asock_connections_destroy(server->m_new_connections);
            asock_connections_destroy(server->m_connections);
            close(server->m_epoll_desc);
            free(server);
            return NULL;
        }
    }


    asock_init_directive(&server->m_directive);
    server->m_directive.m_direct_type = DirectNone;

    server->m_server_id = asock_get_unique_service_id();

    server->m_server_status = Init;

    int thread_start_status = pthread_create(&server->m_thread, NULL,
                                      &asock_server_thread_loop, server);
    if (thread_start_status != 0) {
        SYS_LOG_ERROR("Server thread creating failed");
        server->m_server_status = Aborted;
        asock_connections_destroy(server->m_new_connections);
        asock_connections_destroy(server->m_connections);
        close(server->m_epoll_desc);
        free(server);
        return NULL;
    }

    /// @todo: do normal implementation
    while (server->m_server_status == Init) {
        sleep(1);
    }

    if (server->m_server_status == Aborted) {
        asock_connections_destroy(server->m_new_connections);
        asock_connections_destroy(server->m_connections);
        close(server->m_epoll_desc);
        free(server);
        return NULL;
    }

    return server;
}

void asock_server_destroy(asock_server_t *server)
{
    assert(server);

    if (asock_server_is_running(server))
        asock_server_stop(server);


    asock_connections_destroy(server->m_new_connections);
    asock_connections_destroy(server->m_connections);

    if (close(server->m_epoll_desc) != 0) {
        SYS_LOG_ERROR("Epoll descriptor closing failed");
    }

    if (server->m_config.m_net_ns_fd != -1) {
        if (close(server->m_config.m_net_ns_fd) == -1) {
            SYS_LOG_ERROR("Failed closing net namespace.");
        }
        server->m_config.m_net_ns_fd = -1;
    }
    if (server->m_config.m_net_ns) {
        free(server->m_config.m_net_ns);
        server->m_config.m_net_ns = NULL;
    }
    free(server);
}


AsockReturnCode asock_server_bind(asock_server_t *server, const struct sockaddr *addr, socklen_t addrlen)
{
    if (server == NULL)
        return ASOCK_ERR_INVAL;

    if (server->m_server_status == Running)
        return ASOCK_ERR_BADSTATE;


    return asock_send_directive(&server->m_directive, DirectBind, &addr, &addrlen, NULL, NULL);

//    struct sockaddr *new_addr = malloc(addrlen);
//    if (new_addr == NULL)
//        return ASOCK_ERR_NOMEM;
//       memcpy(new_addr, addr, addrlen);

//    pthread_mutex_lock(&server->m_directive_mutex);
//    server->m_config.m_serv_addr = new_addr;
//    server->m_config.m_serv_addrlen = addrlen;

//    AsockReturnCode result = asock_server_send_directive(server, DirectBind, ExternLocking);

//    pthread_mutex_unlock(&server->m_directive_mutex);

//    return result;
}



AsockReturnCode asock_server_setns(asock_server_t *server, const char* net_ns)
{
    if (server == NULL)
        return ASOCK_ERR_INVAL;

    if (server->m_server_status == Running)
        return ASOCK_ERR_BADSTATE;

    return asock_send_directive(&server->m_directive, DirectSetns, &net_ns, NULL, NULL, NULL);

//    pthread_mutex_lock(&server->m_directive_mutex);
//    free(server->m_config.m_net_ns);
//    server->m_config.m_net_ns = tmp;

//    AsockReturnCode result = asock_server_send_directive(server, DirectSetns, ExternLocking);

//    pthread_mutex_unlock(&server->m_directive_mutex);

//    return result;
}


AsockReturnCode asock_server_start (asock_server_t *server)
{
    if (server == NULL)
        return ASOCK_ERR_INVAL;

    if (server->m_server_status == Running)
        return ASOCK_ERR_BADSTATE;

    return asock_send_directive(&server->m_directive, DirectStart, NULL, NULL, NULL, NULL);
//    return asock_server_send_directive(server, DirectStart, InternLocking);
}


//AsockReturnCode asock_server_start(asock_server_t *server,
//                       uint16_t port,
//                       const char* net_ns)
//{
//    if (asock_server_is_running(server)) {
//        if (port == server->m_config.m_server_port &&
//                strcmp(net_ns,  server->m_config.m_net_ns) == 0) {
//            return ASOCK_SUCCESS;
//        } else {
//            return ASOCK_FAILURE;
//        }
//    }

//    server->m_config.m_server_port = port;

//    char *tmp = server->m_config.m_net_ns;
//    if (net_ns) {
//        server->m_config.m_net_ns = strdup(net_ns);
//        if (server->m_config.m_net_ns == NULL)
//            return ASOCK_ERR_NOMEM;
//    } else {
//        server->m_config.m_net_ns = NULL;
//    }
//    free(tmp);


//    server->m_server_directive = Continue;
//    server->m_server_status = Configuration;

//    int thread_start_status = pthread_create(&server->m_thread, NULL,
//                                      &asock_server_thread_loop, server);
//    if (thread_start_status != 0) {
//        SYS_LOG_ERROR("Server thread creating failed");
//        free(server->m_config.m_net_ns);
//        server->m_config.m_net_ns = NULL;
//        server->m_server_status = Aborted;
//        return ASOCK_FAILURE;
//    }


//    while (server->m_server_status == Configuration)
//        ;//WAITING

//    if (server->m_server_status == Aborted) {
//        pthread_join(server->m_thread, NULL);
//        return ASOCK_FAILURE;
//    }

//    return ASOCK_SUCCESS;
//}



void asock_server_stop(asock_server_t *server)
{
    if (!asock_server_is_running(server))
        return;

    asock_send_directive(&server->m_directive, DirectStop, NULL, NULL, NULL, NULL);
    pthread_join(server->m_thread, NULL);
    server->m_server_status = Aborted;
    LOG_INFO("Server was stopped");

//    server->m_server_directive = DirectStop;
//    pthread_join(server->m_thread, NULL);
//    server->m_server_status = Configuration;
//    LOG_INFO("Server on port %d was stopped", (int)server->m_config.m_server_port);
}


int asock_server_is_running(const asock_server_t *server)
{
    assert(server);
    return server->m_server_status == Running;
}


asock_t* asock_server_get_incom_connection(asock_server_t *server)
{
    assert(server);

    pthread_mutex_lock(&server->m_new_connections_mutex);
    if (asock_connections_size(server->m_new_connections) == 0) {
        pthread_mutex_unlock(&server->m_new_connections_mutex);
        return NULL;
    }

    asock_t *con = asock_connections_at(server->m_new_connections, 0);
    asock_connections_erase(server->m_new_connections, 0);
    pthread_mutex_unlock(&server->m_new_connections_mutex);
    return con;
}





AsockReturnCode asock_server_process_bind_directive(asock_server_t *server)
{
    assert(server);
    asock_directive_t *direct = &server->m_directive;
    pthread_mutex_lock(&direct->m_direct_mutex);

    const struct sockaddr *addr = *(const struct sockaddr **)(direct->m_arg1);
    socklen_t addr_len = *(socklen_t*)(direct->m_arg2);
    AsockReturnCode result = ASOCK_SUCCESS;

    if (bind(server->m_server_socket, addr, addr_len) == -1) {
        SYS_LOG_ERROR("Bind failed");
        result = ASOCK_FAILURE;
    }

    pthread_mutex_unlock(&direct->m_direct_mutex);
    return result;
}

//AsockReturnCode asock_server_process_setns_directive(asock_server_t *server)
//{
//    assert(server);

//    AsockReturnCode result = ASOCK_SUCCESS;
//    int fd_nms = -1;

//    asock_directive_t *direct = &server->m_directive;
//    pthread_mutex_lock(&direct->m_direct_mutex);

//    /* copy namespace str */
//    const char* net_ns = *(const char**)(direct->m_arg1);
//    char* tmp = strdup(net_ns);
//    if (tmp == NULL) {
//        result = ASOCK_ERR_NOMEM;
//        goto EXIT_SETNS_PROCESS;
//    }
//    free(server->m_config.m_net_ns);
//    server->m_config.m_net_ns = tmp;


//    if (server->m_config.m_net_ns_fd != -1) {
//        if (close(server->m_config.m_net_ns_fd) == -1) {
//            SYS_LOG_ERROR("Failed closing net namespace.");
//        }
//        server->m_config.m_net_ns_fd = -1;
//    }

//    /* changing network namespace */
//    if (server->m_config.m_net_ns && server->m_config.m_net_ns[0] != '\0') {
//        fd_nms = open(server->m_config.m_net_ns, O_RDONLY); /* Get descriptor for namespace */
//        if (fd_nms == -1) {
//            SYS_LOG_ERROR("Failed to open nms file %s", server->m_config.m_net_ns);
//            free(server->m_config.m_net_ns);
//            server->m_config.m_net_ns = NULL;
//            result = ASOCK_FAILURE;
//            goto EXIT_SETNS_PROCESS;
//        }
//        if (setns(fd_nms, CLONE_NEWNET) == -1) {
//            SYS_LOG_ERROR("Failed setns");
//            close(fd_nms);
//            free(server->m_config.m_net_ns);
//            server->m_config.m_net_ns = NULL;
//            result = ASOCK_FAILURE;
//            goto EXIT_SETNS_PROCESS;
//        }
//        LOG_DEBUG("setns success!");
//    }
//    result = ASOCK_SUCCESS;

//EXIT_SETNS_PROCESS:
//    pthread_mutex_unlock(&direct->m_direct_mutex);
//    return result;
//}

AsockReturnCode asock_server_change_namespace(asock_server_t *server)
{
    assert(server);
    assert(server->m_config.m_net_ns);

    int fd_nms = -1;

    /* changing network namespace */
    if (server->m_config.m_net_ns[0] != '\0') {
        fd_nms = open(server->m_config.m_net_ns, O_RDONLY); /* Get descriptor for namespace */
        if (fd_nms == -1) {
            SYS_LOG_ERROR("Failed to open nms file %s", server->m_config.m_net_ns);
            free(server->m_config.m_net_ns);
            server->m_config.m_net_ns = NULL;
            return ASOCK_FAILURE;
        }
        if (setns(fd_nms, CLONE_NEWNET) == -1) {
            SYS_LOG_ERROR("Failed setns");
            close(fd_nms);
            free(server->m_config.m_net_ns);
            server->m_config.m_net_ns = NULL;
            return ASOCK_FAILURE;
        }
        LOG_DEBUG("setns success!");
    }
    server->m_config.m_net_ns_fd = fd_nms;
    return ASOCK_SUCCESS;
}

void* asock_server_thread_loop(void *p_server)//asock_server_t *server);
{
    asock_server_t *server = (asock_server_t *)p_server;

    // basic configuration
    if (server->m_config.m_net_ns != NULL) {
        asock_server_change_namespace(server);
    }

    int sock_d;
    sock_d = socket(server->m_config.m_domain,
                    server->m_config.m_type,
                    server->m_config.m_protocol);
    if (sock_d == -1) {
        LOG_WARNING("Failed to create socket");
        server->m_server_status = Aborted;
        return NULL;
    }

    int enable = 1;
    if (setsockopt(sock_d, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        close(sock_d);
        LOG_WARNING("Setsockopt for  SO_REUSEADDR for server failed\n");
        server->m_server_status = Aborted;
        return NULL;
    }

    server->m_server_socket = sock_d;
    server->m_server_status = Configuration;



    // continue configuration
    int continue_configuration = 1;
    while (continue_configuration) {
        asock_wait_for_directive(&server->m_directive);

        enum DirectiveType type = asock_get_directive_type(&server->m_directive);
        switch (type) {
            case DirectStart:
                continue_configuration = 0;
                break;
            case DirectStop:
                server->m_server_status = Aborted;
                asock_set_directive_result(&server->m_directive, ASOCK_SUCCESS);
                return NULL;

            case DirectBind:
                asock_set_directive_result(&server->m_directive,
                                           asock_server_process_bind_directive(server));
                break;
//            case DirectSetns:
//                asock_set_directive_result(&server->m_directive,
//                                           asock_server_process_setns_directive(server));
//                break;

            case DirectSetns:
            case DirectNone:
            default:
                LOG_ERROR("Undefined Directive type in server thread loop");
                assert(0);
                asock_set_directive_result(&server->m_directive, ASOCK_FAILURE);
                return NULL;
                break;
        }
    }

//    if (server->m_server_directive == DirectStop)
//        return NULL;

//    int fd_nms = -1;
//    if (server->m_config.m_net_ns && server->m_config.m_net_ns[0] != '\0') {
//        fd_nms = open(server->m_config.m_net_ns, O_RDONLY); /* Get descriptor for namespace */
//        if (fd_nms == -1) {
//            SYS_LOG_ERROR("Failed to open nms file %s", server->m_config.m_net_ns);
//            server->m_server_status = Aborted;
//            return NULL;
//        }
//        if (setns(fd_nms, CLONE_NEWNET) == -1) {
//            SYS_LOG_ERROR("Failed setns");
//            close(fd_nms);
//            server->m_server_status = Aborted;
//            return NULL;
//        }
//    }

    // starting server
    if (asock_server_start_server(server) == ASOCK_FAILURE) {
//        LOG_WARNING("Failed to start server on port %d\n", (int)server->m_config.m_server_port);
        if (server->m_config.m_net_ns && server->m_config.m_net_ns[0] != '\0') {
//            if (close(fd_nms) == -1) {
//                SYS_LOG_ERROR("Failed closing net namespace.");
//            }
        }
        server->m_server_status = Aborted;
        asock_set_directive_result(&server->m_directive, ASOCK_FAILURE);
        return NULL;
    }
    server->m_server_status = Running;
    asock_set_directive_result(&server->m_directive, ASOCK_SUCCESS);
//    LOG_INFO("Server is running on port %d\n", (int)server->m_config.m_server_port);


    // main loop
    while (1) {
        enum DirectiveType type = asock_get_directive_type(&server->m_directive);
        int stop_is_needed = 0;
        switch (type) {
            case DirectNone:
                break;
            case DirectStop:
                stop_is_needed = 1;
                break;

            case DirectStart:
            case DirectBind:
            case DirectSetns:
                asock_set_directive_result(&server->m_directive, ASOCK_ERR_BADSTATE);
                //todo
                break;

            default:
                LOG_ERROR("Undefined Directive type in server thread loop");
                assert(0);
                asock_set_directive_result(&server->m_directive, ASOCK_FAILURE);
                return NULL;
                break;
        }

        if (stop_is_needed) {
            break;
        }
        asock_server_update_state(server);
    }


    // Closing client connections
//    pthread_mutex_lock(&server->m_new_connections_mutex);
//    size_t sz = asock_connections_size(server->m_new_connections);
//    for (size_t i = sz - 1; i < sz; --i) {
//        asock_t *con = asock_connections_at(server->m_new_connections, i);
//        asock_destroy(con);
//    }
//    pthread_mutex_unlock(&server->m_new_connections_mutex);


    size_t sz = asock_connections_size(server->m_connections);
    for (size_t i = sz - 1; i < sz; --i) {
        asock_t *con = asock_connections_at(server->m_connections, i);
        asock_destroy_asock(con);
    }

    if (close(server->m_server_socket) == -1) {
        SYS_LOG_ERROR("Failed server socket closing");
    }

    if (server->m_config.m_net_ns_fd != -1) {
        if (close(server->m_config.m_net_ns_fd) == -1) {
            SYS_LOG_ERROR("Failed closing net namespace.");
        }
        server->m_config.m_net_ns_fd = -1;
    }
    if (server->m_config.m_net_ns) {
        free(server->m_config.m_net_ns);
        server->m_config.m_net_ns = NULL;
    }

    // sending result for stop directive
    asock_set_directive_result(&server->m_directive, ASOCK_SUCCESS);

    return NULL;
}

void asock_server_update_state(asock_server_t *server)
{
    asock_server_send_unsent_data(server);
    asock_server_check_for_connections_to_close(server);

    int n = epoll_wait(server->m_epoll_desc, server->m_events, MAX_EVENTS, EPOLL_WAIT_DEFAULT_TIMEOUT);
    if (n == -1)
        SYS_LOG_ERROR("epoll_wait failed");
    for (int i = 0; i < n; ++i) {

        if ((server->m_events[i].events & EPOLLERR)
             || (server->m_events[i].events & EPOLLHUP)
             || (!(server->m_events[i].events & EPOLLIN))) {
            SYS_LOG_ERROR("epoll EPOLLERR");
            asock_server_close_connection(server, server->m_events[i].data.fd);
            continue;
        } else if (server->m_server_socket == server->m_events[i].data.fd) {
             /* More incoming connections. */
            while (1) {
                struct sockaddr in_addr;
                socklen_t in_len = sizeof in_addr;
                int infd;
                infd = accept(server->m_server_socket, &in_addr, &in_len);
                if (infd == -1) {
                    if ((errno == EAGAIN) ||
                          (errno == EWOULDBLOCK)) {
                          /* We have processed all incoming  connections.*/
                        break;
                    } else {
                        perror ("accept");
                        break;
                    }
                }

                struct sockaddr_in *addr_in = (struct sockaddr_in*)&in_addr;
//                uint32_t peer_addr = addr_in->sin_addr.s_addr;
                uint16_t peer_port = addr_in->sin_port;

                LOG_INFO("Accepted connection on descriptor %d (host=%s, port=%d)\n",
                    infd, inet_ntoa(addr_in->sin_addr), (int)peer_port);

                if (make_socket_non_blocking(infd) == -1)
                    abort();

                if (add_socket_to_epoll_server(server->m_epoll_desc, infd) == -1)
                    abort();



                asock_t *new_con = (asock_t*)asock_fcreate_asock(infd);
                if (new_con == NULL)
                    goto CLEARING_AFTER_FAILURE;

//                new_con->m_sock_desc = infd;
                new_con->m_master_id = server->m_server_id;
                new_con->m_state = Established;

                new_con->m_peer_addr = malloc(in_len);
                if (new_con->m_peer_addr == NULL) {
                    SYS_LOG_ERROR("Failed to allocate memory for asock address");
                    goto CLEARING_AFTER_FAILURE;
                }
                memcpy(new_con->m_peer_addr, &in_addr, in_len);
                new_con->m_peer_addrlen = in_len;

                if (asock_connections_push(server->m_connections, new_con) == ASOCK_FAILURE) {
                    SYS_LOG_ERROR("asock_connections_push failed");
                    goto CLEARING_AFTER_FAILURE;
                }

                pthread_mutex_lock(&server->m_new_connections_mutex);
                if (asock_connections_push(server->m_new_connections, new_con) ==  ASOCK_FAILURE) {
                    SYS_LOG_ERROR("asock_connections_push failed");
                    size_t sz = asock_connections_size(server->m_connections);
                    asock_connections_erase(server->m_connections, sz - 1);
                    goto CLEARING_AFTER_FAILURE;
                }
                pthread_mutex_unlock(&server->m_new_connections_mutex);

                asock_con_counter_incr(new_con);
                goto SUCCESS;


                CLEARING_AFTER_FAILURE:
                    asock_destroy_asock(new_con);
                    close(infd);

                SUCCESS:
                    ;

            }
            continue;
        } else {
            /* We have data on the fd waiting to be read. Read and
            display it. We must read whatever data is available
            completely, as we are running in edge-triggered mode
            and won't get a notification again for the same
            data. */
            while (1) {
                ssize_t count;
                const int BUF_SIZE = 4096;
                uint8_t buf[BUF_SIZE];

                count = read(server->m_events[i].data.fd, buf, sizeof buf);
                if (count == -1) {
                    if (errno != EAGAIN)  //errno == EAGAIN means we read all data
                        asock_server_close_connection(server, server->m_events[i].data.fd);
                    break;
                } else if (count == 0) { /* End of file. The remote has closed the connection. */
                    asock_server_close_connection(server, server->m_events[i].data.fd);
                    break;
                }

                ssize_t index = asock_server_index_of_con_by_sock_desc(server, server->m_events[i].data.fd);
                if (index != -1) {
                    asock_t *con = asock_connections_at(server->m_connections, index);
                    asock_tsqueue_enqueue(con->m_in_queue, buf, count);
                }
            }
        }
    } /* for */
}



AsockReturnCode asock_server_close_connection(asock_server_t *server, int sockd)
{
    assert(server);
    assert(sockd >= 0);

    pthread_mutex_lock(&server->m_new_connections_mutex);
    size_t new_cons_sz = asock_connections_size(server->m_new_connections);
    for (size_t i = 0; i < new_cons_sz; ++i) {
        asock_t *con = asock_connections_at(server->m_new_connections, i);
        if (con->m_sock_desc == sockd) {
            asock_connections_erase(server->m_new_connections, i);
            break;
        }
    }
    pthread_mutex_unlock(&server->m_new_connections_mutex);


    ssize_t index = asock_server_index_of_con_by_sock_desc(server, sockd);
    if (index == -1)
        return ASOCK_ERR_INVAL;

    asock_t *con = asock_connections_at(server->m_connections, index);
    con->m_state = Failed;

    /* Closing the descriptor will make epoll remove it
       from the set of descriptors which are monitored. */
    if (close(sockd) == -1)
        SYS_LOG_ERROR("socket close failed");
    asock_destroy_asock(con);
    asock_connections_erase(server->m_connections, index);
    return ASOCK_SUCCESS;

}

ssize_t asock_server_index_of_con(asock_server_t *server,
                                  const asock_t *connection)
{
    if (connection->m_master_id != server->m_server_id)
        return -1;

    size_t cons_sz = asock_connections_size(server->m_connections);
    for (size_t i = 0; i < cons_sz; ++i) {
        asock_t *con = asock_connections_at(server->m_connections, i);
        if (connection->m_connection_id == con->m_connection_id) {
            return i;
        }
    }
    return -1;
}

ssize_t asock_server_index_of_con_by_sock_desc(asock_server_t *server,
                                               int sockd)
{
    size_t cons_sz = asock_connections_size(server->m_connections);
    for (size_t i = 0; i < cons_sz; ++i) {
        asock_t *con = asock_connections_at(server->m_connections, i);
        if (con->m_sock_desc == sockd) {
            return i;
        }
    }
    return -1;
}

AsockReturnCode asock_server_start_server(asock_server_t *server)
{
    assert(server);

    if (make_socket_non_blocking(server->m_server_socket) != ASOCK_SUCCESS)
        return ASOCK_FAILURE;

    if (listen(server->m_server_socket, SOMAXCONN) == -1)
        return ASOCK_FAILURE;

    if (add_socket_to_epoll_server(server->m_epoll_desc, server->m_server_socket) == -1)
        return ASOCK_FAILURE;

    return ASOCK_SUCCESS;

//    char port_string[21] = {'\0'};
//    snprintf(port_string, 20, "%d", (int)server->m_config.m_server_port);
//    if ((server->m_server_socket = create_and_bind(port_string)) == -1)
//        return ASOCK_FAILURE;

//    int sock = create_and_bind(server->m_config.m_domain,
//                               server->m_config.m_type,
//                               server->m_config.m_protocol,
//                               server->m_config.m_serv_addr,
//                               server->m_config.m_serv_addrlen);
//    if (sock == -1)
//        return ASOCK_FAILURE;

//    if (make_socket_non_blocking(sock) != ASOCK_SUCCESS) {
//        if (close(sock) == -1)
//            SYS_LOG_ERROR("Closing socket %d failed", (int)sock);
//        return ASOCK_FAILURE;
//    }

//    if (listen(sock, SOMAXCONN) == -1) {
//        SYS_LOG_ERROR("listen for server socket %d failed", (int)sock);
//        if (close(sock) == -1)
//            SYS_LOG_ERROR("Closing socket %d failed", (int)sock);
//        return ASOCK_FAILURE;
//    }

//    if (add_socket_to_epoll_server(server->m_epoll_desc, sock) == -1) {
//        if (close(sock) == -1)
//            SYS_LOG_ERROR("Closing socket %d failed", (int)sock);
//        return ASOCK_FAILURE;
//    }

//    server->m_server_socket = sock;

//    return ASOCK_SUCCESS;
}


void asock_server_send_unsent_data(asock_server_t *server)
{
    size_t con_sz = asock_connections_size(server->m_connections);
    /* starting from the end,  because while writing some connections
       may be closed and deleted from m_connections
    */
    for (size_t i = con_sz - 1; i < con_sz; --i) {

        asock_t *con = asock_connections_at(server->m_connections, i);
        assert(con);

        asock_tsqueue_head_lock(con->m_out_queue);
        size_t out_data_sz = asock_tsqueue_head_size(con->m_out_queue);
        if (out_data_sz != 0) {
            uint8_t *data_to_send = asock_tsqueue_head(con->m_out_queue);
            assert(data_to_send);

            ssize_t send_data_sz = asock_server_try_send_data(server, con, data_to_send, out_data_sz);
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


ssize_t asock_server_try_send_data(asock_server_t *server,
                                   const asock_t *con,
                                   const uint8_t *data,
                                   uint32_t data_size)
{
    if (!asock_server_is_running(server))
        return -1;


    int index = asock_server_index_of_con(server, con);
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
                    asock_server_close_connection(server, con->m_sock_desc);
                    return -1;
                }
                return n;
            }
    }

    return -1;
}

void asock_server_check_for_connections_to_close(asock_server_t *server)
{
    size_t sz = asock_connections_size(server->m_connections);
    for (size_t i = sz - 1; i < sz; --i) {
        asock_t *con = asock_connections_at(server->m_connections, i);
        if (con->m_disconnect) {
            asock_server_close_connection(server, con->m_sock_desc);
        }
    }
}


//AsockReturnCode asock_server_send_directive(asock_server_t *server,
//                                            enum DirectiveType directive,
//                                            enum FunctionLocking locking)
//{
//    assert(server);

//    AsockReturnCode result = ASOCK_UNDEFINED_CODE;

//    if (locking == InternLocking)
//        pthread_mutex_lock(&server->m_directive_mutex);

//    server->m_directive_result = ASOCK_UNDEFINED_CODE;
//    server->m_server_directive = directive;
//    pthread_cond_signal(&server->m_directive_condition);

//    while (server->m_directive_result == ASOCK_UNDEFINED_CODE)
//        pthread_cond_wait(&server->m_directive_result_condition, &server->m_directive_mutex);

//    result = server->m_directive_result;

//    if (locking == InternLocking)
//        pthread_mutex_unlock(&server->m_directive_mutex);
//    return result;
//}





