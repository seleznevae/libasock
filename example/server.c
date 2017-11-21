/* Server example */
#include "asock.h"

#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

int main()
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
    while (1) {
        connection = asock_server_get_incom_connection(server);
        if (connection == NULL)
            sleep(1);
        else
            break;
    }

    while (asock_state(connection) == InProgress)
        sleep(1);

    if (asock_state(connection) == Established) {
        const char *message = "Hello, client!\n";
        asock_write(connection, message, strlen(message));

        sleep(1);
        while (1) {
            char c;
            if (asock_read(connection, &c, sizeof(c) > 0))
                fprintf(stderr, "%c", c);
        }
    }

    asock_destroy(connection);
    asock_server_destroy(server);

    asock_deinit();
    return 0;
}
