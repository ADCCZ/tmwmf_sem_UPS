# SPECIFIKACE KOMUNIKAČNÍHO PROTOKOLU PRO HRU PEXESO

---

## 1. ZÁKLADNÍ CHARAKTERISTIKA

| Parametr | Hodnota |
|-----------|----------|
| **Typ protokolu** | Textový, aplikační vrstva |
| **Transportní protokol** | TCP |
| **Kódování** | ASCII (bez diakritiky) |
| **Ukončení zprávy** | znak nového řádku `\n` |
| **Formát zprávy** | `COMMAND [PARAM1] [PARAM2] ...\n` |
| **Oddělování parametrů** | Jedna mezera (ASCII 0x20) |
| **Reakce na požadavky** | Na každý požadavek přijde odpověď (`OK`, `ERROR`, nebo specifická odpověď) |

---

## 2. PRAVIDLA HRY PEXESO (IMPLEMENTOVANÁ VARIANTA)

- Hra pro 2 až 4 hráče  
- Herní deska obsahuje N párů karet (typicky 8, 12, 16 nebo 18 párů = 16, 24, 32, 36 karet)  
- Každá karta má hodnotu (číslo symbolu), každá hodnota se vyskytuje přesně 2×  
- Hráči se střídají v tazích  
- V každém tahu hráč otočí 2 karty:
  - Pokud se hodnoty shodují → hráč získává 1 bod a hraje znovu  
  - Pokud se neshodují → karty se vrátí lícem dolů a přichází na řadu další hráč  
- Hra končí, když jsou všechny páry nalezeny  
- Vyhrává hráč s nejvyšším skóre  

---

## 3. DATOVÉ TYPY A OMEZENÍ

| Typ | Popis | Formát | Omezení |
|------|--------|----------|----------|
| `nickname` | Přezdívka hráče | String bez mezer | 1–16 znaků, `a-zA-Z0-9_-` |
| `room_id` | Identifikátor místnosti | Integer | 1–9999 |
| `room_name` | Název místnosti | String bez mezer | 1–20 znaků, `a-zA-Z0-9_-` |
| `client_id` | ID klienta přidělené serverem | Integer | 1–9999 |
| `card_id` | Index karty na desce | Integer | 0 až (board_size-1) |
| `card_value` | Hodnota karty (symbol) | Integer | 0 až (board_size/2 - 1) |
| `board_size` | Počet karet celkem | Integer | 16, 24, 32, 36 (sudý) |
| `max_players` | Max. počet hráčů v místnosti | Integer | 2–4 |
| `score` | Skóre hráče | Integer | ≥ 0 |
| `error_code` | Kód chyby | Integer | 100–599 |
| `message` | Textová zpráva | String (může obsahovat `_`) | Max. 100 znaků |
| `status` | Stav místnosti | String | `WAITING`, `PLAYING` |
| `disconnect_type` | Typ výpadku | String | `SHORT`, `LONG` |

---

## 4. ZPRÁVY OD KLIENTA K SERVERU

### 4.1 HELLO
**Účel:** První zpráva po navázání TCP spojení, identifikace hráče  
**Formát:** `HELLO <nickname>`  
**Příklad:** `HELLO Petr123`  
**Odpověď:** `WELCOME` nebo `ERROR`

---

### 4.2 LIST_ROOMS
**Účel:** Výpis všech existujících místností  
**Formát:** `LIST_ROOMS`  
**Odpověď:** `ROOM_LIST`

---

### 4.3 CREATE_ROOM
**Účel:** Vytvoření nové herní místnosti  
**Formát:** `CREATE_ROOM <room_name> <max_players> <board_size>`  
**Příklad:** `CREATE_ROOM MyRoom 3 24`  
**Odpověď:** `ROOM_CREATED` nebo `ERROR`

---

### 4.4 JOIN_ROOM
**Účel:** Vstup do existující místnosti  
**Formát:** `JOIN_ROOM <room_id>`  
**Příklad:** `JOIN_ROOM 42`  
**Odpověď:** `ROOM_JOINED` nebo `ERROR`

---

### 4.5 LEAVE_ROOM
**Účel:** Opuštění místnosti (návrat do lobby)  
**Formát:** `LEAVE_ROOM`  
**Odpověď:** `ROOM_LEFT` nebo `ERROR`

---

### 4.6 START_GAME
**Účel:** Zahájení hry (pouze tvůrce místnosti)  
**Formát:** `START_GAME`  
**Odpověď:** `GAME_START` nebo `ERROR`

---

### 4.7 FLIP
**Účel:** Otočení karty (herní tah)  
**Formát:** `FLIP <card_id>`  
**Příklad:** `FLIP 15`  
**Odpověď:** `CARD_REVEAL`, následně `MATCH` nebo `MISMATCH`

---

### 4.8 PONG
**Účel:** Odpověď na `PING` od serveru  
**Formát:** `PONG`  

---

### 4.9 DISCONNECT
**Účel:** Korektní odpojení klienta  
**Formát:** `DISCONNECT`  

---

### 4.10 RECONNECT
**Účel:** Znovupřipojení po krátkodobém výpadku  
**Formát:** `RECONNECT <nickname>`  
**Příklad:** `RECONNECT Petr123`  
**Odpověď:** `WELCOME` + `GAME_STATE` nebo `ERROR`

---

## 5. ZPRÁVY OD SERVERU KE KLIENTOVI

*(Zde uvádíme výčet všech zpráv včetně příkladů. Např. `WELCOME`, `ERROR`, `ROOM_LIST`, `MATCH`, `PING`, `PLAYER_DISCONNECTED` atd. — viz originální text, formát zachován.)*

Každá zpráva má jednotný formát `COMMAND [PARAM1] [PARAM2] ...\n`.

---

## 6. STAVOVÝ DIAGRAM

### 6.1 Stavy klienta
```

DISCONNECTED -> CONNECTED -> AUTHENTICATED -> IN_LOBBY
-> IN_ROOM -> IN_GAME -> (DISCONNECTED/RECONNECT)

```

### 6.2 Stavy serveru
```

NOT_CONNECTED -> CONNECTED -> AUTHENTICATED -> IN_LOBBY
-> IN_ROOM -> IN_GAME -> SHORT_DISCONNECT -> LONG_DISCONNECT

```

### 6.3 Typický průběh hry
*(Sekvenční diagram komunikace mezi klientem a serverem — viz původní popis.)*

---

## 7. ŘEŠENÍ VÝPADKŮ

### 7.1 Krátkodobý výpadek (`SHORT_DISCONNECT`)
- Server posílá `PING` každých 15 s  
- Timeout `5 s` → pokud není `PONG`, nastává krátkodobý výpadek  
- Hra se pozastaví, ostatní hráči dostanou `PLAYER_DISCONNECTED <nick> SHORT`  
- Klient se automaticky pokusí o `RECONNECT`

### 7.2 Dlouhodobý výpadek (`LONG_DISCONNECT`)
- Po 60 s bez `PONG` → dlouhodobý výpadek  
- Server odešle `PLAYER_DISCONNECTED <nick> LONG`, odstraní hráče  
- Pokud zbývá méně než 2 hráči → `GAME_END`

### 7.3 Korektní odpojení
- Klient pošle `DISCONNECT`  
- Server okamžitě uzavře spojení a informuje ostatní (`PLAYER_LEFT`)

---

## 8. NEVALIDNÍ ZPRÁVY

### Typy:
1. Neznámý příkaz  
2. Špatný formát  
3. Neplatné hodnoty  
4. Příkaz ve špatném stavu  
5. Porušení pravidel  

### Reakce:
- Server pošle `ERROR <code> <message>`  
- Po 3 chybách → odpojení klienta  
- Vše logováno

---

## 9. ROZŠIŘITELNOST PROTOKOLU

- Podpora budoucích funkcí:
  - `CHAT`, `STATS`, `LEADERBOARD`, `SPECTATE`, `AUTH`
- Kompatibilní rozšíření formátů příkazů
- Udržení zpětné kompatibility  

---

## 10. LOGOVÁNÍ

### Server loguje:
- Připojení/odpojení klientů (IP, čas, přezdívka)  
- Vytvoření a zrušení místností  
- Start/konec her  
- Výpadky (`SHORT/LONG`)  
- Nevalidní zprávy  
- Interní chyby  

### Klient loguje:
- Připojení, odpojení  
- Přechody mezi stavy  
- Průběh hry, tahy, výsledky  
- Síťové chyby, reconnect  

---

## 11. PŘÍKLAD KOMPLETNÍ KOMUNIKACE

```

C->S: HELLO Alice
S->C: WELCOME 1

C->S: LIST_ROOMS
S->C: ROOM_LIST 0

C->S: CREATE_ROOM Game1 2 16
S->C: ROOM_CREATED 1

C2->S: HELLO Bob
S->C2: WELCOME 2

C2->S: LIST_ROOMS
S->C2: ROOM_LIST 1 1 Game1 1 2 WAITING

C2->S: JOIN_ROOM 1
S->C2: ROOM_JOINED 1 2 Alice Bob
S->C: PLAYER_JOINED Bob

C->S: START_GAME
S->C: GAME_START 16 2 Alice Bob
S->C2: GAME_START 16 2 Alice Bob

S->C: YOUR_TURN
S->C2: TURN Alice
...
S->C: GAME_END Bob 2 Alice 3 Bob 5
S->C2: GAME_END Bob 2 Alice 3 Bob 5

```

---

## 12. IMPLEMENTAČNÍ POZNÁMKY

- Ukončení zpráv: `\n`  
- Buffering příchozích zpráv nutný  
- Timeouty:
  - `PING` interval: 15 s  
  - `PONG` timeout: 5 s  
  - `SHORT_DISCONNECT` timeout: 60 s  
- Synchronizace vláken (mutexy)  
- Uvolňování neaktivních klientů/místností  
- Validace všech vstupů  

---

## 13. SHRNUTÍ KLÍČOVÝCH VLASTNOSTÍ

✅ Textový, čitelný, snadno laditelný  
✅ Jasná struktura zpráv  
✅ Každý požadavek má odpověď  
✅ Podpora lobby a více místností  
✅ Řešení krátkodobých i dlouhodobých výpadků  
✅ Keepalive (PING/PONG)  
✅ Robustní error-handling  
✅ Broadcast zprávy  
✅ Jasný stavový model  
✅ Rozšiřitelný design  
✅ Bez závislostí na externích knihovnách  

---
