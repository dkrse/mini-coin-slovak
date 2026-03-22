/*
 * tx.c — Implementácia transakcií.
 */

#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include "tx.h"

void tx_compute_hash(tx_t *tx)
{
    EVP_MD_CTX *ctx;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    char buf[512];

    /* Vytvoríme reťazec z obsahu transakcie (vrátane fee). */
    snprintf(buf, sizeof(buf), "%s%s%lu%lu",
             tx->sender, tx->receiver,
             (unsigned long)tx->amount, (unsigned long)tx->fee);

    ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, buf, strlen(buf));
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    /* Konverzia na hex reťazec. */
    for (unsigned int i = 0; i < digest_len; i++) {
        sprintf(tx->tx_hash + (i * 2), "%02x", digest[i]);
    }
    tx->tx_hash[digest_len * 2] = '\0';
}

tx_t tx_create_coinbase(const char *miner_addr)
{
    tx_t tx;

    memset(&tx, 0, sizeof(tx));
    strncpy(tx.sender, COINBASE_ADDR, ADDR_LEN - 1);
    strncpy(tx.receiver, miner_addr, ADDR_LEN - 1);
    tx.amount = MINING_REWARD;
    tx.fee = 0;
    strcpy(tx.signature, "COINBASE");
    tx_compute_hash(&tx);

    return tx;
}

tx_t tx_create(const char *sender, const char *receiver, uint64_t amount,
               uint64_t fee)
{
    tx_t tx;

    memset(&tx, 0, sizeof(tx));
    strncpy(tx.sender, sender, ADDR_LEN - 1);
    strncpy(tx.receiver, receiver, ADDR_LEN - 1);
    tx.amount = amount;
    tx.fee = fee;
    tx_compute_hash(&tx);

    return tx;
}

void tx_print(const tx_t *tx)
{
    if (strcmp(tx->sender, COINBASE_ADDR) == 0) {
        printf("  [COINBASE] -> %s : %lu coin\n",
               tx->receiver, (unsigned long)tx->amount);
    } else {
        printf("  %s -> %s : %lu coin (poplatok: %lu)\n",
               tx->sender, tx->receiver,
               (unsigned long)tx->amount, (unsigned long)tx->fee);
    }
}
