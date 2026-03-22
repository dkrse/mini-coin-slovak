# MiniCoin — Diagramy

## 1. Životný cyklus transakcie

```mermaid
sequenceDiagram
    participant A as Node A (odosielateľ)
    participant MA as Mempool A
    participant B as Node B (príjemca)
    participant MB as Mempool B
    participant T as Ťažiar (hociktorý node)

    A->>A: Vytvorí TX (sender, receiver, amount, fee)
    A->>A: Podpíše TX privátnym kľúčom (Ed25519)
    A->>MA: Pridá TX do lokálneho mempoolu
    A->>B: Broadcastuje TX cez TCP

    B->>MB: Pridá TX do mempoolu

    Note over MA,MB: TX čaká v mempoole na potvrdenie...<br/>Bez ťaženia sa transakcia NEPOTVRDÍ.

    T->>T: Začne ťažiť nový blok
    T->>T: Zoberie TX z mempoolu
    T->>T: Overí zostatok odosielateľa (amount + fee ≤ balance)
    T->>T: Vytvorí coinbase TX (50 odmena + fees)
    T->>T: Mining: hľadá nonce kde SHA-256 začína na "00"
    T->>T: Nonce nájdený → blok vyťažený

    T->>A: Broadcastuje nový blok
    T->>B: Broadcastuje nový blok

    A->>A: Validuje blok, pridá do chainu
    B->>B: Validuje blok, pridá do chainu

    Note over A,B: TX je potvrdená. Zostatky aktualizované.
```

## 2. Pripojenie peera — synchronizácia chain + mempool

```mermaid
sequenceDiagram
    participant A as Node A (existujúci)
    participant B as Node B (nový)

    Note over A: Má blockchain + TX v mempoole

    B->>A: TCP connect
    A->>B: REQUEST_CHAIN
    B->>A: REQUEST_CHAIN
    A->>B: CHAIN_RESPONSE (celý blockchain)
    B->>A: CHAIN_RESPONSE (celý blockchain)

    Note over A,B: Oba porovnajú chain → dlhší vyhráva

    A->>B: NEW_TX (TX #1 z mempoolu)
    A->>B: NEW_TX (TX #2 z mempoolu)
    A->>B: NEW_TX (TX #N z mempoolu)

    Note over B: Teraz má blockchain AJ čakajúce TX.<br/>Môže ťažiť a potvrdiť transakcie.
```

## 3. Scenár s 3 nodes (LAN + VPN)

```mermaid
sequenceDiagram
    participant A as Node A (LAN)
    participant C as Node C (VPN/Tailscale)
    participant B as Node B (LAN)

    Note over A,C: Fáza 1: Pripojenie A→C

    A->>C: TCP connect (cez Tailscale IP)
    A-->C: Chain sync (oba majú len genesis)

    Note over A: Fáza 2: Ťaženie

    A->>A: Ťaží blok #1 → 50 coin
    A->>C: NEW_BLOCK (#1)

    Note over A: Fáza 3: Transakcia A→C

    A->>A: Vytvorí TX: 5 coin + 2 fee
    A->>C: NEW_TX
    C->>C: Pridá do mempoolu

    Note over B: Fáza 4: B sa pripojí

    B->>C: TCP connect
    C->>B: CHAIN_RESPONSE (2 bloky)
    C->>B: NEW_TX (čakajúca TX z mempoolu)

    Note over B: Fáza 5: B ťaží

    B->>B: Ťaží blok #2 (coinbase 50 + fee 2 = 52)
    B->>C: NEW_BLOCK (#2)
    C->>A: NEW_BLOCK (#2)

    Note over A,B: Výsledok: A=43, C=5, B=52
```

## 4. Štruktúra bloku

```mermaid
graph TD
    subgraph "Block #N"
        I[index: N]
        TS[timestamp]
        PH[prev_hash: hash bloku N-1]
        NC[nonce: číslo nájdené miningom]
        H[hash: SHA-256 celého bloku]

        subgraph "Transakcie"
            TX0["TX 0: COINBASE → ťažiar: 50 + fees"]
            TX1["TX 1: Janko → Eva: 10 coin (fee: 1)"]
            TX2["TX 2: Eva → Marienka: 5 coin (fee: 1)"]
        end
    end

    PH -.->|"ukazuje na"| PREV[Block #N-1 hash]

    style TX0 fill:#2d5,stroke:#333
    style H fill:#25d,stroke:#333
    style PH fill:#d52,stroke:#333
```

## 5. Blockchain — reťazec blokov

```mermaid
graph LR
    subgraph G["Genesis Block #0"]
        G_PH["prev: 000...000"]
        G_H["hash: 00005f2f..."]
        G_N["nonce: 873"]
    end

    subgraph B1["Block #1"]
        B1_PH["prev: 00005f2f..."]
        B1_H["hash: 00182d93..."]
        B1_TX["COINBASE → Miner: 50"]
    end

    subgraph B2["Block #2"]
        B2_PH["prev: 00182d93..."]
        B2_H["hash: 0007c2c0..."]
        B2_TX["COINBASE → Miner: 52<br/>Janko → Eva: 5 (fee: 2)"]
    end

    G -->|"hash → prev_hash"| B1
    B1 -->|"hash → prev_hash"| B2
    B2 -->|"..."| B3["Block #3"]
```

## 6. Mining (Proof of Work)

```mermaid
flowchart TD
    A[Začni ťažiť] --> B[Nastav nonce = 0]
    B --> C["Vytvor reťazec: index + timestamp + prev_hash + nonce + TX hashe"]
    C --> D["Vypočítaj SHA-256(reťazec)"]
    D --> E{"Hash začína na '00'?"}
    E -->|Nie| F[nonce++]
    F --> C
    E -->|Áno| G["Blok vyťažený!"]
    G --> H[Pridaj do chainu]
    H --> I[Broadcastuj peerom]

    style G fill:#2d5,stroke:#333
    style E fill:#d92,stroke:#333
```

## 7. Konsenzus — Longest Chain Wins

```mermaid
flowchart TD
    A["Prijatý nový chain od peera"] --> B{"Je dlhší ako lokálny?"}
    B -->|Nie| C["Ignoruj — náš chain je dlhší/rovnaký"]
    B -->|Áno| D{"Je platný?<br/>(validácia všetkých blokov)"}
    D -->|Nie| E["Odmietni — neplatný chain"]
    D -->|Áno| F["Nahraď lokálny chain"]

    F --> G["Offline bloky sa stratia!"]

    style C fill:#d52,stroke:#333
    style E fill:#d52,stroke:#333
    style F fill:#2d5,stroke:#333
    style G fill:#d92,stroke:#333
```

## 8. Offline ťaženie — prečo nefunguje

```mermaid
sequenceDiagram
    participant O as Offline node
    participant S as Sieť (ostatné nodes)

    Note over O: Ťaží offline 3 bloky

    O->>O: Block #1 (offline)
    O->>O: Block #2 (offline)
    O->>O: Block #3 (offline)

    Note over S: Sieť medzitým vyťažila 10 blokov

    O->>S: Pripojí sa k sieti
    S->>O: CHAIN_RESPONSE (10 blokov)

    Note over O: Sieť má dlhší chain (10 > 3)<br/>→ lokálny chain sa NAHRADÍ<br/>→ offline bloky a coiny ZMIZNÚ

    O->>O: Chain nahradený, zostatok = 0
```

## 8b. Lokálne ťaženie vs. sieť — porovnanie chainov

```mermaid
graph TD
    subgraph "Offline node — ťažil 3 bloky"
        OG["Genesis #0"] --> O1["Block #1<br/>COINBASE → Ja: 50"]
        O1 --> O2["Block #2<br/>COINBASE → Ja: 50"]
        O2 --> O3["Block #3<br/>COINBASE → Ja: 50"]
        BAL_O["Môj zostatok: 150 coin"]
    end

    subgraph "Sieť — vyťažila 7 blokov"
        SG["Genesis #0"] --> S1["Block #1"]
        S1 --> S2["Block #2"]
        S2 --> S3["Block #3"]
        S3 --> S4["Block #4"]
        S4 --> S5["Block #5"]
        S5 --> S6["Block #6"]
        S6 --> S7["Block #7"]
    end

    subgraph "Po pripojení"
        R["Sieť má 7 blokov > moje 3 bloky"]
        R --> D["Môj chain sa NAHRADÍ sieťovým"]
        D --> L["Moje offline bloky ZMIZLI"]
        L --> Z["Môj zostatok: 0 coin"]
    end

    style O1 fill:#d52,stroke:#333
    style O2 fill:#d52,stroke:#333
    style O3 fill:#d52,stroke:#333
    style BAL_O fill:#d52,stroke:#333
    style Z fill:#d52,stroke:#333
    style S7 fill:#2d5,stroke:#333
```

**Prečo sa to stane?** Konsenzus pravidlo „longest chain wins" existuje preto,
aby sa sieť vždy zhodla na jednej verzii pravdy. Jeden počítač nemá šancu
prekonať výkon celej siete, preto sa oplatí vždy najprv pripojiť a až potom ťažiť.

V reálnom Bitcoine je to rovnaký princíp. Preto existujú mining pooly 
ťažiari spájajú výkon, aby mali väčšiu šancu nájsť blok. Sólo ťaženie
na jednom počítači je dnes prakticky bezvýznamné.

## 9. Peer-to-Peer sieť — topológia

```mermaid
graph TD
    A["Node A<br/>192.168.1.100:9333<br/>(LAN)"] <-->|TCP| B["Node B<br/>192.168.1.110:9333<br/>(LAN)"]
    A <-->|TCP| C["Node C<br/>100.64.0.2:9333<br/>(Tailscale/VPN)"]
    B <-->|TCP| C

    style A fill:#25d,stroke:#333,color:#fff
    style B fill:#25d,stroke:#333,color:#fff
    style C fill:#d92,stroke:#333,color:#fff
```

Nodes sa pripájajú priamo cez TCP. V LAN cez lokálnu IP,
cez VPN (Tailscale, WireGuard) cez VPN IP adresu.
Nie je potrebný žiadny tunel ani špeciálna konfigurácia.

**Dôležité**: medzi dvoma nodes by malo byť len jedno spojenie.
Pripojenie cez dve cesty (LAN + VPN) spôsobí duplicitné správy.

## 10. Tok peňazí — príklad

```mermaid
flowchart LR
    subgraph "Block #1 (ťaží Janko)"
        CB1["COINBASE → Janko: 50"]
    end

    subgraph "Block #2 (ťaží Eva)"
        CB2["COINBASE → Eva: 50 + 1 fee = 51"]
        TX1["Janko → Eva: 10 (fee: 1)"]
    end

    subgraph "Block #3 (ťaží Janko)"
        CB3["COINBASE → Janko: 50 + 2 fees = 52"]
        TX2["Eva → Marienka: 5 (fee: 1)"]
        TX3["Eva → Janko: 3 (fee: 1)"]
    end

    subgraph "Zostatky po Block #3"
        ZA["Janko: 50 - 10 - 1 + 52 + 3 = 94"]
        ZB["Eva: 51 + 10 - 5 - 1 - 3 - 1 = 51"]
        ZC["Marienka: 5"]
    end

    style ZA fill:#2d5,stroke:#333
    style ZB fill:#25d,stroke:#333
    style ZC fill:#d92,stroke:#333
```

## 11. Prečo sú fees dôležité

```mermaid
flowchart TD
    TX["Transakcia odoslaná"] --> MP["Čaká v mempoole"]
    MP --> Q{"Ťaží niekto?"}
    Q -->|Nie| WAIT["TX čaká navždy...<br/>Zostatky sa nezmenia"]
    Q -->|Áno| MINE["Ťažiar vyberie TX z mempoolu"]
    MINE --> FEE{"Má TX fee?"}
    FEE -->|"Vysoký fee"| PRIORITY["Zahrnutá prednostne"]
    FEE -->|"Nízky fee"| LATER["Zahrnutá neskôr"]
    FEE -->|"Žiadny fee"| SKIP["Ignorovaná (min. fee: 1)"]
    PRIORITY --> CONFIRM["TX potvrdená v bloku"]
    LATER --> CONFIRM

    style WAIT fill:#d52,stroke:#333
    style SKIP fill:#d52,stroke:#333
    style CONFIRM fill:#2d5,stroke:#333
    style PRIORITY fill:#2d5,stroke:#333
```

## 12. Štruktúra transakcie a podpisovanie

```mermaid
flowchart TD
    subgraph "Vytvorenie TX"
        S["sender: abc5f7e1..."]
        R["receiver: 9314ff8b..."]
        AM["amount: 10"]
        FE["fee: 1"]
    end

    S & R & AM & FE --> HASH["SHA-256(sender + receiver + amount + fee)"]
    HASH --> TXH["tx_hash: d4e5f6..."]

    subgraph "Podpisovanie"
        PK["Privátny kľúč (Ed25519)"]
        PK --> SIGN["Ed25519 Sign(tx_hash)"]
        TXH --> SIGN
        SIGN --> SIG["signature: a1b2c3..."]
    end

    subgraph "Verifikácia"
        ADDR["sender adresa = public key"]
        ADDR --> VER["Ed25519 Verify(tx_hash, signature, public_key)"]
        TXH --> VER
        SIG --> VER
        VER --> OK{"Platný?"}
    end

    style OK fill:#2d5,stroke:#333
```

## 13. Merkle Tree (reálny Bitcoin) vs. MiniCoin

```mermaid
graph TD
    subgraph "Bitcoin — Merkle Tree"
        ROOT["Root Hash (v hlavičke bloku)"]
        HAB["Hash(A+B)"]
        HCD["Hash(C+D)"]
        HA["Hash(TX A)"]
        HB["Hash(TX B)"]
        HC["Hash(TX C)"]
        HD["Hash(TX D)"]

        ROOT --> HAB
        ROOT --> HCD
        HAB --> HA
        HAB --> HB
        HCD --> HC
        HCD --> HD
    end

    subgraph "MiniCoin — priame zreťazenie"
        BLOCK["SHA-256(index + timestamp + prev_hash + nonce + hash_A + hash_B + hash_C + hash_D)"]
    end

    style ROOT fill:#2d5,stroke:#333
    style BLOCK fill:#d92,stroke:#333
```

Merkle tree umožňuje overiť jednu TX bez stiahnutia celého bloku (SPV proof).
MiniCoin to nepotrebuje, každý node má celý blockchain.
