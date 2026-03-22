# Changelog

Všetky zmeny v projekte MiniCoin.

## [0.4.0] — 2026-03-22

### Pridané
- **Synchronizácia mempoolu pri pripojení peera**
  - Nový peer automaticky dostane všetky čakajúce TX z mempoolu
  - Callback `on_peer_connected` node pošle TX novému peerovi
  - Funkcia `node_send_tx()` na odoslanie TX konkrétnemu peerovi
  - Bez toho nový node nevedel o čakajúcich transakciách a nemohol ich potvrdiť ťažením
- **Animácie v CLI**
  - Spinner animácia pri generovaní kľúčov, genesis bloku, pripájaní
  - Mining animácia, zobrazuje meniace sa hashe a nonce v reálnom čase
  - Animácia pri odosielaní transakcií (podpisovanie, broadcast)
  - Animácia pri pripájaní k peerovi a synchronizácii

### Zmenené
- Dokumentácia aktualizovaná, pridané VPN/Tailscale scenáre, Merkle tree a Script vysvetlenia
- Odstránené zmienky o SSH tunelovaní (nie je potrebné)

## [0.3.0] — 2026-03-22

### Pridané
- **Transakčné poplatky (fees)**
  - Každá transakcia vyžaduje minimálny poplatok 1 coin
  - Ťažiar dostáva odmenu 50 coin + poplatky zo všetkých transakcií v bloku
  - Odosielateľovi sa strhne suma + poplatok
  - Výpis transakcie zobrazuje poplatok

### Zmenené
- `tx_t` štruktúra rozšírená o pole `fee`
- `tx_create()` prijíma parameter `fee`
- `tx_compute_hash()` zahŕňa fee do výpočtu hashu
- `chain_get_balance()` odpočítava fee od zostatku odosielateľa
- Coinbase transakcia obsahuje odmenu + súčet fees z bloku
- Protokol serializuje/deserializuje pole `fee`

## [0.2.0] — 2026-03-22

### Opravené
- **Segmentation fault pri pripojení peera**
  - `blockchain_t` sa alokuje na heape namiesto stacku (struct je príliš veľký)
  - `protocol_parse_msg()` správne parsuje vnorený JSON payload (počítanie zátvoriek)
- **Synchronizácia chainov pri pripojení**
  - Oba nodes si navzájom vyžiadajú chain (nie len pripájajúci sa node)
  - Nový blok sa nepreposiela späť odosielateľovi (prevencia slučiek)
- **Duplicitné bloky sa ticho ignorujú** (bez chybových hlášok)

### Zmenené
- `broadcast_block_except()` nová interná funkcia, blok sa neposiela späť peerovi od ktorého prišiel
- Chybové hlášky pri nesprávnom indexe bloku odstránené (bežná situácia pri synchronizácii)

## [0.1.0] — 2026-03-22

### Pridané
- **Blockchain**
  - Genesis blok s fixným timestampom (Bitcoin genesis: 2009-01-03)
  - SHA-256 hashing cez OpenSSL
  - Mining s difficulty 2 (hash musí začínať na "00")
  - Validácia celého chainu
  - Longest chain wins konsenzus
- **Transakcie**
  - Bežné transakcie (sender → receiver)
  - Coinbase transakcie (odmena za ťažbu: 50 coin)
  - UTXO validácia zostatkov
- **Peňaženka**
  - Ed25519 kľúčový pár cez OpenSSL
  - Adresa = hex-encoded public key (64 znakov)
  - Podpisovanie a verifikácia transakcií
  - Ukladanie/načítanie z PEM súboru
- **Sieť**
  - TCP server na konfigurovateľnom porte (predvolený: 9333)
  - Pripojenie k peerom cez host:port
  - Automatická synchronizácia chainu pri pripojení
  - Broadcast nových blokov a transakcií
- **Protokol**
  - JSON formát správ zakončených newline
  - Typy správ: NEW_BLOCK, NEW_TX, REQUEST_CHAIN, CHAIN_RESPONSE, PING, PONG
  - Serializácia/deserializácia blokov, transakcií a celého chainu
- **CLI**
  - Interaktívne menu s Unicode rámčekmi
  - Ťaženie blokov
  - Odosielanie transakcií
  - Zobrazenie blockchainu, peňaženky, mempoolu, stavu siete
  - Čisté ukončenie cez SIGINT (Ctrl+C)
- **Mempool**
  - Pole čakajúcich transakcií (max 100)
  - Thread-safe prístup cez mutex
