/*
 * block.h — Definícia bloku a funkcie pre hashing/mining.
 *
 * Blok obsahuje index, timestamp, zoznam transakcií, hash predchádzajúceho
 * bloku, nonce a výsledný hash. Mining hľadá nonce tak, aby hash začínal
 * požadovaným počtom núl (difficulty).
 */

#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>
#include <time.h>
#include "tx.h"

/* Maximálny počet transakcií v jednom bloku. */
#define MAX_TX_PER_BLOCK 10

/* Dĺžka SHA-256 hashu ako hex reťazec (64 znakov + '\0'). */
#define HASH_HEX_LEN 65

/* Difficulty — koľko vedúcich núl musí mať hash bloku. */
#define MINING_DIFFICULTY 2

typedef struct block {
    uint32_t    index;
    time_t      timestamp;
    tx_t        transactions[MAX_TX_PER_BLOCK];
    int         tx_count;
    char        prev_hash[HASH_HEX_LEN];
    uint64_t    nonce;
    char        hash[HASH_HEX_LEN];
} block_t;

/*
 * Vypočíta SHA-256 hash bloku z jeho obsahu (bez poľa hash).
 * Výsledok sa zapíše do out_hash (musí mať aspoň HASH_HEX_LEN bajtov).
 */
void block_compute_hash(const block_t *block, char *out_hash);

/*
 * Ťaží blok — hľadá nonce, pri ktorom hash začína na MINING_DIFFICULTY núl.
 * Naplní block->nonce a block->hash. Vracia počet pokusov.
 */
uint64_t block_mine(block_t *block);

/*
 * Overí, či je hash bloku platný (začína na požadovaný počet núl
 * a zodpovedá obsahu bloku).
 */
int block_validate(const block_t *block);

/*
 * Vytvorí genesis blok (index 0, žiadne transakcie, prev_hash samé nuly).
 */
block_t block_create_genesis(void);

/*
 * Vypíše blok na stdout v čitateľnom formáte.
 */
void block_print(const block_t *block);

#endif /* BLOCK_H */
