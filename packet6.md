# Packet 6/10 – Herní Logika Pexesa

**Datum:** 12. 11. 2025
**Úkol:** Implementace herní logiky Pexesa (Memory card game)

## Přehled

Packet 6 přidává kompletní herní logiku pro Pexeso:
- Vytváření a inicializace hry s náhodným rozmístěním karet
- Obsluha tahů hráčů (otáčení karet)
- Detekce shody/neshody
- Střídání hráčů a počítání bodů
- Detekce konce hry a určení vítěze

## Nové Soubory

### 1. `game.h` (142 řádků)
Header soubor s herními strukturami a funkcemi.

**Klíčové struktury:**
```c
typedef struct {
    int value;           // Hodnota karty (1-18, páry mají stejnou hodnotu)
    card_state_t state;  // CARD_HIDDEN, CARD_REVEALED, CARD_MATCHED
} card_t;

typedef struct game_s {
    int board_size;              // Velikost hrací plochy (4, 5, nebo 6)
    int total_cards;             // board_size * board_size
    int total_pairs;             // total_cards / 2
    card_t *cards;               // Pole karet

    client_t *players[MAX_PLAYERS_PER_ROOM];
    int player_count;
    int player_scores[MAX_PLAYERS_PER_ROOM];
    int player_ready[MAX_PLAYERS_PER_ROOM];

    int current_player_index;    // Index hráče, který je na tahu
    int first_card_index;        // První otočená karta (-1 pokud žádná)
    int second_card_index;       // Druhá otočená karta (-1 pokud žádná)
    int flips_this_turn;         // Počet otočených karet tento tah (0, 1, nebo 2)

    int matched_pairs;           // Počet nalezených párů
    game_state_t state;         // GAME_STATE_WAITING, PLAYING, FINISHED
} game_t;
```

**Hlavní funkce:**
- `game_create()` - vytvoření hry s náhodným rozmístěním
- `game_flip_card()` - otočení karty hráčem
- `game_check_match()` - kontrola shody/neshody
- `game_player_ready()` - označení hráče jako připraveného
- `game_start()` - zahájení hry
- `game_get_winners()` - zjištění vítězů
- `game_destroy()` - ukončení a uvolnění hry

### 2. `game.c` (449 řádků)
Implementace herní logiky.

**Klíčová funkčnost:**

#### Inicializace Hry
```c
game_t* game_create(int board_size, client_t **players, int player_count) {
    // Alokace struktury
    // Vytvoření pole karet
    // Naplnění páry (1,1,2,2,3,3,...)
    // Fisher-Yates shuffle algoritmus
    // Náhodné rozmístění karet
}
```

#### Tah Hráče
```c
int game_flip_card(game_t *game, client_t *client, int card_index) {
    // Kontrola, zda je hra ve stavu PLAYING
    // Kontrola, zda je hráč na tahu
    // Kontrola, zda už neotočil 2 karty
    // Validace indexu karty (0 až total_cards-1)
    // Kontrola, zda karta není už odhalená/spárovaná
    // Otočení karty (CARD_HIDDEN → CARD_REVEALED)
    // Uložení indexu (first_card_index nebo second_card_index)
}
```

#### Kontrola Shody
```c
int game_check_match(game_t *game) {
    if (flips_this_turn != 2) return;

    if (first_value == second_value) {
        // MATCH!
        // Nastavit karty na CARD_MATCHED
        // Přičíst bod aktuálnímu hráči
        // Inkrementovat matched_pairs
        // Stejný hráč pokračuje (current_player_index se nemění)
        // Resetovat first/second_card_index a flips_this_turn
        // Zkontrolovat, zda hra skončila (matched_pairs == total_pairs)
        return 1;
    } else {
        // MISMATCH
        // Vrátit karty na CARD_HIDDEN
        // Přejít na dalšího hráče (current_player_index++)
        // Resetovat indexy
        return 0;
    }
}
```

## Upravené Soubory

### 1. `protocol.h`
Přidány nové příkazy:
```c
// Client → Server
#define CMD_READY "READY"          // Hráč je připraven
#define CMD_START_GAME "START_GAME"  // Vlastník místnosti startuje hru
#define CMD_FLIP "FLIP"            // Otočení karty

// Server → Client
#define CMD_PLAYER_READY "PLAYER_READY"  // Hráč oznámil ready
#define CMD_GAME_START "GAME_START"      // Hra začíná
#define CMD_CARD_REVEAL "CARD_REVEAL"    // Karta byla otočena
#define CMD_MATCH "MATCH"                // Párová shoda
#define CMD_MISMATCH "MISMATCH"          // Neshoda
#define CMD_YOUR_TURN "YOUR_TURN"        // Tvůj tah
#define CMD_GAME_END "GAME_END"          // Konec hry
```

### 2. `room.h`
Přidán ukazatel na hru:
```c
typedef struct room_s {
    // ... existující pole ...
    struct game_s *game;  // Ukazatel na herní instanci (NULL pokud žádná hra)
} room_t;
```

### 3. `room.c`
Inicializace game pointeru:
```c
room->game = NULL;  // Při vytváření místnosti
```

### 4. `client_handler.c`
Přidány 3 nové handlery (203 řádků kódu):

#### `handle_start_game()` (47 řádků)
- Kontrola, zda je klient v místnosti
- Kontrola, zda je vlastník místnosti
- Kontrola, zda už hra neexistuje
- Kontrola minimálního počtu hráčů (2)
- Vytvoření hry: `room->game = game_create(board_size, ...)`
- Broadcast `GAME_CREATED` všem hráčům
- Hráči musí poslat `READY`

#### `handle_ready()` (53 řádků)
- Kontrola, zda existuje hra
- Označení hráče jako ready: `game_player_ready()`
- Broadcast `PLAYER_READY` ostatním
- Kontrola, zda jsou všichni ready: `game_all_players_ready()`
- Pokud ano:
  - Zahájení hry: `game_start()`
  - Nastavení stavu místnosti: `ROOM_STATE_PLAYING`
  - Broadcast `GAME_START` se seznamem hráčů
  - Odeslání `YOUR_TURN` prvnímu hráči

#### `handle_flip()` (96 řádků)
- Kontrola existence hry
- Parsování indexu karty z parametrů
- Pokus o otočení: `game_flip_card()`
- Broadcast `CARD_REVEAL <index> <value> <nickname>` všem
- Pokud to byla druhá karta:
  - Kontrola shody: `game_check_match()`
  - **Pokud MATCH:**
    - Broadcast `MATCH <nickname> <score>`
    - Kontrola konce hry: `game_is_finished()`
    - Pokud konec:
      - `game_get_winners()` získání vítězů
      - Broadcast `GAME_END <winner1> <score1> <winner2> <score2> ...`
      - `game_destroy()` úklid
      - `room->game = NULL`
      - `room->state = ROOM_STATE_WAITING`
    - Pokud hra pokračuje:
      - Odeslání `YOUR_TURN` stejnému hráči (hraje znovu)
  - **Pokud MISMATCH:**
    - Broadcast `MISMATCH`
    - Odeslání `YOUR_TURN` dalšímu hráči

### 5. `Makefile`
Přidán game.c do kompilace:
```makefile
SOURCES = main.c server.c client_handler.c logger.c room.c game.c
game.o: game.c game.h client_handler.h logger.h protocol.h
client_handler.o: ... game.h ...
```

## Protokol

### Herní Flow

#### 1. Vytvoření Hry
```
Client (vlastník): START_GAME
Server → All:      GAME_CREATED 4 Send READY when you are prepared to play
```

#### 2. Připravenost Hráčů
```
Client 1: READY
Server → All: PLAYER_READY Alice

Client 2: READY
Server → All: PLAYER_READY Bob

... (všichni ready) ...

Server → All: GAME_START 4 Alice Bob
Server → Alice: YOUR_TURN
```

#### 3. Tah Hráče (První Karta)
```
Alice: FLIP 5
Server → All: CARD_REVEAL 5 3 Alice
(čeká na druhou kartu)
```

#### 4. Tah Hráče (Druhá Karta - Shoda)
```
Alice: FLIP 12
Server → All: CARD_REVEAL 12 3 Alice
Server → All: MATCH Alice 1
Server → Alice: YOUR_TURN
(Alice hraje znovu, protože našla pár)
```

#### 5. Tah Hráče (Druhá Karta - Neshoda)
```
Alice: FLIP 7
Server → All: CARD_REVEAL 7 2 Alice
Server → All: MISMATCH
Server → Bob: YOUR_TURN
(karty 5 a 7 se otočí zpět, tah přechází na Boba)
```

#### 6. Konec Hry
```
(poslední pár nalezen)
Server → All: MATCH Alice 8
Server → All: GAME_END Alice 8 Bob 0
(hra končí, Alice vyhrála s 8 páry, Bob 0)
```

## Validace a Error Handling

### START_GAME Chyby:
- `ERROR NOT_IN_ROOM` - hráč není v místnosti
- `ERROR NOT_ROOM_OWNER` - pouze vlastník může startovat
- `ERROR Game already started` - hra už běží
- `ERROR NEED_MORE_PLAYERS` - méně než 2 hráči

### READY Chyby:
- `ERROR NOT_IN_ROOM` - hráč není v místnosti
- `ERROR GAME_NOT_STARTED` - hra nebyla vytvořena
- `ERROR Already ready or game already started`

### FLIP Chyby:
- `ERROR NOT_IN_ROOM` - hráč není v místnosti
- `ERROR GAME_NOT_STARTED` - hra neběží
- `ERROR INVALID_PARAMS` - chybí index karty
- `ERROR INVALID_CARD` - platí když:
  - Není hráčův tah
  - Už otočil 2 karty tento tah
  - Index mimo rozsah (< 0 nebo >= total_cards)
  - Karta je už REVEALED nebo MATCHED

## Algoritmy

### Fisher-Yates Shuffle
```c
static void shuffle_array(int *array, int size) {
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}
```

### Generování Karet
```c
// Pro 4x4 = 16 karet = 8 párů
int values[16] = {1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8};
shuffle_array(values, 16);
// Nyní values obsahuje náhodně rozmístěné páry
```

## Testovací Scénář

### Základní Hra (2 Hráče)

**Terminál 1 - Server:**
```bash
./server 127.0.0.1 10000 10 50
```

**Terminál 2 - Alice:**
```bash
nc 127.0.0.1 10000
HELLO Alice
CREATE_ROOM GameRoom 2
```
→ `ROOM_CREATED 1 GameRoom`

**Terminál 3 - Bob:**
```bash
nc 127.0.0.1 10000
HELLO Bob
JOIN_ROOM 1
```
→ `ROOM_JOINED 1 GameRoom`
→ Alice dostane: `PLAYER_JOINED Bob`

**Alice startuje hru:**
```
START_GAME
```
→ All: `GAME_CREATED 4 Send READY when you are prepared to play`

**Alice ready:**
```
READY
```
→ All: `PLAYER_READY Alice`
→ Alice: `READY_OK`

**Bob ready:**
```
READY
```
→ All: `PLAYER_READY Bob`
→ All: `GAME_START 4 Alice Bob`
→ Alice: `YOUR_TURN`

**Alice otočí první kartu:**
```
FLIP 0
```
→ All: `CARD_REVEAL 0 3 Alice`

**Alice otočí druhou kartu (shoda):**
```
FLIP 5
```
→ All: `CARD_REVEAL 5 3 Alice`
→ All: `MATCH Alice 1`
→ Alice: `YOUR_TURN`

**Alice otočí (neshoda):**
```
FLIP 1
FLIP 2
```
→ All: `CARD_REVEAL 1 7 Alice`
→ All: `CARD_REVEAL 2 2 Alice`
→ All: `MISMATCH`
→ Bob: `YOUR_TURN`

**Bob hraje...**
(pokračuje, dokud nejsou nalezeny všechny páry)

**Konec hry:**
→ All: `GAME_END Alice 5 Bob 3`

### Test Error Handling

**Otočení karty, když není tah:**
```
Bob: FLIP 0
```
→ `ERROR INVALID_CARD Cannot flip that card`

**Otočení už otočené karty:**
```
Alice: FLIP 0
Alice: FLIP 0
```
→ druhé: `ERROR INVALID_CARD Cannot flip that card`

**START bez hráčů:**
```
Alice (sama): START_GAME
```
→ `ERROR NEED_MORE_PLAYERS Need at least 2 players`

## Statistiky

- **Nové soubory:** 2 (game.h, game.c)
- **Upravené soubory:** 5 (protocol.h, room.h, room.c, client_handler.c, Makefile)
- **Nové řádky kódu:** ~650
- **Nové funkce:** 11 (v game.c)
- **Nové handlery:** 3 (handle_start_game, handle_ready, handle_flip)
- **Nové protokolové příkazy:** 8

## Kompilace

```bash
cd server_src
make clean
make
```

Očekávaný výstup:
```
gcc -Wall -Wextra -pthread -g -std=c99 -c main.c -o main.o
gcc -Wall -Wextra -pthread -g -std=c99 -c server.c -o server.o
gcc -Wall -Wextra -pthread -g -std=c99 -c client_handler.c -o client_handler.o
gcc -Wall -Wextra -pthread -g -std=c99 -c logger.c -o logger.o
gcc -Wall -Wextra -pthread -g -std=c99 -c room.c -o room.o
gcc -Wall -Wextra -pthread -g -std=c99 -c game.c -o game.o
gcc main.o server.o client_handler.o logger.o room.o game.o -o server -pthread
Build successful: server
```

## Poznámky k Implementaci

1. **Náhodnost:** Používá `srand(time(NULL))` pro seed, každá hra má jiné rozmístění
2. **Velikost plochy:** Momentálně fixní 4x4 (16 karet, 8 párů), lze snadno změnit
3. **Memory leaks:** `game_destroy()` korektně uvolňuje všechnu paměť
4. **Thread-safety:** Game není sdílená mezi vlákny (každá místnost má svojí hru)
5. **Vítězství:** Podporuje remízy (více vítězů se stejným skóre)

## Další Kroky

- **Packet 7:** PING/PONG keepalive a timeout detection
- **Packet 8:** Reconnection mechanism (RECONNECT command, GAME_STATE response)
- **Packet 9:** Konfigurovatelná velikost plochy (4x4, 5x5, 6x6)
- **Packet 10:** Finální testování a dokumentace
