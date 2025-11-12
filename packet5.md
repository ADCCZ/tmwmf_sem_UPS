# PACKET 5/10 - LOBBY A MÍSTNOSTI NA SERVERU

**Datum:** 2025-11-12
**Cíl:** Implementovat správu místností a lobby na serveru
**Status:** ✅ HOTOVO

---

## ZADÁNÍ PACKETU 5

Rozšířit existující server o:
- Globální seznam připojených hráčů
- Seznam místností (ID, název, max hráčů, stav hry)
- Implementace příkazů: LIST_ROOMS, CREATE_ROOM, JOIN_ROOM, LEAVE_ROOM
- Ošetření limitů místností a max počtu hráčů
- Logování všech operací

---

## IMPLEMENTOVANÉ SOUBORY

### Nové soubory (2):
1. **`room.h`** (2.5 KB) - Definice struktur a funkcí pro místnosti
2. **`room.c`** (8.1 KB) - Implementace správy místností

### Upravené soubory (3):
3. **`client_handler.h`** - Přidáno pole `room` do `client_t`
4. **`client_handler.c`** - Přidáno parsování příkazů a handlery
5. **`server.c`** - Přidána inicializace room systému
6. **`Makefile`** - Přidán room.c do SOURCES

---

## DETAILNÍ POPIS ZMĚN

### 1. room.h - Struktura místností

**Nové datové struktury:**

```c
// Stav místnosti
typedef enum {
    ROOM_STATE_WAITING,    // Čeká na hráče
    ROOM_STATE_PLAYING,    // Hra běží
    ROOM_STATE_FINISHED    // Hra skončila
} room_state_t;

// Místnost
typedef struct room_s {
    int room_id;                              // Unikátní ID
    char name[MAX_ROOM_NAME_LENGTH];          // Název (max 64 znaků)
    client_t *players[MAX_PLAYERS_PER_ROOM];  // Pole hráčů (max 4)
    int player_count;                         // Aktuální počet hráčů
    int max_players;                          // Max počet hráčů (2-4)
    room_state_t state;                       // Stav místnosti
    client_t *owner;                          // Tvůrce místnosti
} room_t;
```

**Klíčové funkce:**

| Funkce | Účel |
|--------|------|
| `room_system_init()` | Inicializace pole místností |
| `room_system_shutdown()` | Cleanup všech místností |
| `room_create()` | Vytvoření nové místnosti |
| `room_get_by_id()` | Nalezení místnosti podle ID |
| `room_add_player()` | Přidání hráče do místnosti |
| `room_remove_player()` | Odebrání hráče z místnosti |
| `room_destroy()` | Zrušení místnosti |
| `room_get_list_message()` | Vytvoření ROOM_LIST zprávy |
| `room_broadcast()` | Broadcast zprávy všem hráčům v místnosti |

---

### 2. room.c - Implementace

**Globální proměnné:**

```c
static room_t **rooms = NULL;           // Pole místností
static int max_rooms = 0;                // Max počet místností
static int next_room_id = 1;            // Čítač ID
static pthread_mutex_t rooms_mutex;     // Mutex pro thread-safety
```

**Thread-safety:**
- Všechny operace s místnostmi jsou chráněné mutexem
- Zabraňuje race conditions při současném přístupu více threadů

**Automatické cleanup:**
- Při opuštění posledního hráče se místnost automaticky zruší
- `room_remove_player()` kontroluje `player_count == 0`

**Format ROOM_LIST zprávy:**
```
ROOM_LIST <count> <id1> <name1> <players1> <max1> <state1> <id2> <name2> ...
```

Příklad:
```
ROOM_LIST 2 1 Lobby1 2 4 WAITING 2 MyRoom 1 3 WAITING
```

---

### 3. client_handler.h - Rozšíření client_t

**Přidáno pole:**

```c
typedef struct {
    // ... existující pole ...
    struct room_s *room;  // Pointer na aktuální místnost (NULL = v lobby)
} client_t;
```

**Forward declaration:**
```c
struct room_s;  // Zamezení circular dependency
```

---

### 4. client_handler.c - Parsování a handlery

**Nová funkce `handle_message()`:**
- Parsuje příkaz a parametry
- Separuje příkaz pomocí mezery
- Volá příslušný handler

**Přidané command handlery:**

#### `handle_hello(const char *params)`
```c
// HELLO <nickname>
// → Nastaví nickname, state = STATE_IN_LOBBY
// ← WELCOME <client_id>
```

**Validace:**
- Kontrola stavu (musí být STATE_CONNECTED)
- Kontrola délky nicku (max 32 znaků)

#### `handle_list_rooms()`
```c
// LIST_ROOMS
// → Vrací seznam všech místností
// ← ROOM_LIST <count> <room_data> ...
```

**Validace:**
- Musí být autentifikovaný (STATE_IN_LOBBY nebo vyšší)

#### `handle_create_room(const char *params)`
```c
// CREATE_ROOM <name> <max_players>
// → Vytvoří místnost, přidá tvůrce jako prvního hráče
// ← ROOM_CREATED <room_id> <room_name>
```

**Validace:**
- Musí být v lobby (ne v jiné místnosti)
- `max_players` musí být 2-4
- Kontrola limitu místností

**Chování:**
- Tvůrce je automaticky přidán do místnosti
- `client->state` = STATE_IN_ROOM
- `client->room` = pointer na místnost

#### `handle_join_room(const char *params)`
```c
// JOIN_ROOM <room_id>
// → Přidá hráče do existující místnosti
// ← ROOM_JOINED <room_id> <room_name>
// ← PLAYER_JOINED <nickname> (broadcast ostatním)
```

**Validace:**
- Musí být v lobby
- Místnost musí existovat
- Místnost nesmí být plná

**Broadcast:**
- Všichni hráči v místnosti dostanou `PLAYER_JOINED <nickname>`

#### `handle_leave_room()`
```c
// LEAVE_ROOM
// → Odebere hráče z místnosti
// ← LEFT_ROOM
// ← PLAYER_LEFT <nickname> (broadcast ostatním)
```

**Validace:**
- Musí být v místnosti

**Automatické cleanup:**
- Pokud odejde poslední hráč → místnost se zruší
- Ostatní hráči dostanou `PLAYER_LEFT <nickname>`

#### `handle_pong()`
```c
// PONG
// → Aktualizuje last_activity
```

---

### 5. Cleanup při odpojení

**V `client_handler_thread()` při ukončení:**

```c
// Cleanup - remove from room if in one
if (client->room != NULL) {
    room_remove_player(room, client);

    // Broadcast to other players
    if (room still exists) {
        broadcast "PLAYER_DISCONNECTED <nickname> LONG"
    }
}
```

**Typy odpojení:**
- **SHORT** - krátký výpadek (< 60s) - bude v budoucnu
- **LONG** - dlouhý výpadek (> 60s) - hráč je odstraněn

---

### 6. server.c - Inicializace room systému

**V `server_init()`:**

```c
// Initialize room system
if (room_system_init(max_rooms) != 0) {
    logger_log(LOG_ERROR, "Failed to initialize room system");
    return -1;
}
```

**V `server_shutdown()`:**

```c
// Shutdown room system
room_system_shutdown();
```

---

## TESTOVÁNÍ

### Test 1: Kompilace

```bash
cd server_src
make clean
make
```

**Očekávaný výstup:**
```
gcc -Wall -Wextra -pthread -g -std=c99 -c main.c -o main.o
gcc -Wall -Wextra -pthread -g -std=c99 -c server.c -o server.o
gcc -Wall -Wextra -pthread -g -std=c99 -c client_handler.c -o client_handler.o
gcc -Wall -Wextra -pthread -g -std=c99 -c logger.c -o logger.o
gcc -Wall -Wextra -pthread -g -std=c99 -c room.c -o room.o
gcc main.o server.o client_handler.o logger.o room.o -o server -pthread
Build successful: server
```

### Test 2: Spuštění serveru

```bash
./server 127.0.0.1 10000 10 50
```

**Očekávaný výstup v logu:**
```
[10:30:00] [INFO] Logger initialized
[10:30:00] [INFO] === Pexeso Server Starting ===
[10:30:00] [INFO] Configuration: IP=127.0.0.1, Port=10000, MaxRooms=10, MaxClients=50
[10:30:00] [INFO] Room system initialized (max_rooms=10)
[10:30:00] [INFO] Server initialized: 127.0.0.1:10000 (max_rooms=10, max_clients=50)
[10:30:00] [INFO] Server started, waiting for connections...
```

### Test 3: HELLO a autentizace

```bash
# Terminál 2
nc 127.0.0.1 10000
HELLO Alice
```

**Očekávaný výstup:**
```
WELCOME 1
```

**Server log:**
```
[10:30:15] [INFO] New connection from 127.0.0.1:xxxxx (fd=5)
[10:30:15] [INFO] Client 1: Thread created successfully
[10:30:15] [INFO] Client 1: Received message: 'HELLO Alice'
[10:30:15] [INFO] Client 1 authenticated as 'Alice'
```

### Test 4: LIST_ROOMS (prázdný)

```
LIST_ROOMS
```

**Odpověď:**
```
ROOM_LIST 0
```

### Test 5: CREATE_ROOM

```
CREATE_ROOM MyRoom 4
```

**Odpověď:**
```
ROOM_CREATED 1 MyRoom
```

**Server log:**
```
[10:31:00] [INFO] Client 1 (Alice): Received message: 'CREATE_ROOM MyRoom 4'
[10:31:00] [INFO] Room created: id=1, name='MyRoom', max_players=4, owner=Alice
[10:31:00] [INFO] Client 1 (Alice) created room 1
```

### Test 6: LIST_ROOMS (s místností)

```
# Klient 2
nc 127.0.0.1 10000
HELLO Bob
LIST_ROOMS
```

**Odpověď:**
```
WELCOME 2
ROOM_LIST 1 1 MyRoom 1 4 WAITING
```

### Test 7: JOIN_ROOM

```
# Klient 2 (Bob)
JOIN_ROOM 1
```

**Odpověď klientovi 2:**
```
ROOM_JOINED 1 MyRoom
```

**Broadcast klientovi 1 (Alice):**
```
PLAYER_JOINED Bob
```

**Server log:**
```
[10:32:00] [INFO] Client 2 (Bob): Received message: 'JOIN_ROOM 1'
[10:32:00] [INFO] Client 2 (Bob) joined room 1
```

### Test 8: LEAVE_ROOM

```
# Klient 2 (Bob)
LEAVE_ROOM
```

**Odpověď:**
```
LEFT_ROOM
```

**Broadcast klientovi 1:**
```
PLAYER_LEFT Bob
```

### Test 9: Odpojení hráče v místnosti

```
# Klient 1 (Alice) je v místnosti
# Ctrl+C nebo zavřít terminál
```

**Server log:**
```
[10:33:00] [INFO] Client 1: Connection closed by client
[10:33:00] [INFO] Client 1: Removing from room 1
[10:33:00] [INFO] Client 1 (Alice) left room 1
[10:33:00] [INFO] Room 1 is empty, destroying
[10:33:00] [INFO] Client 1: Closing connection
```

---

## PROTOKOLOVÉ ZPRÁVY

### Klient → Server

| Příkaz | Formát | Popis |
|--------|--------|-------|
| HELLO | `HELLO <nickname>` | Autentizace |
| LIST_ROOMS | `LIST_ROOMS` | Žádost o seznam místností |
| CREATE_ROOM | `CREATE_ROOM <name> <max_players>` | Vytvoření místnosti |
| JOIN_ROOM | `JOIN_ROOM <room_id>` | Vstup do místnosti |
| LEAVE_ROOM | `LEAVE_ROOM` | Opuštění místnosti |
| PONG | `PONG` | Odpověď na PING |

### Server → Klient

| Příkaz | Formát | Popis |
|--------|--------|-------|
| WELCOME | `WELCOME <client_id>` | Úspěšná autentizace |
| ROOM_LIST | `ROOM_LIST <count> <room_data>...` | Seznam místností |
| ROOM_CREATED | `ROOM_CREATED <id> <name>` | Místnost vytvořena |
| ROOM_JOINED | `ROOM_JOINED <id> <name>` | Vstup do místnosti |
| LEFT_ROOM | `LEFT_ROOM` | Opuštění místnosti |
| PLAYER_JOINED | `PLAYER_JOINED <nickname>` | Hráč vstoupil (broadcast) |
| PLAYER_LEFT | `PLAYER_LEFT <nickname>` | Hráč odešel (broadcast) |
| PLAYER_DISCONNECTED | `PLAYER_DISCONNECTED <nick> LONG` | Hráč odpojen |
| ERROR | `ERROR <code> <message>` | Chyba |

---

## CHYBOVÉ STAVY

| Chyba | Kdy nastane |
|-------|-------------|
| `ERROR NOT_AUTHENTICATED` | Příkaz před HELLO |
| `ERROR ALREADY_AUTHENTICATED` | HELLO po autentizaci |
| `ERROR INVALID_PARAMS` | Chybějící nebo neplatné parametry |
| `ERROR ALREADY_IN_ROOM` | CREATE/JOIN když už je v místnosti |
| `ERROR NOT_IN_ROOM` | LEAVE_ROOM když není v místnosti |
| `ERROR ROOM_NOT_FOUND` | JOIN_ROOM s neexistujícím ID |
| `ERROR ROOM_FULL` | JOIN_ROOM do plné místnosti |
| `ERROR ROOM_LIMIT` | CREATE_ROOM při dosažení limitu |

---

## STATISTIKY

| Metrika | Hodnota |
|---------|---------|
| **Nové soubory** | 2 (room.h, room.c) |
| **Upravené soubory** | 4 |
| **Řádky kódu (room.c)** | ~280 LOC |
| **Nové funkce** | 9 (room management) |
| **Nové handlery** | 6 (HELLO, LIST, CREATE, JOIN, LEAVE, PONG) |
| **Thread-safe operace** | ✅ Mutex v room.c |
| **Kompilace** | 0 warnings |

---

## FLOW DIAGRAM - Vytvoření a vstup do místnosti

```
Klient 1 (Alice):
1. HELLO Alice
   ← WELCOME 1

2. CREATE_ROOM MyRoom 4
   ← ROOM_CREATED 1 MyRoom
   [Alice je nyní v room 1]

Klient 2 (Bob):
3. HELLO Bob
   ← WELCOME 2

4. LIST_ROOMS
   ← ROOM_LIST 1 1 MyRoom 1 4 WAITING

5. JOIN_ROOM 1
   ← ROOM_JOINED 1 MyRoom
   [Bob je nyní v room 1]

Klient 1 (Alice) obdrží:
   ← PLAYER_JOINED Bob

6. LEAVE_ROOM (Bob)
   ← LEFT_ROOM
   [Bob je zpět v lobby]

Klient 1 (Alice) obdrží:
   ← PLAYER_LEFT Bob
```

---

## PŘIPRAVENOST PRO ROZŠÍŘENÍ

### ✅ Co je hotovo:
- Správa místností (vytvoření, vstup, opuštění)
- Broadcast zpráv v místnosti
- Automatické cleanup prázdných místností
- Thread-safe operace
- Kompletní logování

### ⏳ Co bude v dalších packetech:
- **START_GAME** - zahájení hry (Packet 6)
- **game.c** - logika Pexesa (Packet 7)
- **FLIP, MATCH, MISMATCH** - herní příkazy (Packet 7)
- **PING/PONG keepalive** - timeout detection (Packet 8)
- **RECONNECT** - automatické znovupřipojení (Packet 9)

---

## ZÁVĚR

**Packet 5 úspěšně dokončen!** ✅

### Co funguje:
- ✅ Autentizace hráčů (HELLO → WELCOME)
- ✅ Seznam místností (LIST_ROOMS)
- ✅ Vytváření místností (CREATE_ROOM)
- ✅ Vstup do místností (JOIN_ROOM)
- ✅ Opuštění místností (LEAVE_ROOM)
- ✅ Broadcast zpráv v místnosti
- ✅ Automatické cleanup při odpojení
- ✅ Thread-safe operace
- ✅ Validace všech vstupů
- ✅ Kompletní logování

**Server je připraven na implementaci herní logiky (Packet 6-7)!**

---

*Dokument vytvořen: 2025-11-12*
*Autor: Claude Code*
*Verze: 1.0*
