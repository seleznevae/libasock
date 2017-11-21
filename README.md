# libasock (WIP)
[![Build Status](https://travis-ci.org/seleznevae/libasock.svg?branch=master)](https://travis-ci.org/seleznevae/libasock)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)

Simple library for asynchronous sockets in linux.

# Examples
```
/* Client example */
#include "asock.h"

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
