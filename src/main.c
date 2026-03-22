/*
 * main.c — Hlavný modul — CLI menu, ťaženie, transakcie.
 *
 * Spustenie:
 *   ./minicoin [port]
 *
 * Pripojenie k peerovi cez SSH tunel:
 *   ssh -L 9334:localhost:9333 user@remote
 *   Potom v menu: Pripojiť peer -> localhost:9334
 */

#define _POSIX_C_SOURCE 200809L
#define _BSD_SOURCE
#define _DEFAULT_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "block.h"
#include "chain.h"
#include "tx.h"
#include "wallet.h"
#include "net.h"
#include "protocol.h"

/* Maximálny počet transakcií čakajúcich na zaradenie do bloku (mempool). */
#define MEMPOOL_SIZE 100

static blockchain_t chain;
static wallet_t     wallet;
static node_t       node;

/* Jednoduchý mempool — pole transakcií. */
static tx_t mempool[MEMPOOL_SIZE];
static int  mempool_count = 0;
static pthread_mutex_t mempool_lock = PTHREAD_MUTEX_INITIALIZER;

/* Cesta k súboru peňaženky. */
static const char *wallet_file = "wallet.pem";

/* Callback pre nové transakcie zo siete — pridáme do mempoolu. */
static void on_new_tx(node_t *n, const tx_t *tx)
{
    (void)n;
    pthread_mutex_lock(&mempool_lock);
    if (mempool_count < MEMPOOL_SIZE) {
        mempool[mempool_count++] = *tx;
    }
    pthread_mutex_unlock(&mempool_lock);
}

/*
 * Callback pri pripojení nového peera — pošleme mu všetky TX z mempoolu,
 * aby ich mohol zahrnúť do bloku pri ťažení.
 */
static void on_peer_connected(node_t *n, int peer_fd)
{
    pthread_mutex_lock(&mempool_lock);
    for (int i = 0; i < mempool_count; i++) {
        node_send_tx(n, peer_fd, &mempool[i]);
    }
    if (mempool_count > 0) {
        printf("[Sieť] Odoslaných %d transakcií z mempoolu novému peerovi.\n",
               mempool_count);
    }
    pthread_mutex_unlock(&mempool_lock);
}

/*
 * Animácia — spinner s textom. Beží ms milisekúnd.
 */
static void animate_spinner(const char *text, int ms)
{
    const char *frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
    int n_frames = 10;
    int elapsed = 0;
    int frame_ms = 100;

    while (elapsed < ms) {
        printf("\r  %s %s", frames[(elapsed / frame_ms) % n_frames], text);
        fflush(stdout);
        usleep(frame_ms * 1000);
        elapsed += frame_ms;
    }
    printf("\r                                                                    \r");
    fflush(stdout);
}

/*
 * Animácia ťaženia — ukazuje meniace sa hashe a nonce.
 */
static void animate_mining(int duration_ms)
{
    int elapsed = 0;
    int frame_ms = 150;
    uint64_t fake_nonce = 0;

    srand((unsigned)time(NULL));

    while (elapsed < duration_ms) {
        /* Generujeme náhodný "hash" pre vizuálny efekt. */
        char fake_hash[65];
        for (int i = 0; i < 64; i++) {
            int r = rand() % 16;
            fake_hash[i] = r < 10 ? '0' + r : 'a' + (r - 10);
        }
        fake_hash[64] = '\0';

        /* Postupne pridávame vedúce nuly ako sa blížime ku koncu. */
        int zeros = (elapsed * 4) / duration_ms; /* 0 → 3 nuly postupne */
        for (int i = 0; i < zeros && i < 4; i++) {
            fake_hash[i] = '0';
        }

        printf("\r  Nonce: %-8lu | Hash: %.32s...", (unsigned long)fake_nonce, fake_hash);
        fflush(stdout);

        fake_nonce += rand() % 500 + 100;
        usleep(frame_ms * 1000);
        elapsed += frame_ms;
    }
    printf("\r                                                                              \r");
    fflush(stdout);
}

/*
 * Zobrazí hlavné menu.
 */
static void print_menu(void)
{
    int64_t balance = chain_get_balance(&chain, wallet.address);
    int peers;

    pthread_mutex_lock(&node.peers_lock);
    peers = node.peer_count;
    pthread_mutex_unlock(&node.peers_lock);

char line[80]; // pomocný buffer pre dynamické riadky

printf("\n");
printf("╔═════════════════════════════════════════════════════════════════════════════╗\n");

// Riadok s Portom
snprintf(line, sizeof(line), "  MiniCoin Node [localhost:%d]", node.port);
printf("║ %-76s║\n", line);

// Riadok so štatistikami (teraz s prirodzenými medzerami)
snprintf(line, sizeof(line), "  Chain: %d blokov | Peers: %d | Zostatok: %ld coin", 
         chain.length, peers, (long)balance);
printf("║ %-76s║\n", line);

// Riadok s Adresou
snprintf(line, sizeof(line), "  Adresa: %s", wallet.address);
printf("║ %-76s║\n", line);

printf("╠═════════════════════════════════════════════════════════════════════════════╣\n");
printf("║  [1] Ťažiť blok                                                             ║\n");
printf("║  [2] Poslať transakciu                                                      ║\n");
printf("║  [3] Zobraziť blockchain                                                    ║\n");
printf("║  [4] Zobraziť peňaženku                                                     ║\n");
printf("║  [5] Pripojiť peer                                                          ║\n");
printf("║  [6] Stav siete                                                             ║\n");
printf("║  [7] Mempool                                                                ║\n");
printf("║  [q] Ukončiť                                                                ║\n");
printf("╚═════════════════════════════════════════════════════════════════════════════╝\n");
printf("> ");
}

/*
 * Ťaženie nového bloku.
 */
static void do_mine(void)
{
    block_t new_block;
    const block_t *last;

    memset(&new_block, 0, sizeof(new_block));

    pthread_mutex_lock(&chain.lock);
    last = chain_last_block(&chain);
    new_block.index = last->index + 1;
    new_block.timestamp = time(NULL);
    strncpy(new_block.prev_hash, last->hash, HASH_HEX_LEN);
    pthread_mutex_unlock(&chain.lock);

    /* Najprv zozbierame transakcie z mempoolu a spočítame poplatky. */
    uint64_t total_fees = 0;
    new_block.tx_count = 1; /* Index 0 je coinbase — naplníme neskôr. */

    pthread_mutex_lock(&mempool_lock);
    for (int i = 0; i < mempool_count && new_block.tx_count < MAX_TX_PER_BLOCK; i++) {
        /* Overíme, či má odosielateľ dostatok (suma + poplatok). */
        int64_t balance = chain_get_balance(&chain, mempool[i].sender);
        uint64_t total_cost = mempool[i].amount + mempool[i].fee;
        if (balance >= (int64_t)total_cost) {
            new_block.transactions[new_block.tx_count++] = mempool[i];
            total_fees += mempool[i].fee;
        }
    }
    mempool_count = 0;
    pthread_mutex_unlock(&mempool_lock);

    /* Coinbase transakcia — odmena + poplatky. */
    new_block.transactions[0] = tx_create_coinbase(wallet.address);
    new_block.transactions[0].amount += total_fees;
    tx_compute_hash(&new_block.transactions[0]);

    printf("\n");
    printf("  ┌─ Ťaženie bloku #%u (%d transakcií) ─────────────────────\n",
           new_block.index, new_block.tx_count);
    printf("  │\n");

    /* Fáza 1: Príprava bloku (2s). */
    animate_spinner("Pripravujem blok, overujem transakcie...", 2000);
    printf("  │ Transakcie overené.\n");

    /* Fáza 2: Hľadanie nonce — animácia (5-8s). */
    printf("  │\n");
    printf("  │ Hľadám platný nonce (difficulty: %d nuly)...\n", MINING_DIFFICULTY);
    printf("  │\n");

    int mine_duration = 5000 + (rand() % 3000); /* 5-8 sekúnd */
    animate_mining(mine_duration);

    /* Skutočné ťaženie (okamžité). */
    uint64_t attempts = block_mine(&new_block);

    printf("  │ Nonce nájdený!\n");
    printf("  │\n");

    /* Fáza 3: Overenie (1s). */
    animate_spinner("Overujem proof-of-work...", 1000);
    printf("  │ Proof-of-work platný.\n");
    printf("  │\n");
    printf("  │ Nonce:     %lu (%lu pokusov)\n",
           (unsigned long)new_block.nonce, (unsigned long)attempts);
    printf("  │ Hash:      %s\n", new_block.hash);
    printf("  │ Prev hash: %s\n", new_block.prev_hash);
    printf("  │ Odmena:    %lu coin\n",
           (unsigned long)new_block.transactions[0].amount);
    printf("  │\n");

    if (chain_add_block(&chain, &new_block) == 0) {
        /* Fáza 4: Broadcast (1-2s). */
        int peers;
        pthread_mutex_lock(&node.peers_lock);
        peers = node.peer_count;
        pthread_mutex_unlock(&node.peers_lock);

        if (peers > 0) {
            animate_spinner("Broadcastujem blok peerom...", 1500);
            printf("  │ Blok odoslaný %d peerom.\n", peers);
        }

        node_broadcast_block(&node, &new_block);

        printf("  │\n");
        printf("  └─ Blok #%u úspešne pridaný do chainu. ─────────────\n",
               new_block.index);
    } else {
        printf("  └─ Chyba: blok nebol pridaný (chain sa medzičasom zmenil?)\n");
    }
}

/*
 * Odoslanie transakcie.
 */
static void do_send(void)
{
    char receiver[ADDR_LEN];
    uint64_t amount, fee;

    printf("Adresa príjemcu: ");
    if (scanf("%64s", receiver) != 1) return;

    printf("Suma: ");
    if (scanf("%lu", &amount) != 1) return;

    printf("Poplatok (min. %d): ", MIN_TX_FEE);
    if (scanf("%lu", &fee) != 1) return;

    if (fee < MIN_TX_FEE) {
        printf("Poplatok musí byť aspoň %d coin.\n", MIN_TX_FEE);
        return;
    }

    /* Overíme zostatok (suma + poplatok). */
    int64_t balance = chain_get_balance(&chain, wallet.address);
    uint64_t total_cost = amount + fee;
    if (balance < (int64_t)total_cost) {
        printf("Nedostatočný zostatok (%ld < %lu).\n",
               (long)balance, (unsigned long)total_cost);
        return;
    }

    printf("\n");

    /* Fáza 1: Vytvorenie TX (1s). */
    animate_spinner("Vytváram transakciu...", 1000);

    tx_t tx = tx_create(wallet.address, receiver, amount, fee);

    /* Fáza 2: Podpisovanie (2s). */
    animate_spinner("Podpisujem transakciu privátnym kľúčom (Ed25519)...", 2000);

    if (wallet_sign_tx(&wallet, &tx) != 0) {
        printf("Chyba pri podpisovaní transakcie.\n");
        return;
    }
    printf("  Transakcia podpísaná.\n");

    /* Fáza 3: Mempool (1s). */
    animate_spinner("Pridávam do mempoolu...", 1000);

    pthread_mutex_lock(&mempool_lock);
    if (mempool_count < MEMPOOL_SIZE) {
        mempool[mempool_count++] = tx;
    }
    pthread_mutex_unlock(&mempool_lock);

    /* Fáza 4: Broadcast (2s). */
    int peers;
    pthread_mutex_lock(&node.peers_lock);
    peers = node.peer_count;
    pthread_mutex_unlock(&node.peers_lock);

    if (peers > 0) {
        animate_spinner("Broadcastujem transakciu na sieť...", 2000);
        printf("  Transakcia odoslaná %d peerom.\n", peers);
    }

    node_broadcast_tx(&node, &tx);

    printf("\n");
    tx_print(&tx);
    printf("\n  Celková cena: %lu coin (suma) + %lu coin (fee) = %lu coin\n",
           (unsigned long)amount, (unsigned long)fee,
           (unsigned long)total_cost);
    printf("\n  >>> Pre potvrdenie transakcie vyťaž blok [1]. <<<\n");
}

/*
 * Zobrazenie peňaženky.
 */
static void do_wallet_info(void)
{
    int64_t balance = chain_get_balance(&chain, wallet.address);

    printf("\n=== Peňaženka ===\n");
    printf("Adresa:   %s\n", wallet.address);
    printf("Zostatok: %ld coin\n", (long)balance);
    printf("Súbor:    %s\n", wallet_file);
}

/*
 * Pripojenie k peerovi.
 */
static void do_connect_peer(void)
{
    char host[256];
    int port;

    printf("Host (napr. localhost): ");
    if (scanf("%255s", host) != 1) return;

    printf("Port (napr. 9334): ");
    if (scanf("%d", &port) != 1) return;

    printf("\n");
    animate_spinner("Pripájam sa k peerovi...", 2000);

    if (node_connect_peer(&node, host, (uint16_t)port) == 0) {
        printf("  Pripojený k %s:%d.\n", host, port);
        animate_spinner("Synchronizujem blockchain...", 3000);
        printf("  Synchronizácia dokončená.\n");
    } else {
        printf("  Chyba pri pripájaní k %s:%d.\n", host, port);
    }
}

/*
 * Stav siete.
 */
static void do_network_status(void)
{
    printf("\n=== Stav siete ===\n");
    printf("Port: %d\n", node.port);

    pthread_mutex_lock(&node.peers_lock);
    printf("Peers: %d\n", node.peer_count);
    for (int i = 0; i < node.peer_count; i++) {
        printf("  [%d] %s:%d %s\n", i,
               node.peers[i].host, node.peers[i].port,
               node.peers[i].active ? "(aktívny)" : "(neaktívny)");
    }
    pthread_mutex_unlock(&node.peers_lock);

    printf("\nPre pripojenie cez SSH tunel:\n");
    printf("  ssh -L <local_port>:localhost:%d user@tento_stroj\n", node.port);
}

/*
 * Zobrazenie mempoolu.
 */
static void do_mempool(void)
{
    pthread_mutex_lock(&mempool_lock);
    printf("\n=== Mempool (%d transakcií) ===\n", mempool_count);
    for (int i = 0; i < mempool_count; i++) {
        tx_print(&mempool[i]);
    }
    pthread_mutex_unlock(&mempool_lock);
}

/*
 * Úhľadné ukončenie.
 */
static volatile int quit_flag = 0;

static void sigint_handler(int sig)
{
    (void)sig;
    quit_flag = 1;
    printf("\nUkončujem...\n");
}

int main(int argc, char *argv[])
{
    uint16_t port = DEFAULT_PORT;

    if (argc > 1) {
        port = (uint16_t)atoi(argv[1]);
    }

    /* SIGINT handler pre čisté ukončenie. */
    signal(SIGINT, sigint_handler);

    printf("╔═══════════════════════════════════════════════════════╗\n");
    printf("║            MiniCoin — Decentralizovaná mena           ║\n");
    printf("║                    Demo implementácia                 ║\n");
    printf("╚═══════════════════════════════════════════════════════╝\n\n");

    /* Načítame alebo vytvoríme peňaženku. */
    if (wallet_load(&wallet, wallet_file) == 0) {
        animate_spinner("Načítavam peňaženku...", 1500);
        printf("  Peňaženka načítaná z %s\n", wallet_file);
    } else {
        animate_spinner("Generujem Ed25519 kľúčový pár...", 2500);
        if (wallet_create(&wallet) != 0) {
            fprintf(stderr, "Chyba pri vytváraní peňaženky.\n");
            return 1;
        }
        if (wallet_save(&wallet, wallet_file) == 0) {
            printf("  Nová peňaženka uložená do %s\n", wallet_file);
        }
    }
    printf("  Adresa: %s\n\n", wallet.address);

    /* Inicializácia blockchainu. */
    animate_spinner("Ťažím genesis blok...", 3000);
    chain_init(&chain);
    printf("  Genesis blok: %s\n\n", chain.blocks[0].hash);

    /* Spustíme sieťový node. */
    animate_spinner("Spúšťam TCP server...", 1500);
    if (node_init(&node, port, &chain) != 0) {
        fprintf(stderr, "Chyba pri štarte node.\n");
        wallet_destroy(&wallet);
        chain_destroy(&chain);
        return 1;
    }
    node.on_new_tx = on_new_tx;
    node.on_peer_connected = on_peer_connected;
    printf("\n");

    /* Hlavná slučka — interaktívne menu. */
    char choice[16];
    while (!quit_flag) {
        print_menu();

        if (scanf("%15s", choice) != 1) break;

        if (strcmp(choice, "1") == 0) {
            do_mine();
        } else if (strcmp(choice, "2") == 0) {
            do_send();
        } else if (strcmp(choice, "3") == 0) {
            chain_print(&chain);
        } else if (strcmp(choice, "4") == 0) {
            do_wallet_info();
        } else if (strcmp(choice, "5") == 0) {
            do_connect_peer();
        } else if (strcmp(choice, "6") == 0) {
            do_network_status();
        } else if (strcmp(choice, "7") == 0) {
            do_mempool();
        } else if (strcmp(choice, "q") == 0 || strcmp(choice, "Q") == 0) {
            break;
        } else {
            printf("Neznáma voľba.\n");
        }
    }

    /* Ukončenie. */
    printf("Zastavujem node...\n");
    node_stop(&node);
    wallet_destroy(&wallet);
    chain_destroy(&chain);
    printf("Hotovo.\n");

    return 0;
}
