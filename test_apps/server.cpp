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
        printf("asock_server --port PortNumber --bytes-to-transmit BytesToTransmit --bytes-to-recieve BytesToRecieve [--setns NamespacePath] \n");
        return 1;
    }

    uint16_t port = 0;
    uint64_t bytesOut = 0;
    uint64_t bytesIn = 0;
    std::string nms;

    for (int i = 1; i < argc; ++i) {
        if (argv[i] == std::string("--port") && i + 1 < argc) {
            i++;
            port = std::stoi(argv[i]);
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

    asock_server_t *server = NULL;
    if (nms == "")
        server = asock_server_create(AF_INET, SOCK_STREAM, 0);
    else
        server = asock_server_create_ns(AF_INET, SOCK_STREAM, 0, argv[2]);
    if (server == NULL) {
        asock_deinit();
        printf("Failed to create asock server\n");
        return 3;
    }

    struct sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_addr.s_addr = htonl(INADDR_ANY);
    sa.sin_port = htons(port);
    int bind_status = asock_server_bind(server, (struct sockaddr*)&sa, sizeof(struct sockaddr));
    if (bind_status != ASOCK_SUCCESS) {
        asock_server_destroy(server);
        asock_deinit();
        printf("Failed to bind server to %d port\n", (int)port);
        return 5;
    }

    int start_status = asock_server_listen(server);
    if (start_status != ASOCK_SUCCESS) {
        asock_server_destroy(server);
        asock_deinit();
        printf("Failed to start server\n");
        return 6;
    }

    asock_t *incom_con = NULL;
    while (true) {
        incom_con = asock_server_accept(server);
        if (incom_con == NULL)
            sleep(1);
        else
            break;
    }

    sleep(5); //debug
    fprintf(stderr, "Beginning to write\n");
    while (bytesOut > 0) {
        unsigned char out[256];
        int count = std::min(bytesOut, sizeof(out));
        for (int i = 0; i < count; ++i) {
            out[i] = i;
        }
        if (asock_write(incom_con, out, count) != count) {
            printf("Write failed\n");
            return 7;
        }
        bytesOut -= count;
        usleep(100);
    }

    fprintf(stderr, "Beginning to read\n");

    uint64_t bytesInLeft = bytesIn;
    while (bytesInLeft > 0) {
        unsigned char in[256];
        int count = std::min(bytesInLeft, sizeof(in));
        int countRead = asock_read(incom_con, in, count);
        if (countRead == -1) {
            printf("Read failed\n");
            return 8;
        }
        for (int i = 0; i < countRead; ++i) {
            if (in[i] != ((unsigned char)(bytesIn - bytesInLeft) + (unsigned char)i)) {
                printf("Incorrect values rcv\n");
                return 8;
            }
        }
        bytesInLeft -= countRead;
        usleep(100);
    }

    asock_destroy(incom_con);
    asock_server_destroy(server);
    asock_deinit();
    return 0;
}
