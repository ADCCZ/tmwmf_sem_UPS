# SPECIFIKACE KOMUNIKAČNÍHO PROTOKOLU PRO HRU PEXESO

---

## KOMPILACE

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
**Formát:** `RECONNECT <client_id>`
**Příklad:** `RECONNECT 42`
**Odpověď:** `WELCOME <client_id> Reconnected successfully` + `GAME_STATE` (pokud ve hře) nebo `ERROR`

---

## 5. ZPRÁVY OD SERVERU KE KLIENTOVI

### 5.1 WELCOME
**Účel:** Potvrzení úspěšné autentizace nebo reconnectu
**Formát:** `WELCOME <client_id> [message]`
**Příklad:** `WELCOME 42` nebo `WELCOME 42 Reconnected successfully`

---

### 5.2 ERROR
**Účel:** Oznámení chyby
**Formát:** `ERROR <error_code> [message]`
**Příklady:**
- `ERROR INVALID_COMMAND Unknown command`
- `ERROR ROOM_FULL Room is full`
- `ERROR NOT_YOUR_TURN Wait for your turn`

---

### 5.3 ROOM_LIST
**Účel:** Seznam všech místností v lobby
**Formát:** `ROOM_LIST <count> [<id> <name> <players> <max> <status>] ...`
**Příklad:** `ROOM_LIST 2 1 Game1 2 4 WAITING 2 Game2 1 2 PLAYING`

---

### 5.4 ROOM_CREATED
**Účel:** Potvrzení vytvoření místnosti
**Formát:** `ROOM_CREATED <room_id> <room_name>`
**Příklad:** `ROOM_CREATED 5 MyRoom`

---

### 5.5 ROOM_JOINED
**Účel:** Potvrzení vstupu do místnosti + seznam hráčů
**Formát:** `ROOM_JOINED <room_id> <room_name> <player1> <player2> ...`
**Příklad:** `ROOM_JOINED 5 MyRoom Alice Bob`

---

### 5.6 PLAYER_JOINED
**Účel:** Oznámení, že do místnosti vstoupil nový hráč (broadcast)
**Formát:** `PLAYER_JOINED <nickname>`
**Příklad:** `PLAYER_JOINED Charlie`

---

### 5.7 PLAYER_LEFT
**Účel:** Oznámení, že hráč opustil místnost (broadcast)
**Formát:** `PLAYER_LEFT <nickname>`
**Příklad:** `PLAYER_LEFT Bob`

---

### 5.8 PLAYER_READY
**Účel:** Oznámení, že hráč je připraven hrát (broadcast)
**Formát:** `PLAYER_READY <nickname>`
**Příklad:** `PLAYER_READY Alice`

---

### 5.9 GAME_CREATED
**Účel:** Hra byla vytvořena, čeká se na READY od všech
**Formát:** `GAME_CREATED <board_size> <message>`
**Příklad:** `GAME_CREATED 16 Send READY when you are prepared to play`

---

### 5.10 GAME_START
**Účel:** Hra začíná (všichni hráči jsou ready)
**Formát:** `GAME_START <board_size> <player_count> <player1> <player2> ...`
**Příklad:** `GAME_START 16 2 Alice Bob`

---

### 5.11 YOUR_TURN
**Účel:** Je na tahu tento klient
**Formát:** `YOUR_TURN`

---

### 5.12 CARD_REVEAL
**Účel:** Oznámení otočení karty (broadcast)
**Formát:** `CARD_REVEAL <card_id> <card_value> <nickname>`
**Příklad:** `CARD_REVEAL 7 3 Alice`

---

### 5.13 MATCH
**Účel:** Karty se shodují, hráč získává bod (broadcast)
**Formát:** `MATCH <nickname> <new_score>`
**Příklad:** `MATCH Alice 5`

---

### 5.14 MISMATCH
**Účel:** Karty se neshodují, přechod na dalšího hráče (broadcast)
**Formát:** `MISMATCH <next_player>`
**Příklad:** `MISMATCH Bob`

---

### 5.15 GAME_END
**Účel:** Hra skončila, výsledky
**Formát:** `GAME_END <winner1> <score1> <winner2> <score2> ...`
**Příklad:** `GAME_END Alice 8 Bob 6`

---

### 5.16 GAME_END_FORFEIT
**Účel:** Hra skončila vzdáním (hráč se nevrátil po 90s)
**Formát:** `GAME_END_FORFEIT <player1> <score1> <player2> <score2> ...`
**Příklad:** `GAME_END_FORFEIT Alice 5 Bob 3`

---

### 5.17 GAME_STATE
**Účel:** Obnovení stavu hry po reconnectu
**Formát:** `GAME_STATE <board_size> <player_count> <current_player> <player1> <score1> <player2> <score2> ... <revealed_count> [<card_id> <card_value>] ...`
**Příklad:** `GAME_STATE 16 2 Alice Alice 3 Bob 2 4 5 2 7 2`

---

### 5.18 PING
**Účel:** Keepalive kontrola spojení
**Formát:** `PING`
**Očekávaná odpověď:** `PONG`

---

### 5.19 PLAYER_DISCONNECTED
**Účel:** Hráč se odpojil (broadcast)
**Formát:** `PLAYER_DISCONNECTED <nickname> <type> [message]`
**Příklady:**
- `PLAYER_DISCONNECTED Bob SHORT Waiting for reconnect (up to 90 seconds)...`
- `PLAYER_DISCONNECTED Bob LONG Player removed`
- `PLAYER_DISCONNECTED Bob REMOVED Game continues`

---

### 5.20 PLAYER_RECONNECTED
**Účel:** Hráč se úspěšně znovu připojil (broadcast)
**Formát:** `PLAYER_RECONNECTED <nickname>`
**Příklad:** `PLAYER_RECONNECTED Bob`

---

### 5.21 ROOM_OWNER_CHANGED
**Účel:** Změna vlastníka místnosti (broadcast)
**Formát:** `ROOM_OWNER_CHANGED <new_owner_nickname>`
**Příklad:** `ROOM_OWNER_CHANGED Alice`

---

### 5.22 LEFT_ROOM
**Účel:** Potvrzení opuštění místnosti
**Formát:** `LEFT_ROOM`

---

### 5.23 READY_OK
**Účel:** Potvrzení READY
**Formát:** `READY_OK`

---

### 5.24 SERVER_SHUTDOWN
**Účel:** Server se vypíná
**Formát:** `SERVER_SHUTDOWN [message]`
**Příklad:** `SERVER_SHUTDOWN Server is shutting down`

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
- **Server:** Posílá `PING` každých 5 s po přijetí `PONG` (PONG_WAIT_INTERVAL)
- **PONG timeout:** 5 s → pokud není `PONG`, klient je označen jako odpojený
- **Detekce na klientu:** READ_TIMEOUT 15 s → pokud není zpráva od serveru, klient detekuje výpadek
- **Automatický reconnect (klient):**
  - 7 pokusů po 10 sekundách = 70 sekund celkem
  - Klient posílá `RECONNECT <client_id>`
- **Server čeká:** 90 sekund na reconnect
- Hra se pozastaví, ostatní hráči dostanou `PLAYER_DISCONNECTED <nick> SHORT`

### 7.2 Dlouhodobý výpadek (`LONG_DISCONNECT`)
- **Po 90 s bez reconnect** → dlouhodobý výpadek
- Server odešle `PLAYER_DISCONNECTED <nick> LONG`, odstraní hráče
- Pokud zbývá méně než 2 hráči → `GAME_END_FORFEIT`
- Pokud zbývá 2+ hráčů → hra pokračuje bez odpojeného hráče

### 7.3 Korektní odpojení
- Klient zavře spojení nebo aplikaci
- Server detekuje uzavření TCP spojení
- Server informuje ostatní (`PLAYER_LEFT`)

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
# Klient 1 (Alice) se připojí
C->S: HELLO Alice
S->C: WELCOME 1

C->S: LIST_ROOMS
S->C: ROOM_LIST 0

C->S: CREATE_ROOM Game1 2 16
S->C: ROOM_CREATED 1 Game1

# Klient 2 (Bob) se připojí
C2->S: HELLO Bob
S->C2: WELCOME 2

C2->S: LIST_ROOMS
S->C2: ROOM_LIST 1 1 Game1 1 2 WAITING

C2->S: JOIN_ROOM 1
S->C2: ROOM_JOINED 1 Game1 Alice Bob
S->C: PLAYER_JOINED Bob

# Alice (owner) zahajuje hru
C->S: START_GAME
S->C: GAME_START 16 2 Alice Bob
S->C2: GAME_START 16 2 Alice Bob

# Alice je na tahu
S->C: YOUR_TURN

# Alice otočí kartu 5
C->S: FLIP 5
S->C: CARD_REVEAL 5 3 Alice
S->C2: CARD_REVEAL 5 3 Alice

# Alice otočí kartu 12
C->S: FLIP 12
S->C: CARD_REVEAL 12 7 Alice
S->C2: CARD_REVEAL 12 7 Alice

# Neshodují se -> Bob je na tahu
S->C: MISMATCH Bob
S->C2: MISMATCH Bob
S->C2: YOUR_TURN

# Bob otočí kartu 3
C2->S: FLIP 3
S->C: CARD_REVEAL 3 2 Bob
S->C2: CARD_REVEAL 3 2 Bob

# Bob otočí kartu 8
C2->S: FLIP 8
S->C: CARD_REVEAL 8 2 Bob
S->C2: CARD_REVEAL 8 2 Bob

# Shodují se! Bob získává bod a hraje znovu
S->C: MATCH Bob 1
S->C2: MATCH Bob 1
S->C2: YOUR_TURN

# ... hra pokračuje ...

# Konec hry (všechny páry nalezeny)
S->C: GAME_END Alice 3 Bob 5
S->C2: GAME_END Alice 3 Bob 5

# Keepalive během hry
S->C: PING
C->S: PONG

```

---

## 12. IMPLEMENTAČNÍ POZNÁMKY

- Ukončení zpráv: `\n`
- Buffering příchozích zpráv nutný
- **Timeouty (server):**
  - `PONG_WAIT_INTERVAL`: 5 s (čas mezi PONG a dalším PING)
  - `PONG_TIMEOUT`: 5 s (max. čas na odpověď PONG)
  - `RECONNECT_TIMEOUT`: 90 s (čekání na reconnect)
- **Timeouty (klient):**
  - `CONNECTION_TIMEOUT`: 5 s (timeout při navazování spojení)
  - `READ_TIMEOUT`: 15 s (timeout při čtení ze socketu)
  - `RECONNECT_INTERVAL`: 10 s (interval mezi reconnect pokusy)
  - `MAX_RECONNECT_ATTEMPTS`: 7 (celkem 70 sekund)
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
