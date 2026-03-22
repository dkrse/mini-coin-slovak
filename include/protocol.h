/*
 * protocol.h — Serializácia a deserializácia sieťových správ.
 *
 * Správy sa prenášajú ako JSON reťazce zakončené newline.
 * Tento modul konvertuje medzi C štruktúrami a JSON formátom.
 * JSON parsing je manuálny (bez externej knižnice) pre minimalizáciu
 * závislostí.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "block.h"
#include "chain.h"
#include "net.h"

/*
 * Serializuje blok do JSON reťazca.
 * Volajúci musí uvoľniť vrátený reťazec cez free().
 * Vracia NULL pri chybe.
 */
char *protocol_serialize_block(const block_t *block);

/*
 * Deserializuje blok z JSON reťazca.
 * Vracia 0 pri úspechu, -1 pri chybe.
 */
int protocol_deserialize_block(const char *json, block_t *block);

/*
 * Serializuje transakciu do JSON reťazca.
 */
char *protocol_serialize_tx(const tx_t *tx);

/*
 * Deserializuje transakciu z JSON reťazca.
 */
int protocol_deserialize_tx(const char *json, tx_t *tx);

/*
 * Serializuje celý blockchain do JSON.
 */
char *protocol_serialize_chain(const blockchain_t *chain);

/*
 * Deserializuje blockchain z JSON.
 */
int protocol_deserialize_chain(const char *json, blockchain_t *chain);

/*
 * Serializuje sieťovú správu (type + payload) do JSON.
 * Formát: {"type": <int>, "payload": <json>}\n
 */
char *protocol_serialize_msg(msg_type_t type, const char *payload);

/*
 * Parsuje typ správy z JSON reťazca.
 * Vracia typ správy, payload sa zapíše do out_payload (musí byť uvoľnený).
 */
msg_type_t protocol_parse_msg(const char *json, char **out_payload);

#endif /* PROTOCOL_H */
