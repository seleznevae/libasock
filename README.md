# libasock (WIP)
[![Build Status](https://travis-ci.org/seleznevae/libasock.svg?branch=master)](https://travis-ci.org/seleznevae/libasock)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Simple library for asynchronous sockets in linux.

# Examples
```
/* Client example */
#include "asock.h"

#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

int main(int argc, char **argv)
{
    asock_init();
    
    asock_t *client = asock_create(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(8888);
    asock_connect(client, (struct sockaddr*)&sa, sizeof(struct sockaddr));


    if (asock_state(client) == Established) {
      const char *message = "Hello, server!";
      asock_write(client, message, sizeof(message) + 1);
    }

    while (asock_state(client) == Established) {
      char c;
      if (asock_read(client, &c, 1) > 0) {
        putchar(c);
      }
    }


    asock_destroy(client);
    
    asock_deinit();
    return 0;
}
```

```
/* Server example */

#include "asock.h"


#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

int main(int argc, char **argv)
{
    asock_init();
    
    asock_server_t *server = asock_server_create(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(8888);
    asock_server_bind(server, (struct sockaddr*)&sa, sizeof(struct sockaddr));

    asock_server_start(server);

    asock_t *connection = NULL;
    while (true) {
        connection = asock_server_get_incom_connection(server);
        if (connection == NULL)
            sleep(1);
        else
            break;
    }

    char incom_message[64];
    

    asock_destroy(connection);
    asock_server_destroy(server);
    
    asock_deinit();
    return 0;
}


```
