#include "asock_utils.h"


#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>

#include "asock_logger.h"
#include "asock_defs.h"
#include "asock.h"

inline int make_socket_non_blocking(int sfd)
{
    int flags, s;

    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1) {
        SYS_LOG_ERROR("fcntl failed during getting socket %d flags", sfd);
        return ASOCK_FAILURE;
    }

    flags |= O_NONBLOCK;
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1) {
        SYS_LOG_ERROR("fcntl failed during making socker %d non blocking", sfd);
        return ASOCK_FAILURE;
    }

    return ASOCK_SUCCESS;
}

inline int add_socket_to_epoll(int epolld, int sfd)
{
    struct epoll_event event;
    memset(&event, '\0', sizeof(event));
    event.data.fd = sfd;
    event.events = EPOLLOUT | EPOLLIN  | EPOLLET;
    int status = epoll_ctl(epolld, EPOLL_CTL_ADD, sfd, &event);
    if (status == -1) {
        SYS_LOG_ERROR("epoll_ctl failed (epoll %d, sfd %d)", epolld, sfd);
        return -1;
    }
    return 0;
}

inline int add_socket_to_epoll_server(int epolld, int sfd)
{
    struct epoll_event event;
    memset(&event, '\0', sizeof(event));
    event.data.fd = sfd;
    event.events = EPOLLIN  | EPOLLET;
    int status = epoll_ctl(epolld, EPOLL_CTL_ADD, sfd, &event);
    if (status == -1) {
        SYS_LOG_ERROR("epoll_ctl failed (epoll %d, sfd %d)", epolld, sfd);
        return -1;
    }
    return 0;
}

int bind_socket_to_ip_address(int *sock_d, uint32_t host_order_ip_address)
{
    struct sockaddr_in localaddr;
    localaddr.sin_family      = AF_INET;
    localaddr.sin_addr.s_addr = htonl(host_order_ip_address);
    localaddr.sin_port        = 0;  /* Any local port will do */
    if (bind(*sock_d, (struct sockaddr *)&localaddr, sizeof(localaddr)) == 0)
        return ASOCK_SUCCESS;
    else
        return ASOCK_FAILURE;
}



//int create_and_bind(const char*port)
//{
//    struct addrinfo hints;
//    struct addrinfo *addr_result, *rp;
//    int status;
//    int sock_d;

//    memset (&hints, 0, sizeof (struct addrinfo));
//    hints.ai_family   = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
//    hints.ai_socktype = SOCK_STREAM;   /* TCP socket */
//    hints.ai_flags    = AI_PASSIVE;

//    status = getaddrinfo (NULL, port, &hints, &addr_result);
//    if (status != 0) {
//        LOG_WARNING("getaddrinfo error: %s\n", gai_strerror(status));
//        return -1;
//    }

//    for (rp = addr_result; rp != NULL; rp = rp->ai_next) {
//        sock_d = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
//        if (sock_d == -1)
//            continue;

//        int enable = 1;
//        if (setsockopt(sock_d, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0)
//            LOG_WARNING("setsockopt for  SO_REUSEADDR failed\n");

//        status = bind(sock_d, rp->ai_addr, rp->ai_addrlen);
//        if (status == 0)  /* We managed to bind successfully! */
//            break;
//        close(sock_d);
//    }

//    freeaddrinfo(addr_result);
//    if (rp == NULL) {
//        SYS_LOG_ERROR("Failed to bound socket to port %s\n", port);
//        return -1;
//    }
//    return sock_d;
//}

int create_and_bind(int domain, int type, int protocol, const struct sockaddr *addr, socklen_t addrlen)
{
    int sock_d;

    sock_d = socket (domain, type, protocol);
    if (sock_d == -1) {
        LOG_WARNING("Failed to create socket");
        return -1;
    }

    int enable = 1;
    if (setsockopt(sock_d, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable)) < 0) {
        LOG_WARNING("setsockopt for  SO_REUSEADDR failed\n");
        close(sock_d);
        return -1;
    }

    if (bind(sock_d, addr, addrlen) == -1) {
        LOG_WARNING("Bind failed");
        close(sock_d);
        return -1;
    }

    return sock_d;
}
