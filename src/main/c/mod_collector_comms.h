#include <httpd.h>
#ifndef __MOD_COLLECTOR_COMMS__
#define __MOD_COLLECTOR_COMMS__

typedef enum {
    DISCONNECTED,
    CONNECTED,
    FAILED
} connection_status;


typedef struct {
    char *server_ip;
    int server_port;
    connection_status conn_status;
    apr_socket_t *socket;
    long last_failure;
} server_connection;


#define END_OF_MESSAGE           "\n"

void initialize_connection(server_connection *server_connection, char *ip, int port);
void socket_write(apr_pool_t *pool, server_connection *server_connection, request_rec *r, char *buffer);
void socket_failure(server_connection *server_connection);
apr_socket_t* get_socket(apr_pool_t *pool, server_connection *server_connection, request_rec *r);

#endif