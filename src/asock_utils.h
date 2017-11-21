#ifndef ASOCK_UTILS_H
#define ASOCK_UTILS_H


#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>


int make_socket_non_blocking(int sfd);
int add_socket_to_epoll(int epolld, int sfd);
int add_socket_to_epoll_server(int epolld, int sfd);
int bind_socket_to_ip_address(int *sock_d, uint32_t host_order_ip_address);
/*int create_and_bind(const char *port); */
int create_and_bind(int domain, int type, int protocol, const struct sockaddr *addr, socklen_t addrlen);





#endif // ASOCK_UTILS_H
