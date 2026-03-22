/*
 * net.h — Sieťová vrstva — TCP server/klient pre komunikáciu medzi nodes.
 *
 * Každý node beží ako TCP server a zároveň sa pripája k ostatným peers.
 * Správy sa posielajú ako JSON zakončené newline '\n'.
 * Pripojenie medzi nodes sa tuneluje cez SSH.
 */

#ifndef NET_H
#define NET_H

#include <stdint.h>
#include <pthread.h>
#include "chain.h"

/* Predvolený port pre node. */
#define DEFAULT_PORT 9333

/* Maximálny počet pripojených peers. */
#define MAX_PEERS 16

/* Maximálna veľkosť správy (bajty). */
#define MAX_MSG_SIZE (1024 * 1024)

/* Typy sieťových správ. */
typedef enum {
    MSG_NEW_BLOCK,      /* Nový vyťažený blok. */
    MSG_NEW_TX,         /* Nová transakcia. */
    MSG_REQUEST_CHAIN,  /* Žiadosť o celý blockchain. */
    MSG_CHAIN_RESPONSE, /* Odpoveď s celým blockchainom. */
    MSG_PEER_LIST,      /* Zoznam známych peers. */
    MSG_PING,           /* Kontrola spojenia. */
    MSG_PONG            /* Odpoveď na ping. */
} msg_type_t;

typedef struct peer {
    int         fd;         /* Socket file descriptor. */
    char        host[256];  /* Hostname/IP. */
    uint16_t    port;       /* Port. */
    int         active;     /* Či je spojenie aktívne. */
    pthread_t   thread;     /* Vlákno na čítanie správ od peera. */
} peer_t;

typedef struct node {
    uint16_t        port;
    int             server_fd;
    peer_t          peers[MAX_PEERS];
    int             peer_count;
    blockchain_t    *chain;     /* Ukazovateľ na zdieľaný blockchain. */
    pthread_mutex_t peers_lock;
    volatile int    running;    /* Flag pre ukončenie. */
    pthread_t       accept_thread;

    /* Callback — volá sa keď príde nový blok alebo transakcia. */
    void (*on_new_block)(struct node *node, const block_t *block);
    void (*on_new_tx)(struct node *node, const tx_t *tx);

    /*
     * Callback pre synchronizáciu mempoolu — volá sa pri pripojení peera.
     * Funkcia má poslať všetky TX z mempoolu novému peerovi.
     * Parametre: node, peer file descriptor.
     */
    void (*on_peer_connected)(struct node *node, int peer_fd);
} node_t;

/*
 * Inicializuje node a spustí TCP server na danom porte.
 * Vracia 0 pri úspechu, -1 pri chybe.
 */
int node_init(node_t *node, uint16_t port, blockchain_t *chain);

/*
 * Zastaví node — uzavrie všetky spojenia.
 */
void node_stop(node_t *node);

/*
 * Pripojí sa k peerovi na danom hoste a porte.
 * Vracia 0 pri úspechu, -1 pri chybe.
 */
int node_connect_peer(node_t *node, const char *host, uint16_t port);

/*
 * Odošle nový blok všetkým pripojeným peers.
 */
void node_broadcast_block(node_t *node, const block_t *block);

/*
 * Odošle novú transakciu všetkým pripojeným peers.
 */
void node_broadcast_tx(node_t *node, const tx_t *tx);

/*
 * Požiada peera o celý blockchain (pre synchronizáciu).
 */
void node_request_chain(node_t *node, int peer_idx);

/*
 * Odošle transakciu konkrétnemu peerovi (podľa fd).
 */
void node_send_tx(node_t *node, int peer_fd, const tx_t *tx);

#endif /* NET_H */
