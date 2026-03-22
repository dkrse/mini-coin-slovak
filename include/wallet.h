/*
 * wallet.h — Peňaženka — generovanie kľúčov a podpisovanie transakcií.
 *
 * Používa Ed25519 cez OpenSSL pre generovanie kľúčového páru,
 * podpisovanie a overovanie transakcií.
 */

#ifndef WALLET_H
#define WALLET_H

#include <openssl/evp.h>
#include "tx.h"

typedef struct wallet {
    EVP_PKEY    *keypair;            /* Ed25519 kľúčový pár. */
    char        address[ADDR_LEN];   /* Hex-encoded public key (skrátený). */
} wallet_t;

/*
 * Vytvorí novú peňaženku — vygeneruje Ed25519 kľúčový pár.
 * Vracia 0 pri úspechu, -1 pri chybe.
 */
int wallet_create(wallet_t *wallet);

/*
 * Uvoľní prostriedky peňaženky.
 */
void wallet_destroy(wallet_t *wallet);

/*
 * Podpíše transakciu privátnym kľúčom peňaženky.
 * Zapíše podpis do tx->signature.
 * Vracia 0 pri úspechu, -1 pri chybe.
 */
int wallet_sign_tx(const wallet_t *wallet, tx_t *tx);

/*
 * Overí podpis transakcie pomocou public key odosielateľa.
 * Vracia 0 ak je podpis platný, -1 ak nie.
 */
int wallet_verify_tx(const tx_t *tx);

/*
 * Uloží peňaženku do súboru (PEM formát).
 */
int wallet_save(const wallet_t *wallet, const char *path);

/*
 * Načíta peňaženku zo súboru.
 */
int wallet_load(wallet_t *wallet, const char *path);

#endif /* WALLET_H */
