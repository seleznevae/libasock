#include "third_party/Catch/catch.hpp"
#include "asock.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

TEST_CASE( "Test asock lib initializing", "[asock_lib]" )
{
    AsockReturnCode status = asock_init();
    REQUIRE (ASOCK_IS_SUCCESS(status));
    asock_deinit();
}



TEST_CASE( "Test basic server and asocket connection", "[asock_lib]" )
{
    AsockReturnCode status = asock_init();
    REQUIRE (ASOCK_IS_SUCCESS(status));

    WHEN("Having asock server and service") {
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
            start_status = asock_server_listen(server);

        }
        REQUIRE(bind_status == ASOCK_SUCCESS);
        REQUIRE(start_status == ASOCK_SUCCESS);

        WHEN("Connecting to server") {
            struct sockaddr_in server_addr;
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port);
            server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

            asock_t *service_con = asock_create(AF_INET, SOCK_STREAM, 0);
            REQUIRE(service_con != NULL);

            int status = asock_connect(service_con,
                                     (struct sockaddr *)&server_addr,
                                     sizeof(struct sockaddr_in));
            REQUIRE( status == ASOCK_SUCCESS );


            int counter = 0;
            while (asock_state(service_con) == InProgress && counter++ < 100)
                usleep(1000);
            REQUIRE(asock_state(service_con) == Established);


            asock_t *server_con = NULL;
            while (! (server_con = asock_server_accept(server)))
                usleep(500);
            REQUIRE(server_con != NULL);
            REQUIRE(asock_state(server_con) == Established);



            THEN("Server and client can send messages to each other") {

                std::vector<char> snd_message;
                std::vector<char> rcv_message;

                // Client sends message to server
                const char *message = "O Captain! my Captain! Our fearful trip is done.";
                int mes_len = strlen(message) + 1;
                snd_message = std::vector<char>(message, message + mes_len);
                REQUIRE(asock_write(service_con, message, mes_len) == (mes_len));

                // Server recieves message from client
                const int BUF_SZ = 128;
                char buf[BUF_SZ];
                while (rcv_message.size() < snd_message.size()) {
                    usleep(1000);
                    ssize_t read_sz = asock_read(server_con, buf, BUF_SZ);
                    std::copy(buf, buf + read_sz, std::back_inserter(rcv_message));
                }
                REQUIRE(asock_state(server_con) == Established);
                REQUIRE(asock_state(service_con) == Established);
                REQUIRE(rcv_message == snd_message);


                snd_message.clear();
                rcv_message.clear();

                // Server sends reply to client
                const char *reply = "The ship has weather’d every rack, the prize we sought is won,";
                int reply_len = strlen(reply) + 1;
                snd_message = std::vector<char>(reply, reply + reply_len);
                REQUIRE(asock_write(server_con, reply, reply_len) == (reply_len));
                usleep(1000);
                memset(buf, '\0', BUF_SZ);

                // Client recieved reply from server
                while (rcv_message.size() < snd_message.size()) {
                    usleep(1000);
                    ssize_t read_sz = asock_read(service_con, buf, BUF_SZ);
                    std::copy(buf, buf + read_sz, std::back_inserter(rcv_message));
                }
                REQUIRE(asock_state(server_con) == Established);
                REQUIRE(asock_state(service_con) == Established);
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
                    total_transmitted += asock_write(service_con, buf, bytes_to_transmit);
                }
                REQUIRE(total_transmitted == TOTAL_TRANSM_BYTES);

                // Server recieves message from client
                while (rcv_message.size() < snd_message.size()) {
                    usleep(1000);
                    ssize_t read_sz = asock_read(server_con, buf, BUF_SZ);
                    std::copy(buf, buf + read_sz, std::back_inserter(rcv_message));
                }
                REQUIRE(asock_state(server_con) == Established);
                REQUIRE(asock_state(service_con) == Established);
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
                    total_transmitted += asock_write(server_con, buf, bytes_to_transmit);
                }
                REQUIRE(total_transmitted == TOTAL_TRANSM_BYTES);

                // Client recieved reply from server
                while (rcv_message.size() < snd_message.size()) {
                    usleep(1000);
                    ssize_t read_sz = asock_read(service_con, buf, BUF_SZ);
                    if (read_sz > 0)
                        std::copy(buf, buf + read_sz, std::back_inserter(rcv_message));
                    else
                        break;
                }
                REQUIRE(rcv_message == snd_message);

            }


            asock_destroy(server_con);

            // Closing conenction
            counter = 0;
            while (asock_state(service_con) == Established && counter++ < 100)
                usleep(500);
            REQUIRE(asock_state(service_con) == Failed);

            asock_destroy(service_con);
        }

        asock_server_destroy(server);
    }

    asock_deinit();
}
