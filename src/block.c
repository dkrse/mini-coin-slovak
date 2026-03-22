/*
 * block.c — Implementácia bloku, hashovania a ťaženia.
 */

#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include "block.h"

void block_compute_hash(const block_t *block, char *out_hash)
{
    EVP_MD_CTX *ctx;
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    char buf[4096];
    int offset;

    /* Zreťazíme všetky polia bloku (okrem hash) do jedného reťazca. */
    offset = snprintf(buf, sizeof(buf), "%u%ld%s%lu",
                      block->index, (long)block->timestamp,
                      block->prev_hash, (unsigned long)block->nonce);

    /* Pridáme hashe všetkých transakcií. */
    for (int i = 0; i < block->tx_count && offset < (int)sizeof(buf) - 65; i++) {
        offset += snprintf(buf + offset, sizeof(buf) - offset, "%s",
                           block->transactions[i].tx_hash);
    }

    ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_sha256(), NULL);
    EVP_DigestUpdate(ctx, buf, strlen(buf));
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    for (unsigned int i = 0; i < digest_len; i++) {
        sprintf(out_hash + (i * 2), "%02x", digest[i]);
    }
    out_hash[digest_len * 2] = '\0';
}

uint64_t block_mine(block_t *block)
{
    char hash[HASH_HEX_LEN];
    char target[MINING_DIFFICULTY + 1];
    uint64_t attempts = 0;

    /* Target je reťazec núl podľa difficulty. */
    memset(target, '0', MINING_DIFFICULTY);
    target[MINING_DIFFICULTY] = '\0';

    block->nonce = 0;
    do {
        block_compute_hash(block, hash);
        attempts++;
        if (strncmp(hash, target, MINING_DIFFICULTY) == 0) {
            strncpy(block->hash, hash, HASH_HEX_LEN);
            return attempts;
        }
        block->nonce++;
    } while (1);
}

int block_validate(const block_t *block)
{
    char computed[HASH_HEX_LEN];
    char target[MINING_DIFFICULTY + 1];

    block_compute_hash(block, computed);

    /* Hash musí zodpovedať uloženému. */
    if (strcmp(computed, block->hash) != 0) {
        return -1;
    }

    /* Hash musí začínať požadovaným počtom núl. */
    memset(target, '0', MINING_DIFFICULTY);
    target[MINING_DIFFICULTY] = '\0';
    if (strncmp(block->hash, target, MINING_DIFFICULTY) != 0) {
        return -1;
    }

    return 0;
}

block_t block_create_genesis(void)
{
    block_t genesis;

    memset(&genesis, 0, sizeof(genesis));
    genesis.index = 0;
    genesis.timestamp = 1231006505; /* Bitcoin genesis timestamp. */
    genesis.tx_count = 0;
    memset(genesis.prev_hash, '0', 64);
    genesis.prev_hash[64] = '\0';

    /* Ťažíme genesis blok. */
    block_mine(&genesis);

    return genesis;
}

void block_print(const block_t *block)
{
    printf("┌─ Block #%u ─────────────────────────────────────────────\n",
           block->index);
    printf("│ Čas:       %s", ctime(&block->timestamp));
    printf("│ Prev hash: %s\n", block->prev_hash);
    printf("│ Hash:      %s\n", block->hash);
    printf("│ Nonce:     %lu\n", (unsigned long)block->nonce);
    printf("│ Transakcie (%d):\n", block->tx_count);
    for (int i = 0; i < block->tx_count; i++) {
        printf("│ ");
        tx_print(&block->transactions[i]);
    }
    printf("└────────────────────────────────────────────────────────\n");
}
