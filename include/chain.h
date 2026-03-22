/*
 * chain.h — Blockchain — linked list blokov s validáciou a konsenzu.
 *
 * Blockchain uchováva celý reťazec blokov. Podporuje pridávanie nových
 * blokov, validáciu celého chainu a nahradenie za dlhší platný chain
 * (longest chain wins).
 */

#ifndef CHAIN_H
#define CHAIN_H

#include <pthread.h>
#include "block.h"

/* Maximálny počet blokov v chainu (pre zjednodušenie statické pole). */
#define MAX_BLOCKS 10000

typedef struct blockchain {
    block_t     blocks[MAX_BLOCKS];
    int         length;
    pthread_mutex_t lock; /* Ochrana pri súbežnom prístupe (mining vs. sieť). */
} blockchain_t;

/*
 * Inicializuje blockchain s genesis blokom.
 */
void chain_init(blockchain_t *chain);

/*
 * Uvoľní prostriedky blockchainu (mutex).
 */
void chain_destroy(blockchain_t *chain);

/*
 * Vráti ukazovateľ na posledný blok v chainu.
 */
const block_t *chain_last_block(const blockchain_t *chain);

/*
 * Pridá nový blok do chainu. Blok musí byť platný a nadväzovať
 * na posledný blok. Vracia 0 pri úspechu, -1 pri chybe.
 * Thread-safe (zamkne mutex).
 */
int chain_add_block(blockchain_t *chain, const block_t *block);

/*
 * Validuje celý blockchain od genesis bloku.
 * Vracia 0 ak je platný, -1 ak nie.
 */
int chain_validate(const blockchain_t *chain);

/*
 * Vypočíta zostatok danej adresy prehľadaním všetkých transakcií.
 */
int64_t chain_get_balance(const blockchain_t *chain, const char *addr);

/*
 * Nahradí lokálny chain za iný, ak je dlhší a platný.
 * Vracia 0 ak bol nahradený, -1 ak nie.
 * Thread-safe.
 */
int chain_replace(blockchain_t *chain, const blockchain_t *other);

/*
 * Vypíše celý blockchain na stdout.
 */
void chain_print(const blockchain_t *chain);

#endif /* CHAIN_H */
