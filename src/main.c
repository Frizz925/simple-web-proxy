#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <uv.h>

#include "pool.h"

#define DEFAULT_HOST "0.0.0.0"
#define DEFAULT_PORT 8080
#define DEFAULT_BACKLOG 5

#define CONNECTION_POOL_SIZE 256

#define BUFFER_SIZE 512
#define BUFFER_POOL_SIZE 512

#define HTTP_RESPONSE_200 "HTTP/1.1 200 Connection Established\r\n\r\n"
#define HTTP_RESPONSE_503 "HTTP/1.1 503 Resource Unavailable\r\n\r\n"

typedef struct {
    uv_tcp_t server;
    pool_t conn_pool, buf_pool;
    const char *host;
    int port;
} app_context_t;

typedef struct {
    uv_tcp_t sock;
    char family;
    char addr[32];
    int port;
} connection_peer_t;

typedef struct {
    connection_peer_t src, dst;
    app_context_t *ctx;
} connection_t;

void print_out(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stdout, format, ap);
    fflush(stdout);
    va_end(ap);
}

void print_err(const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    fflush(stderr);
    va_end(ap);
}

void print_uv_err(const char *msg, int err) {
    print_err("%s: %s\n", msg, uv_strerror(err));
}

void client_alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    connection_t *conn = (connection_t *)handle->data;
    *buf = uv_buf_init(pool_allocate(&conn->ctx->buf_pool), BUFFER_SIZE);
}

void client_close_cb(uv_handle_t *handle) {
    connection_t *conn = (connection_t *)handle->data;
    pool_deallocate(&conn->ctx->conn_pool, conn);
}

void client_read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf) {
    connection_t *conn = (connection_t *)stream->data;
    if (nread <= 0) {
        print_uv_err("read", nread);
        uv_close((uv_handle_t *)stream, client_close_cb);
        goto cleanup;
    }

cleanup:
    if (buf->base) pool_deallocate(&conn->ctx->buf_pool, buf->base);
}

int get_remote_addr(uv_tcp_t *handle, connection_peer_t *peer) {
    int addrlen = sizeof(struct sockaddr_storage);
    struct sockaddr_storage ssaddr;
    struct sockaddr_in *saddr = (struct sockaddr_in *)&ssaddr;
    struct sockaddr_in6 *saddr6 = (struct sockaddr_in6 *)&ssaddr;

    int rc = uv_tcp_getpeername(handle, (struct sockaddr *)&ssaddr, &addrlen);
    if (rc) return rc;

    peer->family = ssaddr.ss_family;
    switch (ssaddr.ss_family) {
    case AF_INET:
        uv_ip4_name(saddr, peer->addr, sizeof(peer->addr));
        peer->port = ntohs(saddr->sin_port);
        break;
    case AF_INET6:
        uv_ip6_name(saddr6, peer->addr, sizeof(peer->addr));
        peer->port = ntohs(saddr6->sin6_port);
        break;
    default:
        return -1;
    }
    return 0;
}

void connect_cb(uv_stream_t *server, int status) {
    int rc;
    if (status) {
        print_uv_err("listen", status);
        uv_close((uv_handle_t *)server, NULL);
        return;
    }

    app_context_t *ctx = (app_context_t *)server->data;
    connection_t *conn = (connection_t *)pool_allocate(&ctx->conn_pool);
    uv_tcp_t *client = (uv_tcp_t *)&conn->src;
    client->data = conn;
    conn->ctx = ctx;

    uv_tcp_init(server->loop, client);
    rc = uv_accept(server, (uv_stream_t *)client);
    if (rc) {
        print_uv_err("accept", rc);
        goto client_cleanup;
    }

    rc = get_remote_addr(client, &conn->src);
    if (rc) {
        print_uv_err("get_remote_addr", rc);
        goto client_cleanup;
    }
    print_out("Accepted connection from %s:%d\n", conn->src.addr, conn->src.port);

    uv_read_start((uv_stream_t *)client, client_alloc_cb, client_read_cb);
    return;

client_cleanup:
    uv_close((uv_handle_t *)client, client_close_cb);
}

void resolve_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *res) {
    if (status) {
        print_uv_err("getaddrinfo", status);
        goto cleanup;
    }

    app_context_t *ctx = (app_context_t *)req->data;
    uv_tcp_t *server = &ctx->server;
    uv_tcp_init(req->loop, server);
    server->data = ctx;

    int rc = uv_tcp_bind(server, res->ai_addr, 0);
    if (rc) {
        print_uv_err("bind", rc);
        goto cleanup;
    }
    print_out("Server listening at %s:%d\n", ctx->host, ctx->port);

    pool_init(&ctx->conn_pool, sizeof(connection_t), CONNECTION_POOL_SIZE);
    pool_init(&ctx->buf_pool, BUFFER_SIZE, BUFFER_POOL_SIZE);
    uv_listen((uv_stream_t *)server, DEFAULT_BACKLOG, connect_cb);

cleanup:
    uv_freeaddrinfo(res);
}

int main(int argc, char **argv) {
    int port = argc >= 2 ? atoi(argv[1]) : DEFAULT_PORT;
    char *host = argc >= 3 ? argv[2] : DEFAULT_HOST;
    app_context_t app_ctx = {.host = host, .port = port};

    char serv[6];
    sprintf(serv, "%d", port);

    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
    };

    uv_loop_t *loop = uv_default_loop();
    uv_getaddrinfo_t req = {.data = &app_ctx};
    uv_getaddrinfo(loop, &req, resolve_cb, host, serv, &hints);

    return uv_run(loop, UV_RUN_DEFAULT);
}