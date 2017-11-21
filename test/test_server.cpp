#include "third_party/Catch/catch.hpp"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include "asock.h"
#include "test_helpers.h"
#include "asock_logger.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <iostream>

TEST_CASE( "Test asock_server starting", "[asock_server_basic]" )
{
    asock_server_t *server1 = asock_server_create(AF_INET, SOCK_STREAM, 0);
    asock_server_t *server2 = asock_server_create(AF_INET, SOCK_STREAM, 0);

    REQUIRE (server1 != NULL);
    REQUIRE (server2 != NULL);

    WHEN("Starting server then it should start successfully") {
        uint16_t port = 13000;
        int start_status = ASOCK_FAILURE;
        int bind_status;
        while (start_status != ASOCK_SUCCESS && port < 13020) {
            port++;
            struct sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = htonl(INADDR_ANY);
            sa.sin_port = htons(port);
            bind_status = asock_server_bind(server1, (struct sockaddr*)&sa, sizeof(struct sockaddr));
            if (bind_status != ASOCK_SUCCESS) {
                continue;
            }
            start_status = asock_server_start(server1);

        }
        REQUIRE(bind_status == ASOCK_SUCCESS);
        REQUIRE(start_status == ASOCK_SUCCESS);

        sleep(1);

        WHEN("Binding one more server on the same port should fail") {
            struct sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = htonl(INADDR_ANY);
            sa.sin_port = htons(port);
            bind_status = asock_server_bind(server1, (struct sockaddr*)&sa, sizeof(struct sockaddr));
            REQUIRE(bind_status != ASOCK_FAILURE);
        }

        WHEN("Starting one more server on another port, should succeed") {
            start_status = ASOCK_FAILURE;
            while (start_status != ASOCK_SUCCESS && port < 13020) {
                port++;
                struct sockaddr_in sa;
                sa.sin_family = AF_INET;
                sa.sin_addr.s_addr = htonl(INADDR_ANY);
                sa.sin_port = htons(port);
                bind_status = asock_server_bind(server2, (struct sockaddr*)&sa, sizeof(struct sockaddr));
                if (bind_status != ASOCK_SUCCESS) {
                    continue;
                }
                start_status = asock_server_start(server2);
            }
            REQUIRE(bind_status == ASOCK_SUCCESS);
            REQUIRE(start_status == ASOCK_SUCCESS);
        }
    }

    asock_server_stop(server2);
    asock_server_stop(server1);

    asock_server_destroy(server2);
    asock_server_destroy(server1);
}


TEST_CASE( "Test basic connection to server", "[asock_server]" )
{
    WHEN("Having asock server") {
        asock_server_t *server = asock_server_create(AF_INET, SOCK_STREAM, 0);

        REQUIRE (server != NULL);

        uint16_t port = 13000;
        int start_status = ASOCK_FAILURE;
        int bind_status;
        while (start_status != ASOCK_SUCCESS && port < 13020) {
            port++;
            struct sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = htonl(INADDR_ANY);
            sa.sin_port = htons(port);
            bind_status = asock_server_bind(server, (struct sockaddr*)&sa, sizeof(struct sockaddr));
            if (bind_status != ASOCK_SUCCESS) {
                continue;
            }
            start_status = asock_server_start(server);
        }
        REQUIRE(bind_status == ASOCK_SUCCESS);
        REQUIRE(start_status == ASOCK_SUCCESS);


        WHEN("Connecting to it") {
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            REQUIRE(fd != -1);

            usleep(1000);
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

            int con_stat = connect(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
            REQUIRE(con_stat != -1);

            asock_t *connection = NULL;
            DO_100_TIMES_WHILE(
                connection = asock_server_get_incom_connection(server), /* action */
                connection == NULL /* condition */
            );
            REQUIRE(connection != NULL);

            THEN("Server and client can send messages to each other") {

                std::vector<char> snd_message;
                std::vector<char> rcv_message;

                // Client sends message to server
                const char *message = "O Captain! my Captain! our fearful trip is done.";
                int mes_len = strlen(message) + 1;
                snd_message = std::vector<char>(message, message + mes_len);
                REQUIRE(write(fd, message, mes_len) == (mes_len));

                // Server recieves message from client
                const int BUF_SZ = 128;
                char buf[BUF_SZ];
                while (rcv_message.size() < snd_message.size()) {
                    usleep(1000);
                    ssize_t read_sz = asock_read(connection, buf, BUF_SZ);
                    std::copy(buf, buf + read_sz, std::back_inserter(rcv_message));
                }
                REQUIRE(asock_state(connection) == Established);
                REQUIRE(rcv_message.size() == snd_message.size());
                REQUIRE(rcv_message == snd_message);

                snd_message.clear();
                rcv_message.clear();

                // Server sends reply to client
                const char *reply = "The ship has weather’d every rack, the prize we sought is won,";
                int reply_len = strlen(reply) + 1;
                snd_message = std::vector<char>(reply, reply + reply_len);
                REQUIRE(asock_write(connection, reply, reply_len) == (reply_len));
                usleep(1000);
                memset(buf, '\0', BUF_SZ);

                // Client recieved reply from server
                while (rcv_message.size() < snd_message.size()) {
                    usleep(1000);
                    ssize_t read_sz = read(fd, buf, BUF_SZ);
                    std::copy(buf, buf + read_sz, std::back_inserter(rcv_message));
                }
                REQUIRE(asock_state(connection) == Established);
                REQUIRE(rcv_message.size() == snd_message.size());
                REQUIRE(rcv_message == snd_message);
            }

            THEN("Server and client can send huge messages to each other") {

                // Client sends message to serverd
                const size_t TOTAL_TRANSM_BYTES = 1024 * 1024 * 10 + 25;
                std::vector<char> snd_message;
                std::vector<char> rcv_message;
                const size_t BUF_SZ = 4096;
                char buf[BUF_SZ];
                size_t total_transmitted = 0;
                while (total_transmitted < TOTAL_TRANSM_BYTES) {
                    size_t bytes_to_transmit = std::min(BUF_SZ, TOTAL_TRANSM_BYTES - total_transmitted);
                    for (size_t i = 0; i < bytes_to_transmit; ++i) {
                        buf[i] = rand() % 128;
                        snd_message.push_back(buf[i]);
                    }
                    total_transmitted += write(fd, buf, bytes_to_transmit);
                }
                REQUIRE(total_transmitted == TOTAL_TRANSM_BYTES);

                // Server recieves message from client
                while (rcv_message.size() < snd_message.size()) {
                    usleep(1000);
                    ssize_t read_sz = asock_read(connection, buf, BUF_SZ);
                    std::copy(buf, buf + read_sz, std::back_inserter(rcv_message));
                }
                REQUIRE(asock_state(connection) == Established);
                REQUIRE(rcv_message == snd_message);

                snd_message.clear();
                rcv_message.clear();
                total_transmitted = 0;

                // Server sends reply to client
                while (total_transmitted < TOTAL_TRANSM_BYTES) {
                    size_t bytes_to_transmit = std::min(BUF_SZ, TOTAL_TRANSM_BYTES - total_transmitted);
                    for (size_t i = 0; i < bytes_to_transmit; ++i) {
                        buf[i] = rand() % 128;
                        snd_message.push_back(buf[i]);
                    }
                    total_transmitted += asock_write(connection, buf, bytes_to_transmit);
                }
                REQUIRE(total_transmitted == TOTAL_TRANSM_BYTES);

                // Client recieved reply from server
                while (rcv_message.size() < snd_message.size()) {
                    usleep(1000);
                    ssize_t read_sz = read(fd, buf, BUF_SZ);
                    if (read_sz > 0)
                        std::copy(buf, buf + read_sz, std::back_inserter(rcv_message));
                    else
                        break;
                }
                REQUIRE(rcv_message == snd_message);

            }


            // Closing conenction
            REQUIRE(close(fd) != -1);
            while (asock_state(connection) == Established)
                usleep(500);
            REQUIRE(asock_state(connection) == Failed);

            asock_destroy(connection);
        }

        asock_server_stop(server);
        asock_server_destroy(server);
    }

}



TEST_CASE( "Test reading from closed connection", "[asock_server]" )
{

    WHEN("Having asock server") {
        asock_server_t *server = asock_server_create(AF_INET, SOCK_STREAM, 0);

        REQUIRE (server != NULL);

        uint16_t port = 13000;
        int start_status = ASOCK_FAILURE;
        int bind_status;
        while (start_status != ASOCK_SUCCESS && port < 13020) {
            port++;
            struct sockaddr_in sa;
            sa.sin_family = AF_INET;
            sa.sin_addr.s_addr = htonl(INADDR_ANY);
            sa.sin_port = htons(port);
            bind_status = asock_server_bind(server, (struct sockaddr*)&sa, sizeof(struct sockaddr));
            if (bind_status != ASOCK_SUCCESS) {
                continue;
            }
            start_status = asock_server_start(server);
        }
        REQUIRE(bind_status == ASOCK_SUCCESS);
        REQUIRE(start_status == ASOCK_SUCCESS);


        WHEN("Connecting to it") {
            int fd = socket(AF_INET, SOCK_STREAM, 0);
            REQUIRE(fd != -1);

            usleep(1000);
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

            int con_stat = connect(fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
            REQUIRE(con_stat != -1);



            asock_t *connection = NULL;
            DO_100_TIMES_WHILE(
                connection = asock_server_get_incom_connection(server), /* action */
                connection == NULL /* condition */
            );
            REQUIRE(connection != NULL);
            REQUIRE(asock_state(connection) == Established);


            THEN("Server can read data after connection with peer was close") {
                // Client sends message to server
                const char *message = "What happens to a dream deferred?";
                int mes_len = strlen(message) + 1;
                REQUIRE(write(fd, message, mes_len) == (mes_len));
                REQUIRE(close(fd) == (0));


                // Server recieves message from client
                while (asock_state(connection) == Established)
                    usleep(1000);
                REQUIRE(asock_state(connection) == Failed);
                const int BUF_SZ = 128;
                char buf[BUF_SZ];
                ssize_t read_sz = asock_read(connection, buf, BUF_SZ);
                REQUIRE(read_sz == mes_len);
                REQUIRE(strcmp(buf, message) == 0);

                asock_destroy(connection);
            }

            THEN("Client can read data after connection server breaks connection") {
                // Server sends reply to client
                const char *reply = "Does it dry up like a raisin in the sun?";
                int reply_len = strlen(reply) + 1;
                REQUIRE(asock_write(connection, reply, reply_len) == (reply_len));
                asock_destroy(connection);


                // Client recieved reply from server
                const int BUF_SZ = 128;
                char buf[BUF_SZ] = {'\0'};
                usleep(1000);
                int read_sz = read(fd, buf, BUF_SZ);
                REQUIRE(read_sz == reply_len);
                REQUIRE(strcmp(buf, reply) == 0);
            }


        }

        asock_server_destroy(server);
    }

}
