/*
  _ _ _                          _
 | (_) |                        | |
 | |_| |__   __ _ ___  ___   ___| | __
 | | | '_ \ / _` / __|/ _ \ / __| |/ /
 | | | |_) | (_| \__ \ (_) | (__|   <
 |_|_|_.__/ \__,_|___/\___/ \___|_|\_\

libasock

Version 0.1.0
https://github.com/seleznevae/libasock

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
Copyright (c) 2017 Anton Seleznev <seleznevae@protonmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#ifndef ASOCK_H
#define ASOCK_H

#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>


#ifdef __cplusplus
extern "C"
{
#endif


typedef int AsockReturnCode;

/* Possible values of AsockReturnCode */
#define ASOCK_SUCCESS        0
#define ASOCK_ERR_NOMEM      1    /* No space */
#define ASOCK_ERR_INVAL      2    /* Invalid argument */
#define ASOCK_ERR_BADSTATE   2    /* Invalid object state for operation */
#define ASOCK_FAILURE        666
#define ASOCK_UNDEFINED_CODE 1666

#define ASOCK_IS_SUCCESS(arg) ((arg) == ASOCK_SUCCESS)




/* Asock lib */
AsockReturnCode asock_init();
void asock_deinit();


/*  Asock */

struct asock;
typedef struct asock asock_t;

enum AsockConState {
    Idle,
    InProgress,
    Established,
    Failed
};

/*
 Possible changes in state of asock connection state

    Idle ---> InProgress ---> Established
     |            |             |
     |            |             |
     |            v             |
     -------->  Failed  <--------
*/

asock_t * asock_create(int domain, int type, int protocol);
asock_t * asock_create_ns(int domain, int type, int protocol, const char* net_ns);

/* todo: make destroy variant so that it will wait until all data in socket will be passed to the reciever */
void asock_destroy(asock_t *con);
ssize_t asock_write(asock_t *con, const void *src, size_t src_sz);
ssize_t asock_read(asock_t *con, void *dst, size_t dst_sz);
AsockReturnCode asock_connect(asock_t *con, const struct sockaddr *addr, socklen_t addrlen);
AsockReturnCode asock_bind(asock_t *con, const struct sockaddr *addr, socklen_t addrlen);

enum AsockConState asock_state(asock_t *con);






/* Asock server */

struct asock_server;
typedef struct asock_server asock_server_t;



asock_server_t* asock_server_create (int domain, int type, int protocol);
asock_server_t* asock_server_create_ns(int domain, int type, int protocol, const char* net_ns);
void            asock_server_destroy(asock_server_t*);

AsockReturnCode asock_server_bind(asock_server_t *server, const struct sockaddr *addr, socklen_t addrlen);

AsockReturnCode asock_server_start(asock_server_t *server);
void  asock_server_stop (asock_server_t *server);
int   asock_server_is_running(const asock_server_t *server);

asock_t* asock_server_get_incom_connection(asock_server_t *server);











#ifdef __cplusplus
}
#endif

#endif // ASOCK_H
