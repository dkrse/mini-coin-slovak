/*
 * net.c — Sieťová vrstva — TCP server, peer management, broadcast.
 *
 * Každý node beží TCP server, ktorý prijíma spojenia od peers.
 * Pre každého peera beží samostatné vlákno, ktoré číta správy.
 * Správy sú JSON zakončené newline '\n'.
 */

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>
#include "net.h"
#include "protocol.h"

/* Forward declarations. */
static void broadcast_block_except(node_t *node, const block_t *block,
                                    int exclude_fd);

/*
 * Prečíta jednu riadkovú správu zo socketu.
 * Vracia alokovaný buffer alebo NULL.
 */
static char *read_message(int fd)
{
    char *buf = malloc(MAX_MSG_SIZE);
    if (!buf) return NULL;

    size_t total = 0;
    while (total < MAX_MSG_SIZE - 1) {
        ssize_t n = recv(fd, buf + total, 1, 0);
        if (n <= 0) {
            free(buf);
            return NULL;
        }
        if (buf[total] == '\n') {
            buf[total] = '\0';
            return buf;
        }
        total++;
    }
    buf[total] = '\0';
    return buf;
}

/*
 * Odošle správu cez socket.
 */
static int send_message(int fd, const char *msg)
{
    size_t len = strlen(msg);
    size_t sent = 0;

    while (sent < len) {
        ssize_t n = send(fd, msg + sent, len - sent, MSG_NOSIGNAL);
        if (n <= 0) return -1;
        sent += n;
    }
    return 0;
}

/*
 * Spracuje prijatú správu od peera.
 */
static void handle_message(node_t *node, int peer_fd, const char *raw)
{
    char *payload = NULL;
    msg_type_t type = protocol_parse_msg(raw, &payload);

    switch (type) {
    case MSG_NEW_BLOCK: {
        block_t block;
        if (payload && protocol_deserialize_block(payload, &block) == 0) {
            if (chain_add_block(node->chain, &block) == 0) {
                printf("\n[Sieť] Prijatý nový blok #%u.\n", block.index);
                if (node->on_new_block) {
                    node->on_new_block(node, &block);
                }
                /* Prepošleme ostatným peerom okrem odosielateľa. */
                broadcast_block_except(node, &block, peer_fd);
            }
        }
        break;
    }

    case MSG_NEW_TX: {
        tx_t tx;
        if (payload && protocol_deserialize_tx(payload, &tx) == 0) {
            printf("\n[Sieť] Prijatá nová transakcia: %s -> %s : %lu\n",
                   tx.sender, tx.receiver, (unsigned long)tx.amount);
            if (node->on_new_tx) {
                node->on_new_tx(node, &tx);
            }
        }
        break;
    }

    case MSG_REQUEST_CHAIN: {
        /* Odošleme celý chain. */
        char *chain_json = protocol_serialize_chain(node->chain);
        if (chain_json) {
            char *msg = protocol_serialize_msg(MSG_CHAIN_RESPONSE, chain_json);
            if (msg) {
                send_message(peer_fd, msg);
                free(msg);
            }
            free(chain_json);
        }
        break;
    }

    case MSG_CHAIN_RESPONSE: {
        /* Prijatý chain — nahradíme ak je dlhší. */
        if (payload) {
            blockchain_t *other = calloc(1, sizeof(*other));
            if (other) {
                pthread_mutex_init(&other->lock, NULL);

                if (protocol_deserialize_chain(payload, other) == 0) {
                    chain_replace(node->chain, other);
                }
                pthread_mutex_destroy(&other->lock);
                free(other);
            }
        }
        break;
    }

    case MSG_PING: {
        char *msg = protocol_serialize_msg(MSG_PONG, NULL);
        if (msg) {
            send_message(peer_fd, msg);
            free(msg);
        }
        break;
    }

    case MSG_PONG:
        /* Peer žije. */
        break;

    default:
        break;
    }

    free(payload);
}

/* Wrapper pre thread parameter. */
typedef struct {
    node_t *node;
    int peer_idx;
} peer_thread_arg_t;

static void *peer_handler(void *arg)
{
    peer_thread_arg_t *pta = (peer_thread_arg_t *)arg;
    node_t *node = pta->node;
    int idx = pta->peer_idx;
    free(pta);

    peer_t *peer = &node->peers[idx];

    while (node->running && peer->active) {
        char *msg = read_message(peer->fd);
        if (!msg) {
            printf("\n[Sieť] Peer %s:%d odpojený.\n", peer->host, peer->port);
            peer->active = 0;
            close(peer->fd);
            break;
        }
        handle_message(node, peer->fd, msg);
        free(msg);
    }

    return NULL;
}

/*
 * Pridá nového peera a spustí pre neho čítacie vlákno.
 */
static int add_peer(node_t *node, int fd, const char *host, uint16_t port)
{
    pthread_mutex_lock(&node->peers_lock);

    if (node->peer_count >= MAX_PEERS) {
        pthread_mutex_unlock(&node->peers_lock);
        close(fd);
        return -1;
    }

    int idx = node->peer_count;
    peer_t *peer = &node->peers[idx];
    peer->fd = fd;
    strncpy(peer->host, host, sizeof(peer->host) - 1);
    peer->port = port;
    peer->active = 1;
    node->peer_count++;

    pthread_mutex_unlock(&node->peers_lock);

    peer_thread_arg_t *pta = malloc(sizeof(*pta));
    pta->node = node;
    pta->peer_idx = idx;

    pthread_create(&peer->thread, NULL, peer_handler, pta);
    pthread_detach(peer->thread);

    printf("[Sieť] Peer pripojený: %s:%d\n", host, port);

    /* Po pripojení vždy požiadame o chain pre synchronizáciu. */
    node_request_chain(node, idx);

    /* Pošleme peerovi naše čakajúce transakcie z mempoolu. */
    if (node->on_peer_connected) {
        node->on_peer_connected(node, fd);
    }

    return 0;
}

/*
 * Vlákno pre prijímanie nových spojení.
 */
static void *accept_thread(void *arg)
{
    node_t *node = (node_t *)arg;
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    while (node->running) {
        int client_fd = accept(node->server_fd,
                               (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            if (node->running) {
                perror("accept");
            }
            continue;
        }

        char host[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, host, sizeof(host));
        uint16_t port = ntohs(client_addr.sin_port);

        add_peer(node, client_fd, host, port);
    }

    return NULL;
}

int node_init(node_t *node, uint16_t port, blockchain_t *chain)
{
    memset(node, 0, sizeof(*node));
    node->port = port;
    node->chain = chain;
    node->running = 1;
    pthread_mutex_init(&node->peers_lock, NULL);

    /* Vytvoríme TCP server socket. */
    node->server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (node->server_fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(node->server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(node->server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(node->server_fd);
        return -1;
    }

    if (listen(node->server_fd, 5) < 0) {
        perror("listen");
        close(node->server_fd);
        return -1;
    }

    /* Spustíme vlákno na prijímanie spojení. */
    pthread_create(&node->accept_thread, NULL, accept_thread, node);

    printf("[Sieť] Node počúva na porte %d.\n", port);
    return 0;
}

void node_stop(node_t *node)
{
    node->running = 0;

    /* Zatvoríme server socket — to preruší accept(). */
    if (node->server_fd >= 0) {
        shutdown(node->server_fd, SHUT_RDWR);
        close(node->server_fd);
    }

    /* Zatvoríme všetky peer spojenia. */
    pthread_mutex_lock(&node->peers_lock);
    for (int i = 0; i < node->peer_count; i++) {
        if (node->peers[i].active) {
            node->peers[i].active = 0;
            close(node->peers[i].fd);
        }
    }
    pthread_mutex_unlock(&node->peers_lock);
    pthread_mutex_destroy(&node->peers_lock);
}

int node_connect_peer(node_t *node, const char *host, uint16_t port)
{
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
        fprintf(stderr, "Nemôžem nájsť host: %s\n", host);
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(res);
        return -1;
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        fprintf(stderr, "Nemôžem sa pripojiť k %s:%d: %s\n",
                host, port, strerror(errno));
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    add_peer(node, fd, host, port);

    /* Po pripojení požiadame o chain pre synchronizáciu. */
    node_request_chain(node, node->peer_count - 1);

    return 0;
}

/*
 * Interná funkcia — broadcast bloku všetkým peerom okrem exclude_fd.
 * Ak exclude_fd == -1, posiela všetkým.
 */
static void broadcast_block_except(node_t *node, const block_t *block,
                                    int exclude_fd)
{
    char *block_json = protocol_serialize_block(block);
    if (!block_json) return;

    char *msg = protocol_serialize_msg(MSG_NEW_BLOCK, block_json);
    free(block_json);
    if (!msg) return;

    pthread_mutex_lock(&node->peers_lock);
    for (int i = 0; i < node->peer_count; i++) {
        if (node->peers[i].active && node->peers[i].fd != exclude_fd) {
            send_message(node->peers[i].fd, msg);
        }
    }
    pthread_mutex_unlock(&node->peers_lock);

    free(msg);
}

void node_broadcast_block(node_t *node, const block_t *block)
{
    broadcast_block_except(node, block, -1);
}

void node_broadcast_tx(node_t *node, const tx_t *tx)
{
    char *tx_json = protocol_serialize_tx(tx);
    if (!tx_json) return;

    char *msg = protocol_serialize_msg(MSG_NEW_TX, tx_json);
    free(tx_json);
    if (!msg) return;

    pthread_mutex_lock(&node->peers_lock);
    for (int i = 0; i < node->peer_count; i++) {
        if (node->peers[i].active) {
            send_message(node->peers[i].fd, msg);
        }
    }
    pthread_mutex_unlock(&node->peers_lock);

    free(msg);
}

void node_request_chain(node_t *node, int peer_idx)
{
    if (peer_idx < 0 || peer_idx >= node->peer_count) return;
    if (!node->peers[peer_idx].active) return;

    char *msg = protocol_serialize_msg(MSG_REQUEST_CHAIN, NULL);
    if (msg) {
        send_message(node->peers[peer_idx].fd, msg);
        free(msg);
    }
}

void node_send_tx(node_t *node, int peer_fd, const tx_t *tx)
{
    (void)node;
    char *tx_json = protocol_serialize_tx(tx);
    if (!tx_json) return;

    char *msg = protocol_serialize_msg(MSG_NEW_TX, tx_json);
    free(tx_json);
    if (!msg) return;

    send_message(peer_fd, msg);
    free(msg);
}
