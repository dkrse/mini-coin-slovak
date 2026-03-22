/*
 * wallet.c — Implementácia peňaženky (Ed25519 kľúče, podpisovanie).
 */

#include <stdio.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include "wallet.h"

/*
 * Extrahuje public key z EVP_PKEY a uloží ho ako hex reťazec do addr.
 */
static int extract_address(EVP_PKEY *key, char *addr)
{
    unsigned char pubkey[32];
    size_t len = sizeof(pubkey);

    if (EVP_PKEY_get_raw_public_key(key, pubkey, &len) != 1) {
        fprintf(stderr, "Chyba pri extrakcii public key.\n");
        return -1;
    }

    /* Prvých 32 bajtov public key ako hex = 64 znakov (zmestí sa do ADDR_LEN). */
    for (size_t i = 0; i < len; i++) {
        sprintf(addr + (i * 2), "%02x", pubkey[i]);
    }
    addr[len * 2] = '\0';

    return 0;
}

int wallet_create(wallet_t *wallet)
{
    EVP_PKEY *pkey = NULL;
    EVP_PKEY_CTX *ctx;

    memset(wallet, 0, sizeof(*wallet));

    ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_ED25519, NULL);
    if (!ctx) {
        goto err;
    }

    if (EVP_PKEY_keygen_init(ctx) <= 0) {
        goto err;
    }

    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) {
        goto err;
    }

    EVP_PKEY_CTX_free(ctx);

    wallet->keypair = pkey;
    if (extract_address(pkey, wallet->address) != 0) {
        EVP_PKEY_free(pkey);
        return -1;
    }

    return 0;

err:
    fprintf(stderr, "Chyba pri generovaní kľúčov: %s\n",
            ERR_error_string(ERR_get_error(), NULL));
    if (ctx) EVP_PKEY_CTX_free(ctx);
    if (pkey) EVP_PKEY_free(pkey);
    return -1;
}

void wallet_destroy(wallet_t *wallet)
{
    if (wallet->keypair) {
        EVP_PKEY_free(wallet->keypair);
        wallet->keypair = NULL;
    }
}

int wallet_sign_tx(const wallet_t *wallet, tx_t *tx)
{
    EVP_MD_CTX *md_ctx;
    unsigned char sig[64];
    size_t sig_len = sizeof(sig);

    md_ctx = EVP_MD_CTX_new();
    if (!md_ctx) {
        return -1;
    }

    if (EVP_DigestSignInit(md_ctx, NULL, NULL, NULL, wallet->keypair) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return -1;
    }

    /* Podpisujeme tx_hash. */
    if (EVP_DigestSign(md_ctx, sig, &sig_len,
                       (unsigned char *)tx->tx_hash,
                       strlen(tx->tx_hash)) != 1) {
        EVP_MD_CTX_free(md_ctx);
        return -1;
    }

    EVP_MD_CTX_free(md_ctx);

    /* Uložíme podpis ako hex. */
    for (size_t i = 0; i < sig_len; i++) {
        sprintf(tx->signature + (i * 2), "%02x", sig[i]);
    }
    tx->signature[sig_len * 2] = '\0';

    return 0;
}

int wallet_verify_tx(const tx_t *tx)
{
    /* Coinbase transakcie nepotrebujú verifikáciu podpisu. */
    if (strcmp(tx->sender, COINBASE_ADDR) == 0) {
        return 0;
    }

    /*
     * Pre plnú verifikáciu by sme potrebovali public key odosielateľa.
     * V zjednodušenej verzii adresa JE public key (hex), takže ho
     * vieme rekonštruovať.
     */
    unsigned char pubkey[32];
    size_t pubkey_len = 32;

    /* Dekódujeme hex adresu na bajty. */
    for (size_t i = 0; i < 32; i++) {
        unsigned int byte;
        if (sscanf(tx->sender + (i * 2), "%02x", &byte) != 1) {
            return -1;
        }
        pubkey[i] = (unsigned char)byte;
    }

    /* Vytvoríme EVP_PKEY z raw public key. */
    EVP_PKEY *pkey = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL,
                                                   pubkey, pubkey_len);
    if (!pkey) {
        return -1;
    }

    /* Dekódujeme hex podpis na bajty. */
    unsigned char sig[64];
    size_t sig_len = strlen(tx->signature) / 2;
    if (sig_len > sizeof(sig)) sig_len = sizeof(sig);

    for (size_t i = 0; i < sig_len; i++) {
        unsigned int byte;
        if (sscanf(tx->signature + (i * 2), "%02x", &byte) != 1) {
            EVP_PKEY_free(pkey);
            return -1;
        }
        sig[i] = (unsigned char)byte;
    }

    /* Verifikácia. */
    EVP_MD_CTX *md_ctx = EVP_MD_CTX_new();
    int ret = -1;

    if (EVP_DigestVerifyInit(md_ctx, NULL, NULL, NULL, pkey) != 1) {
        goto cleanup;
    }

    if (EVP_DigestVerify(md_ctx, sig, sig_len,
                         (unsigned char *)tx->tx_hash,
                         strlen(tx->tx_hash)) == 1) {
        ret = 0;
    }

cleanup:
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);
    return ret;
}

int wallet_save(const wallet_t *wallet, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) {
        perror("wallet_save: fopen");
        return -1;
    }

    if (PEM_write_PrivateKey(f, wallet->keypair, NULL, NULL, 0, NULL, NULL) != 1) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

int wallet_load(wallet_t *wallet, const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        return -1;
    }

    memset(wallet, 0, sizeof(*wallet));
    wallet->keypair = PEM_read_PrivateKey(f, NULL, NULL, NULL);
    fclose(f);

    if (!wallet->keypair) {
        return -1;
    }

    return extract_address(wallet->keypair, wallet->address);
}
