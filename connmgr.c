#include "connmgr.h"

#include "config.h"
#include "lib/tcpsock.h"
#include "lib/vector.h"
#include "sbuffer.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <time.h>
#include <unistd.h>

void connmgr_listen(int port_number, sbuffer_t* buffer) {

#if DEBUG
    const int fd =
        open("sensor_data_recv", O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);
    assert(fd > 0);
#endif

    vector_t* sockets = vector_create();

    {
        tcpsock_t* connection_socket = NULL;
        if (tcp_passive_open(&connection_socket, port_number) != TCP_NO_ERROR)
            exit(EXIT_FAILURE);
        vector_add(sockets, connection_socket);
    }

    bool active = true;
    struct pollfd* fds = NULL;
    while (active) {
        fds = realloc(fds, vector_size(sockets) * sizeof(*fds));

        for (size_t i = 0; i < vector_size(sockets); i++) {
            tcpsock_t* socket = vector_at(sockets, i);
            fds[i] = (struct pollfd){
                .fd = socket->sd,
                .events = POLLIN,
            };
        }

        int n = poll(fds, vector_size(sockets), TIMEOUT * 1000);
        assert(n != -1);

        if (n == 0) {
            // quit the connmgr (TIMEOUT was reached)
            printf("%d tupples added\n",getAdded(buffer));
            printf("%d tupples calculated\n",getData(buffer));
            printf("%d tupples stored\n",getStored(buffer));
            printf("No sensor data received after " TO_STRING(TIMEOUT) " seconds. Quitting server.\n");
            active = false;
        } else {
            // loop over sockets
            size_t size = vector_size(sockets); // cache up front because some sockets may get added
            for (size_t i = 0; i < size; i++) {
                tcpsock_t* socket = vector_at(sockets, i);
                if (i != 0 && time(NULL) > *tcp_last_seen(socket) + TIMEOUT) {
                    printf("Sensor with id %d timed out. \n", *tcp_last_seen_sensor_id(socket));
                    tcp_close(&socket);
                    vector_remove_at_index(sockets, i);
                    break;
                } else if ((fds[i].revents & POLLIN) != 0) {
                    *tcp_last_seen(socket) = time(NULL);
                    if (i == 0) { // a new sensor is connected
                        tcpsock_t* new_socket = NULL;
                        tcp_wait_for_connection(socket, &new_socket);
                        // this does not invalidate our loop since we only iterate over the original sockets
                        vector_add(sockets, new_socket);
                    } else { // data from existing connection is obtained
                        sensor_data_t data;
                        int bytes = sizeof(data.id);
                        tcp_receive(socket, &data.id, &bytes);

                        bytes = sizeof(data.value);
                        tcp_receive(socket, &data.value, &bytes);

                        bytes = sizeof(data.ts);
                        const int result = tcp_receive(socket, &data.ts, &bytes);

                        if (!socket->announced) {
                            printf("A new sensor with id = %" PRIu16 " has opened a new connection\n", data.id);
                            socket->announced = true;
                        }

                        if ((result == TCP_NO_ERROR) && bytes) {
                            *tcp_last_seen_sensor_id(socket) = data.id;
#if DEBUG
                            ASSERT_ELSE_PERROR(write(fd, &data.id, sizeof(data.id)) == sizeof(data.id));
                            ASSERT_ELSE_PERROR(write(fd, &data.value, sizeof(data.value)) == sizeof(data.value));
                            ASSERT_ELSE_PERROR(write(fd, &data.ts, sizeof(data.ts)) == sizeof(data.ts));
#endif
                            printf("sensor id = %" PRIu16 " - temperature = %g - timestamp = %ld\n", data.id, data.value, data.ts);
                            int ret = sbuffer_insert_first(buffer, &data);
                            assert(ret == SBUFFER_SUCCESS);
                        } else if (result == TCP_CONNECTION_CLOSED) {
                            printf("Sensor with id %" PRIu16 " disconnected\n", *tcp_last_seen_sensor_id(socket));
                            tcp_close(&socket);
                            vector_remove_at_index(sockets, i);
                            break;
                        }
                    }
                }
            }
        }
    }
    free(fds);
#if DEBUG
    close(fd);
#endif

    for (size_t i = 0; i < vector_size(sockets); i++) {
        tcpsock_t* socket = vector_at(sockets, i);
        tcp_close(&socket);
    }
    vector_destroy(sockets);
}
