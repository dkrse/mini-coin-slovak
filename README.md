# MiniCoin — Decentralizovaná mena

Zjednodušená implementácia kryptomeny inšpirovanej Bitcoinom, napísaná v C.
Určená na demonštráciu princípov blockchainu, proof-of-work, digitálnych podpisov
a peer-to-peer siete.

Nodes sa pripájajú cez TCP a tvoria decentralizovanú sieť,
kde sa bloky a transakcie automaticky šíria medzi všetkými účastníkmi.
Funguje v LAN aj cez VPN (napr. Tailscale, WireGuard).

## Funkcie

- **Blockchain** — reťazec blokov prepojených SHA-256 hashmi
- **Proof of Work** — ťaženie blokov (difficulty 2, rýchle pre demo)
- **Ed25519 peňaženka** — generovanie kľúčov, podpisovanie transakcií
- **Transakcie s poplatkami** — odosielanie coinov s transakčným fee pre ťažiara
- **P2P sieť** — TCP komunikácia, broadcast blokov a transakcií
- **Synchronizácia mempoolu** — nový peer automaticky dostane čakajúce transakcie
- **Konsenzus** — longest valid chain wins
- **Animované ťaženie** — vizuálne zobrazenie procesu hľadania nonce

## Závislosti

- **GCC** (alebo iný C11 kompilátor)
- **OpenSSL** (libssl-dev / openssl-devel)
- **pthreads** (štandardne súčasť glibc)

### Inštalácia závislostí

```bash
# Fedora / RHEL
sudo dnf install openssl-devel gcc make

# Ubuntu / Debian
sudo apt install libssl-dev gcc make

# Arch
sudo pacman -S openssl gcc make
```

## Kompilácia

```bash
make
```

## Spustenie

Po kompilácii sa binárka nachádza v `build/bin/`:

```bash
# Spustí node na predvolenom porte 9333
./build/bin/minicoin

# Spustí node na vlastnom porte
./build/bin/minicoin 9334

# Alebo cez make:
make run      # port 9333
make run2     # port 9334
```

## Použitie

Po spustení sa zobrazí interaktívne menu:

```
╔═════════════════════════════════════════════════════════════════════════════╗
║  MiniCoin Node [localhost:9333]                                             ║
║  Chain: 1 blokov | Peers: 0 | Zostatok: 0 coin                              ║
║  Adresa: a85fe50b275b1106bb0fc983dc9162abd99d0f639078efd41a6877bc5254dcbf   ║
╠═════════════════════════════════════════════════════════════════════════════╣
║  [1] Ťažiť blok                                                             ║
║  [2] Poslať transakciu                                                      ║
║  [3] Zobraziť blockchain                                                    ║
║  [4] Zobraziť peňaženku                                                     ║
║  [5] Pripojiť peer                                                          ║
║  [6] Stav siete                                                             ║
║  [7] Mempool                                                                ║
║  [q] Ukončiť                                                                ║
╚═════════════════════════════════════════════════════════════════════════════╝
```

### Základný postup

1. **Pripoj peer** `[5]` — najprv sa pripoj k ostatným nodes
2. **Vyťaž blok** `[1]` — získaš 50 coinov (odmena za ťažbu)
3. **Pošli transakciu** `[2]` — zadaj adresu príjemcu, sumu a poplatok
4. **Vyťaž blok** `[1]` — transakcia sa potvrdí a zahrnie do bloku

### Pripojenie nodes

#### Na jednom počítači (dva terminály)

```bash
# Terminál 1
./minicoin 9333

# Terminál 2
./minicoin 9334
# V menu: [5] → localhost → 9333
```

#### Na dvoch počítačoch v LAN

```bash
# PC-A (192.168.1.100)
./minicoin 9333

# PC-B (192.168.1.110)
./minicoin 9333
# V menu: [5] → 192.168.1.100 → 9333
```

#### Cez VPN (Tailscale, WireGuard)

```bash
# PC-A (Tailscale IP: 100.64.0.1)
./minicoin 9333

# PC-C (vzdialený, Tailscale IP: 100.64.0.2)
./minicoin 9333
# V menu: [5] → 100.64.0.1 → 9333
```

Pripojenie cez VPN funguje rovnako ako v LAN, zadáš VPN IP adresu peera.
Nie je potrebný žiadny tunel ani špeciálna konfigurácia.

### Scenár s 3 nodes (LAN + VPN)

```
PC-A (LAN: 192.168.1.100) ─── LAN ─── PC-B (LAN: 192.168.1.110)
         │
     Tailscale
         │
PC-C (Tailscale: 100.64.0.2)
```

1. **A** spustí minicoin, pripojí sa na **C** (cez Tailscale IP)
2. **A** vyťaží blok → 50 coin, blok sa broadcastuje na **C**
3. **A** pošle 5 coin na **C** (fee: 2) → TX v mempoole A aj C
4. **B** sa pripojí na **C** → dostane blockchain + **čakajúce TX z mempoolu**
5. **B** vyťaží blok → zahrnie TX → dostane 50 odmena + 2 fee = **52 coin**
6. **A**: 50 - 5 - 2 = **43 coin**, **C**: **5 coin**, **B**: **52 coin**

### Príklad transakcie

```
PC-A: Vyťaží 2 bloky → zostatok 100 coin
PC-A: Pošle 10 coin na adresu PC-B s poplatkom 1 coin
PC-A: Vyťaží blok → transakcia potvrdená
PC-A: Zostatok = 100 - 10 - 1 + 50 = 139 coin (dostal aj odmenu za ťažbu)
PC-B: Zostatok = 10 coin
```

Ak by blok vyťažil PC-B:

```
PC-A: Zostatok = 100 - 10 - 1 = 89 coin
PC-B: Zostatok = 10 + 50 + 1 = 61 coin (dostal coin + odmenu + fee)
```

## Dôležité princípy

### Transakcie sa nepotvrdia samy

Odoslaná transakcia je len v **mempoole** (čakáreň). Kým niekto nevyťaží blok,
transakcia nie je potvrdená a zostatky sa nezmenia. Preto sú poplatky dôležité —
motivujú ťažiarov potvrdzovať transakcie.

### Mempool sa synchronizuje

Keď sa nový node pripojí, dostane nielen blockchain, ale aj **všetky čakajúce
transakcie z mempoolu** peera. Vďaka tomu môže hociktorý node vyťažiť blok
a zahrnúť čakajúce transakcie.

### Lokálne ťaženie pred pripojením do siete

Technicky môžeš ťažiť aj bez pripojenia k sieti, ťaženie je čisto lokálna
operácia (hľadanie nonce). Problém nastane keď sa pripojíš:

1. **Tvoj chain je kratší ako sieťový** (najčastejší prípad) — sieť ťaží
   viacerými nodes súčasne, takže je takmer vždy rýchlejšia. Po pripojení
   sa tvoj lokálny chain **nahradí dlhším chainom zo siete**. Všetky tvoje
   lokálne vyťažené bloky, coiny a transakcie **zmiznú**.

2. **Tvoj chain je dlhší** (veľmi nepravdepodobné) sieť by prevzala
   tvoj chain. V praxi sa to nestane, lebo jeden počítač neprekoná
   výpočtový výkon celej siete.

**Preto: vždy sa najprv pripoj k sieti, potom ťaž.** Inak ťažíš zbytočne.

V reálnom Bitcoine je to rovnaký princíp, preto existujú mining pooly,
kde ťažiari spájajú výkon. Sólo ťaženie na jednom počítači je dnes
prakticky bezvýznamné.

### Longest chain wins

Ak dva nodes vyťažia blok súčasne, vznikne fork. Vyhráva **najdlhší platný chain**.
Kratší sa zahodí, transakcie z neho sa vrátia do mempoolu (v reálnom Bitcoine;
v tejto demo verzii sa stratia).

## Čo MiniCoin nemá (a reálny Bitcoin áno)

### Merkle Tree

V Bitcoine sa transakcie v bloku hashujú do stromovej štruktúry:

```
          Root Hash
         /        \
    Hash AB        Hash CD
    /    \         /    \
 Hash A  Hash B  Hash C  Hash D
   |       |       |       |
  TX A    TX B    TX C    TX D
```

Výhoda: na overenie jednej transakcie netreba stiahnuť celý blok, stačí
niekoľko hashov (Merkle proof). Dôležité pre mobilné peňaženky (SPV klienti).

MiniCoin jednoducho zreťazí všetky TX hashe.

### Bitcoin Script

V Bitcoine transakcia obsahuje malý program, ktorý definuje podmienky utratenia:

- **Multisig** — na utratenie treba 2 z 3 podpisov (napr. firemný účet)
- **Timelock** — peniaze sa dajú utratiť až po určitom čase
- **Hash lock** — základ pre Lightning Network

MiniCoin má len jednoduché "sender → receiver" s jedným Ed25519 podpisom.

### Ďalšie rozdiely

| Vlastnosť | MiniCoin | Bitcoin |
|---|---|---|
| Difficulty | Fixné 2 nuly | Dynamické, ~19+ núl |
| Odmena za blok | Vždy 50 | Halving každé ~4 roky |
| Kryptografia | Ed25519 | ECDSA secp256k1 |
| Adresa | Raw public key | Base58Check / Bech32 |
| Script | Nie | Bitcoin Script |
| Merkle tree | Nie | Áno |
| UTXO | Zjednodušený | Plný UTXO set |
| Peer discovery | Manuálne | DNS seeds, addr messages |
| Block time | Okamžite | ~10 minút |
| Mempool | Jednoduché pole | Prioritná fronta podľa fee |

## Štruktúra projektu

```
├── Makefile                 # Build systém
├── README.md                # Tento súbor
├── LICENSE                  # MIT licencia
├── build/                   # Build výstup (generované)
│   ├── bin/
│   │   └── minicoin         # Spustiteľná aplikácia
│   └── obj/                 # Objektové súbory (.o)
├── include/                 # Hlavičkové súbory
│   ├── block.h              # Blok, hashing, mining
│   ├── chain.h              # Blockchain, validácia, konsenzus
│   ├── net.h                # TCP sieť, peer management
│   ├── protocol.h           # JSON serializácia správ
│   ├── tx.h                 # Transakcie, UTXO, poplatky
│   └── wallet.h             # Ed25519 kľúče, podpisovanie
├── src/                     # Zdrojové súbory
│   ├── block.c
│   ├── chain.c
│   ├── main.c               # CLI menu, hlavná slučka, animácie
│   ├── net.c
│   ├── protocol.c
│   ├── tx.c
│   └── wallet.c
└── docs/                    # Dokumentácia
    ├── architecture.md      # Architektúra systému
    ├── changelog.md         # História zmien
    └── diagrams.md          # Mermaid diagramy
```

## Dokumentácia

- [Architektúra](docs/architecture.md) — podrobný popis vrstiev a modulov
- [Diagramy](docs/diagrams.md) — Mermaid diagramy procesov a dátových tokov
- [Changelog](docs/changelog.md) — história zmien

## Author 
krse
## Licencia

MIT License — viď [LICENSE](LICENSE).
