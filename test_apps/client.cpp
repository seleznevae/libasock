#include "asock.h"
#include <stdio.h>
#include <string>
#include <unistd.h>
#include <iostream>


#include <arpa/inet.h>
#include <netinet/tcp.h>

int main(int argc, char **argv)
{
    if (argc < 5) {
        printf("Incorrect usage\n");
        printf("asock_client --port PortNumber --server-address Address--bytes-to-transmit BytesToTransmit --bytes-to-recieve BytesToRecieve [--setns NamespacePath] \n");
        return 1;
    }

    uint16_t port = 0;
    uint64_t bytesOut = 0;
    uint64_t bytesIn = 0;
    std::string nms;

    in_addr_t server_addr = htonl(INADDR_ANY);

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == std::string("--port") && i + 1 < argc) {
            i++;
            port = std::stoi(argv[i]);
        } else if (argv[i] == std::string("--server-address") && i + 1 < argc) {
            i++;
            server_addr = inet_addr(argv[i]);
        } else if (argv[i] == std::string("--bytes-to-transmit") && i + 1 < argc) {
            i++;
            bytesOut = std::stoll(argv[i]);
        } else if (argv[i] == std::string("--bytes-to-recieve") && i + 1 < argc) {
            i++;
            bytesIn = std::stoll(argv[i]);
        } else if (argv[i] == std::string("--setns") && i + 1 < argc) {
            i++;
            nms = argv[i];
        }
    }


    if (asock_init() != ASOCK_SUCCESS) {
        printf("Failed to init asock lib\n");
        return 2;
    }

    asock_t *client = NULL;
    if (nms == "")
        client = asock_create(AF_INET, SOCK_STREAM, 0);
    else
        client = asock_create_ns(AF_INET, SOCK_STREAM, 0, argv[2]);
    if (client == NULL) {
        asock_deinit();
        printf("Failed to create asock\n");
        return 3;
    }

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = server_addr;
    sa.sin_port = htons(port);

    int bind_status = asock_connect(client, (struct sockaddr*)&sa, sizeof(struct sockaddr));
    if (bind_status != ASOCK_SUCCESS) {
        asock_destroy(client);
        asock_deinit();
        printf("Failed to connect to server\n");
        return 5;
    }

    while (asock_state(client) == InProgress)
        sleep(1);

    if (asock_state(client) == Failed){
        asock_destroy(client);
        asock_deinit();
        printf("Connection failed\n");
        return 6;
    }


    sleep(2);
    asock_destroy(client);
    asock_deinit();
    return 0;

    while (bytesOut > 0) {
        unsigned char out[256];
        int count = std::min(bytesOut, sizeof(out));
        for (int i = 0; i < count; ++i) {
            out[i] = i;
        }
        if (asock_write(client, out, count) != count) {
            printf("Write failed\n");
            return 7;
        }
        bytesOut -= count;
    }

    uint64_t bytesInLeft = bytesIn;
    while (bytesInLeft > 0) {
        unsigned char in[256];
        int count = std::min(bytesInLeft, sizeof(in));
        int countRead = asock_read(client, in, count);
        if (countRead == -1) {
            printf("Read failed\n");
            return 8;
        }
        for (int i = 0; i < countRead; ++i) {
            if (in[i] != ((unsigned char)(bytesIn - bytesInLeft) + (unsigned char)i)) {
                printf("Incorrect value (# %d) rcv\n", (int)((bytesIn - bytesInLeft) + i));
                return 8;
            }
        }
        bytesInLeft -= countRead;
    }

    sleep(1);

    asock_destroy(client);
    asock_deinit();
    printf("All data were sucessfully transmitted\n");
    return 0;
}
