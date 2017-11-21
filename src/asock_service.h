#ifndef ASOCK_SERVICE_H
#define ASOCK_SERVICE_H








#ifdef __cplusplus
extern "C"
{
#endif

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "asock_defs.h"
#include "asock_impl.h"

struct asock_service;
typedef struct asock_service asock_service_t;





asock_service_t* asock_service_create();
asock_service_t* asock_service_create_ns(const char *net_ns);
void             asock_service_destroy(asock_service_t*);

AsockReturnCode  asock_service_start(asock_service_t *service);
void  asock_service_stop (asock_service_t *service);
int   asock_service_is_running(const asock_service_t *service);

//AsockReturnCode asock_service_setns(asock_service_t *server, const char* net_ns);

/*
asock_t* asock_service_connect(asock_service_t *service,
                               const struct sockaddr *addr,
                               socklen_t addrlen);
*/

asock_t * asock_service_create_asock(asock_service_t *service, int domain, int type, int protocol);


AsockReturnCode   asock_service_connect(asock_service_t *service,
                                        asock_t *socket,
                                        const struct sockaddr *addr,
                                        socklen_t addrlen);



#ifdef __cplusplus
}
#endif






#endif /* ASOCK_SERVICE_H */
