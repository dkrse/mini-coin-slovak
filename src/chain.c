/*
 * chain.c — Implementácia blockchainu.
 */

#include <stdio.h>
#include <string.h>
#include "chain.h"

void chain_init(blockchain_t *chain)
{
    memset(chain, 0, sizeof(*chain));
    pthread_mutex_init(&chain->lock, NULL);

    /* Vytvoríme genesis blok. */
    chain->blocks[0] = block_create_genesis();
    chain->length = 1;

    printf("Genesis blok vytvorený. Hash: %s\n",
           chain->blocks[0].hash);
}

void chain_destroy(blockchain_t *chain)
{
    pthread_mutex_destroy(&chain->lock);
}

const block_t *chain_last_block(const blockchain_t *chain)
{
    if (chain->length == 0) {
        return NULL;
    }
    return &chain->blocks[chain->length - 1];
}

int chain_add_block(blockchain_t *chain, const block_t *block)
{
    int ret = -1;

    pthread_mutex_lock(&chain->lock);

    if (chain->length >= MAX_BLOCKS) {
        fprintf(stderr, "Blockchain je plný.\n");
        goto out;
    }

    /* Blok musí nadväzovať na posledný. Tiché ignorovanie duplikátov. */
    const block_t *last = chain_last_block(chain);
    if (block->index != (uint32_t)(last->index + 1)) {
        goto out;
    }

    if (strcmp(block->prev_hash, last->hash) != 0) {
        fprintf(stderr, "Nesprávny prev_hash bloku.\n");
        goto out;
    }

    /* Validácia hashu bloku. */
    if (block_validate(block) != 0) {
        fprintf(stderr, "Neplatný hash bloku.\n");
        goto out;
    }

    chain->blocks[chain->length] = *block;
    chain->length++;
    ret = 0;

out:
    pthread_mutex_unlock(&chain->lock);
    return ret;
}

int chain_validate(const blockchain_t *chain)
{
    for (int i = 1; i < chain->length; i++) {
        const block_t *prev = &chain->blocks[i - 1];
        const block_t *curr = &chain->blocks[i];

        /* Musí nadväzovať na predchádzajúci blok. */
        if (strcmp(curr->prev_hash, prev->hash) != 0) {
            fprintf(stderr, "Chain neplatný na bloku #%d: prev_hash.\n", i);
            return -1;
        }

        /* Hash musí byť platný. */
        if (block_validate(curr) != 0) {
            fprintf(stderr, "Chain neplatný na bloku #%d: hash.\n", i);
            return -1;
        }
    }
    return 0;
}

int64_t chain_get_balance(const blockchain_t *chain, const char *addr)
{
    int64_t balance = 0;

    for (int i = 0; i < chain->length; i++) {
        const block_t *block = &chain->blocks[i];
        for (int j = 0; j < block->tx_count; j++) {
            const tx_t *tx = &block->transactions[j];
            if (strcmp(tx->receiver, addr) == 0) {
                balance += (int64_t)tx->amount;
            }
            if (strcmp(tx->sender, addr) == 0) {
                balance -= (int64_t)(tx->amount + tx->fee);
            }
        }
    }

    return balance;
}

int chain_replace(blockchain_t *chain, const blockchain_t *other)
{
    int ret = -1;

    pthread_mutex_lock(&chain->lock);

    /* Nahradíme iba ak je druhý chain dlhší a platný. */
    if (other->length <= chain->length) {
        goto out;
    }

    if (chain_validate(other) != 0) {
        fprintf(stderr, "Prijatý chain nie je platný.\n");
        goto out;
    }

    /* Skopírujeme bloky. */
    memcpy(chain->blocks, other->blocks,
           sizeof(block_t) * other->length);
    chain->length = other->length;
    ret = 0;

    printf("Chain nahradený. Nová dĺžka: %d blokov.\n", chain->length);

out:
    pthread_mutex_unlock(&chain->lock);
    return ret;
}

void chain_print(const blockchain_t *chain)
{
    printf("\n=== Blockchain (%d blokov) ===\n\n", chain->length);
    for (int i = 0; i < chain->length; i++) {
        block_print(&chain->blocks[i]);
        printf("\n");
    }
}
