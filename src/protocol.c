/*
 * protocol.c — Serializácia/deserializácia správ (manuálny JSON).
 *
 * Formát je úmyselne jednoduchý — nie produkčný JSON parser,
 * ale postačujúci pre demo. Správy sú jednoriadkové JSON zakončené '\n'.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"

/* Pomocná funkcia — escapuje reťazec pre JSON. */
static void json_escape(const char *src, char *dst, size_t dst_size)
{
    size_t j = 0;
    for (size_t i = 0; src[i] && j < dst_size - 2; i++) {
        if (src[i] == '"' || src[i] == '\\') {
            dst[j++] = '\\';
        }
        dst[j++] = src[i];
    }
    dst[j] = '\0';
}

/* Pomocná funkcia — nájde hodnotu kľúča v JSON (jednoduché, nie rekurzívne). */
static const char *json_find_key(const char *json, const char *key)
{
    char search[128];
    snprintf(search, sizeof(search), "\"%s\":", key);
    const char *p = strstr(json, search);
    if (!p) {
        /* Skúsime s medzerou pred dvojbodkou. */
        snprintf(search, sizeof(search), "\"%s\" :", key);
        p = strstr(json, search);
    }
    if (!p) return NULL;
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Pomocná funkcia — extrahuje string hodnotu. */
static int json_get_string(const char *json, const char *key,
                            char *out, size_t out_size)
{
    const char *p = json_find_key(json, key);
    if (!p || *p != '"') return -1;
    p++; /* Preskočíme úvodzovku. */

    size_t i = 0;
    while (*p && *p != '"' && i < out_size - 1) {
        if (*p == '\\' && *(p + 1)) {
            p++;
        }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 0;
}

/* Pomocná funkcia — extrahuje integer hodnotu. */
static int json_get_uint64(const char *json, const char *key, uint64_t *out)
{
    const char *p = json_find_key(json, key);
    if (!p) return -1;
    *out = strtoull(p, NULL, 10);
    return 0;
}

static int json_get_int(const char *json, const char *key, int *out)
{
    const char *p = json_find_key(json, key);
    if (!p) return -1;
    *out = (int)strtol(p, NULL, 10);
    return 0;
}

char *protocol_serialize_tx(const tx_t *tx)
{
    char *buf = malloc(1024);
    if (!buf) return NULL;

    char sender_esc[ADDR_LEN * 2], receiver_esc[ADDR_LEN * 2];
    json_escape(tx->sender, sender_esc, sizeof(sender_esc));
    json_escape(tx->receiver, receiver_esc, sizeof(receiver_esc));

    snprintf(buf, 1024,
        "{\"sender\":\"%s\",\"receiver\":\"%s\",\"amount\":%lu,"
        "\"fee\":%lu,\"signature\":\"%s\",\"tx_hash\":\"%s\"}",
        sender_esc, receiver_esc, (unsigned long)tx->amount,
        (unsigned long)tx->fee, tx->signature, tx->tx_hash);

    return buf;
}

int protocol_deserialize_tx(const char *json, tx_t *tx)
{
    memset(tx, 0, sizeof(*tx));

    if (json_get_string(json, "sender", tx->sender, ADDR_LEN) != 0) return -1;
    if (json_get_string(json, "receiver", tx->receiver, ADDR_LEN) != 0) return -1;
    if (json_get_uint64(json, "amount", &tx->amount) != 0) return -1;
    json_get_uint64(json, "fee", &tx->fee); /* Fee môže byť 0 (coinbase). */
    json_get_string(json, "signature", tx->signature, SIG_LEN);
    json_get_string(json, "tx_hash", tx->tx_hash, sizeof(tx->tx_hash));

    return 0;
}

char *protocol_serialize_block(const block_t *block)
{
    /* Serializujeme transakcie ako pole. */
    char txs_buf[8192] = "[";
    size_t offset = 1;

    for (int i = 0; i < block->tx_count; i++) {
        char *tx_json = protocol_serialize_tx(&block->transactions[i]);
        if (!tx_json) return NULL;

        if (i > 0) {
            offset += snprintf(txs_buf + offset, sizeof(txs_buf) - offset, ",");
        }
        offset += snprintf(txs_buf + offset, sizeof(txs_buf) - offset,
                           "%s", tx_json);
        free(tx_json);
    }
    snprintf(txs_buf + offset, sizeof(txs_buf) - offset, "]");

    char *buf = malloc(16384);
    if (!buf) return NULL;

    snprintf(buf, 16384,
        "{\"index\":%u,\"timestamp\":%ld,\"prev_hash\":\"%s\","
        "\"nonce\":%lu,\"hash\":\"%s\",\"tx_count\":%d,"
        "\"transactions\":%s}",
        block->index, (long)block->timestamp, block->prev_hash,
        (unsigned long)block->nonce, block->hash, block->tx_count,
        txs_buf);

    return buf;
}

int protocol_deserialize_block(const char *json, block_t *block)
{
    uint64_t tmp;

    memset(block, 0, sizeof(*block));

    if (json_get_uint64(json, "index", &tmp) != 0) return -1;
    block->index = (uint32_t)tmp;

    if (json_get_uint64(json, "timestamp", &tmp) != 0) return -1;
    block->timestamp = (time_t)tmp;

    if (json_get_string(json, "prev_hash", block->prev_hash, HASH_HEX_LEN) != 0)
        return -1;

    if (json_get_uint64(json, "nonce", &block->nonce) != 0) return -1;
    if (json_get_string(json, "hash", block->hash, HASH_HEX_LEN) != 0) return -1;
    if (json_get_int(json, "tx_count", &block->tx_count) != 0) return -1;

    /* Parsovanie poľa transakcií. */
    const char *txs_start = json_find_key(json, "transactions");
    if (txs_start && *txs_start == '[') {
        txs_start++;
        for (int i = 0; i < block->tx_count && i < MAX_TX_PER_BLOCK; i++) {
            /* Nájdeme začiatok objektu. */
            const char *obj_start = strchr(txs_start, '{');
            if (!obj_start) break;

            /* Nájdeme koniec objektu (jednoúrovňový). */
            int depth = 0;
            const char *p = obj_start;
            do {
                if (*p == '{') depth++;
                else if (*p == '}') depth--;
                p++;
            } while (depth > 0 && *p);

            /* Skopírujeme JSON transakcie. */
            size_t obj_len = p - obj_start;
            char *tx_json = malloc(obj_len + 1);
            if (!tx_json) return -1;
            memcpy(tx_json, obj_start, obj_len);
            tx_json[obj_len] = '\0';

            protocol_deserialize_tx(tx_json, &block->transactions[i]);
            free(tx_json);

            txs_start = p;
        }
    }

    return 0;
}

char *protocol_serialize_chain(const blockchain_t *chain)
{
    /* Odhadneme veľkosť: ~16KB na blok. */
    size_t buf_size = chain->length * 16384 + 256;
    char *buf = malloc(buf_size);
    if (!buf) return NULL;

    size_t offset = snprintf(buf, buf_size, "{\"length\":%d,\"blocks\":[",
                              chain->length);

    for (int i = 0; i < chain->length; i++) {
        char *block_json = protocol_serialize_block(&chain->blocks[i]);
        if (!block_json) {
            free(buf);
            return NULL;
        }

        if (i > 0) {
            offset += snprintf(buf + offset, buf_size - offset, ",");
        }
        offset += snprintf(buf + offset, buf_size - offset, "%s", block_json);
        free(block_json);
    }

    snprintf(buf + offset, buf_size - offset, "]}");
    return buf;
}

int protocol_deserialize_chain(const char *json, blockchain_t *chain)
{
    int length;

    if (json_get_int(json, "length", &length) != 0) return -1;
    if (length <= 0 || length > MAX_BLOCKS) return -1;

    /* Nájdeme pole blokov. */
    const char *blocks_start = json_find_key(json, "blocks");
    if (!blocks_start || *blocks_start != '[') return -1;
    blocks_start++;

    chain->length = 0;
    for (int i = 0; i < length; i++) {
        const char *obj_start = strchr(blocks_start, '{');
        if (!obj_start) break;

        /* Nájdeme koniec bloku — musíme počítať vnorené {}. */
        int depth = 0;
        const char *p = obj_start;
        do {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            p++;
        } while (depth > 0 && *p);

        size_t obj_len = p - obj_start;
        char *block_json = malloc(obj_len + 1);
        if (!block_json) return -1;
        memcpy(block_json, obj_start, obj_len);
        block_json[obj_len] = '\0';

        if (protocol_deserialize_block(block_json, &chain->blocks[i]) != 0) {
            free(block_json);
            return -1;
        }
        free(block_json);

        chain->length++;
        blocks_start = p;
    }

    return 0;
}

char *protocol_serialize_msg(msg_type_t type, const char *payload)
{
    size_t payload_len = payload ? strlen(payload) : 2;
    char *buf = malloc(payload_len + 128);
    if (!buf) return NULL;

    snprintf(buf, payload_len + 128,
             "{\"type\":%d,\"payload\":%s}\n",
             (int)type, payload ? payload : "{}");

    return buf;
}

msg_type_t protocol_parse_msg(const char *json, char **out_payload)
{
    int type;

    if (json_get_int(json, "type", &type) != 0) {
        *out_payload = NULL;
        return -1;
    }

    /* Payload je všetko za "payload": — musíme správne spárovať zátvorky. */
    const char *p = json_find_key(json, "payload");
    if (p) {
        const char *start = p;
        int depth = 0;
        int in_string = 0;
        const char *q = start;

        /* Prejdeme celý payload s počítaním vnorených {} a []. */
        do {
            if (*q == '"' && (q == start || *(q - 1) != '\\')) {
                in_string = !in_string;
            } else if (!in_string) {
                if (*q == '{' || *q == '[') depth++;
                else if (*q == '}' || *q == ']') depth--;
            }
            q++;
        } while (depth > 0 && *q);

        size_t len = q - start;
        *out_payload = malloc(len + 1);
        if (*out_payload) {
            memcpy(*out_payload, start, len);
            (*out_payload)[len] = '\0';
        }
    } else {
        *out_payload = NULL;
    }

    return (msg_type_t)type;
}
