/*
 * tx.h — Transakcie a zjednodušený UTXO model.
 *
 * Každá transakcia má odosielateľa, príjemcu, sumu a podpis.
 * Coinbase transakcia (odmena za ťažbu) má prázdneho odosielateľa.
 */

#ifndef TX_H
#define TX_H

#include <stdint.h>

/* Maximálna dĺžka adresy (hex-encoded public key skrátený). */
#define ADDR_LEN 65

/* Maximálna dĺžka podpisu (hex-encoded). */
#define SIG_LEN 129

/* Špeciálna adresa pre coinbase transakcie. */
#define COINBASE_ADDR "COINBASE"

/* Odmena za vyťaženie bloku. */
#define MINING_REWARD 50

/* Minimálny transakčný poplatok. */
#define MIN_TX_FEE 1

typedef struct tx {
    char        sender[ADDR_LEN];
    char        receiver[ADDR_LEN];
    uint64_t    amount;
    uint64_t    fee;             /* Transakčný poplatok pre ťažiara. */
    char        signature[SIG_LEN];
    char        tx_hash[65];     /* SHA-256 hash transakcie. */
} tx_t;

/*
 * Vypočíta hash transakcie z jej obsahu (sender, receiver, amount).
 * Výsledok sa zapíše do tx->tx_hash.
 */
void tx_compute_hash(tx_t *tx);

/*
 * Vytvorí coinbase transakciu (odmena pre ťažiara).
 */
tx_t tx_create_coinbase(const char *miner_addr);

/*
 * Vytvorí bežnú transakciu. Podpis sa pridá neskôr cez wallet.
 */
tx_t tx_create(const char *sender, const char *receiver, uint64_t amount,
               uint64_t fee);

/*
 * Vypíše transakciu na stdout.
 */
void tx_print(const tx_t *tx);

#endif /* TX_H */
