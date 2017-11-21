/* Client example */
#include "asock.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

int main()
{
    asock_init();

    asock_t *client = asock_create(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(8888);
    asock_connect(client, (struct sockaddr*)&sa, sizeof(struct sockaddr));

    while (asock_state(client) == InProgress)
        sleep(1);

    if (asock_state(client) == Established) {
        const char *message = "Hello, server!\n";
        asock_write(client, message, strlen(message));

        char c;
        while (1) {
            if (asock_read(client, &c, sizeof(c)) > 0)
                fprintf(stderr, "%c", c);
        }
    }

    asock_destroy(client);

    asock_deinit();
    return 0;
}
