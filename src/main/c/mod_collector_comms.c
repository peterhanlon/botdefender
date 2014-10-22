
/*
 The MIT License (MIT)

 Copyright (c)2014 Peter Hanlon

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

#include <http_protocol.h>
#include "http_config.h"
#include "http_log.h"
#include "mod_collector_comms.h"


void initialize_connection(server_connection *server_connection, char *ip, int port) {
    server_connection->conn_status = FAILED;
    server_connection->last_failure = 0;
    server_connection->socket = NULL;
    server_connection->server_ip = ip;
    server_connection->server_port = port;
}


void socket_failure(server_connection *server_connection) {
    if (server_connection->socket != NULL) {
        apr_socket_close(server_connection->socket);
    }

    apr_time_t now = apr_time_now();
    server_connection->last_failure = (now/1000);

    server_connection->conn_status = FAILED;
    server_connection->socket = NULL;
}



/*
 * Get a socket to the Java Collector server
 */
apr_socket_t* get_socket(apr_pool_t *pool, server_connection *server_connection, request_rec *r)
{
    apr_sockaddr_t *sa;
    apr_status_t rv;
    int family = APR_INET;

    apr_time_t now = apr_time_now();
    long now_ms = (now/1000);

    // If the current socket is NULL i.e. no connection and the time since the last failure is
    // greater than 1 second, try to connect again.
    if ((server_connection->socket == NULL) && ((now_ms - server_connection->last_failure) > 1000)) {
        rv = apr_sockaddr_info_get(&sa, server_connection->server_ip, family, server_connection->server_port, 0, pool);

        if (rv != APR_SUCCESS) {
            if (server_connection->conn_status != FAILED) {
                ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                        "mod_collector: [1] listener socket not open, unable to get socket info");
            }
            socket_failure(server_connection);
            return NULL;
        }

        rv = apr_socket_create(&server_connection->socket, sa->family, SOCK_STREAM, APR_PROTO_TCP, pool);

        if (rv != APR_SUCCESS) {
            if (server_connection->conn_status != FAILED) {
                ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                        "mod_collector: [2] listener socket not open, unable to create socket");
            }
            socket_failure(server_connection);
            return NULL;
        }

        rv = apr_socket_connect(server_connection->socket, sa);

        if (rv != APR_SUCCESS) {
            if (server_connection->conn_status != FAILED) {
                ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                        "mod_collector: listener socket not open, unable to connect to socket [%s] [%d]",
                        server_connection->server_ip, server_connection->server_port);
            }
            socket_failure(server_connection);
            return NULL;
        }

        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                "mod_collector: Socket created to listener");

        server_connection->conn_status = CONNECTED;
    }

    return server_connection->socket;
}



/*
 * Write the JSON string containing request/response to the socket
 */
void socket_write(apr_pool_t *pool, server_connection *server_connection, request_rec *r, char *buffer)
{
    apr_socket_t *sock = get_socket(pool, server_connection, r);

    if (sock == NULL) {
        return;
    }

    // Send message
    apr_size_t eomSize = strlen(END_OF_MESSAGE);
    apr_size_t msgSize = strlen(buffer);

    int rc = apr_socket_send(sock, buffer, &msgSize);
    if (rc != 0) {
        socket_failure(server_connection);
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                "mod_collector: Failed to send message (1), status %d", rc);
        return;
    }

    rc = apr_socket_send(sock, END_OF_MESSAGE, &eomSize);
    if (rc != 0) {
        socket_failure(server_connection);
        ap_log_error(APLOG_MARK, APLOG_NOERRNO|APLOG_NOTICE, 0, r->server,
                "mod_collector: Failed to send message (2), status %d", rc);
        return;
    }
}
