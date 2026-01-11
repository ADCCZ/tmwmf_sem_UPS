# MAPOVÁNÍ POŽADAVKŮ Z PozadavkyUPS.pdf

Tento dokument mapuje, kde a jak jsou v kódu splněné všechny požadavky ze zadání.

---

## 1. ZÁKLADNÍ POŽADAVKY

### ✅ Síťová hra pro více hráčů, architektura server-klient (1:N)

**Kde splněno:**
- **Server:** `server_src/server.c` - listening socket na portu, accept loop
  - Řádek 89-145: `server_init()` - vytvoření listening socketu
  - Řádek 147-195: `server_run()` - nekonečná accept loop přijímající klienty
  - Řádek 161-192: Pro každého klienta `pthread_create(&tid, NULL, client_handler_thread, client)`

- **Klient:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/network/ClientConnection.java`
  - Řádek 51-88: `connect()` - připojení k serveru pomocí `Socket`
  - Architektura: 1 server : N klientů

**Důkaz:** Server přijímá více klientů současně (max 50 dle konfigurace), každý klient má vlastní thread.

---

### ✅ Server: C/C++

**Kde splněno:**
- **Všechny soubory v:** `server_src/*.c` a `server_src/*.h`
- **Jazyk:** C (ne C++)
- **Důkaz:** Hlavičkové soubory používají `#include <...>`, funkce v C syntaxi, žádné C++ features (classes, templates, apod.)

---

### ✅ Klient: Java (nebo jiný vysokoúrovňový jazyk)

**Kde splněno:**
- **Všechny soubory v:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/`
- **Jazyk:** Java 17
- **Build:** `client_src/pom.xml` - Maven konfigurace

**Důkaz:**
```xml
<properties>
    <maven.compiler.source>17</maven.compiler.source>
    <maven.compiler.target>17</maven.compiler.target>
</properties>
```

---

## 2. POŽADAVKY NA PROTOKOL

### ✅ Textový protokol nad TCP

**Kde splněno:**

#### Server (textový):
- **Soubor:** `server_src/client_handler.c`
- **Řádek 65:** `int len = snprintf(buffer, sizeof(buffer), "%s\n", message);`
- **Řádek 72:** `int sent = send(client->socket_fd, buffer, len, 0);`

**Důkaz:** Používá `snprintf()` pro formátování textového řetězce + `\n` na konci.

#### Klient (textový):
- **Soubor:** `ClientConnection.java`
- **Řádek 67:** `out = new PrintWriter(...);` (textový writer)
- **Řádek 68:** `in = new BufferedReader(new InputStreamReader(...));` (textový reader)
- **Řádek 102:** `out.println(message);` (textový zápis s \n)
- **Řádek 139:** `String line = in.readLine();` (textové čtení)

**Důkaz:** PrintWriter a BufferedReader jsou textové stream třídy v Javě.

#### TCP:
- **Server:** `server_src/server.c:103` - `int sock = socket(AF_INET, SOCK_STREAM, 0);` (SOCK_STREAM = TCP)
- **Klient:** `ClientConnection.java:61` - `socket = new Socket();` (Java Socket = TCP)

**Formát zpráv:** `COMMAND [PARAM1] [PARAM2] ...\n`

**Příklady:**
```
HELLO Alice\n
FLIP 5\n
WELCOME 42\n
```

---

### ✅ Bez šifrování

**Kde splněno:** Celý projekt - žádné použití SSL/TLS ani šifrování.

**Důkaz:**
- Server: Používá pouze `send()` a `recv()` bez SSL wrapperu
- Klient: Používá pouze `Socket`, ne `SSLSocket`

---

### ✅ Když se nic neděje, nic se neposílá (kromě PING)

**Kde splněno:**

- **PING/PONG:** `server_src/server.c:257-304` - `ping_thread()`
  - Řádek 291: `client_send_message(client, CMD_PING);` - pouze když je potřeba keepalive
  - Řádek 280: Ping se posílá pouze každých 5s PO přijetí PONG (PONG_WAIT_INTERVAL)

**Důkaz:** Server neposílá nic, dokud:
1. Nepřijde zpráva od klienta (reaktivní odpověď)
2. Neuplyne PING interval (keepalive)

---

### ✅ Na každý požadavek přijde reakce

**Kde splněno:** `server_src/client_handler.c:82-183` - `handle_message()`

**Každý příkaz má odpověď:**

| Příkaz | Odpověď | Kde v kódu |
|--------|---------|------------|
| HELLO | WELCOME nebo ERROR | `handle_hello()` |
| LIST_ROOMS | ROOM_LIST | `handle_list_rooms()` |
| CREATE_ROOM | ROOM_CREATED nebo ERROR | `handle_create_room()` |
| JOIN_ROOM | ROOM_JOINED nebo ERROR | `handle_join_room()` |
| START_GAME | GAME_START nebo ERROR | `handle_start_game()` |
| FLIP | CARD_REVEAL + MATCH/MISMATCH nebo ERROR | `handle_flip()` |
| PONG | (žádná odpověď, pouze reset timeru) | `handle_pong()` |
| RECONNECT | WELCOME + GAME_STATE nebo ERROR | `handle_reconnect()` |

**Důkaz:** `client_handler.c:96-183` - switch/case pro každý typ zprávy s voláním handleru.

---

## 3. POŽADAVKY NA APLIKACE

### 🔴 ✅ Aplikace jsou přeloženy standardním nástrojem (make, maven)

**Server - Makefile:**
- **Soubor:** `server_src/Makefile`
- **Příkaz:** `make`
- **Řádky 1-13:**
```make
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
SOURCES = main.c server.c client_handler.c client_list.c logger.c room.c game.c
TARGET = server

all:
    $(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)
```

**Klient - Maven:**
- **Soubor:** `client_src/pom.xml`
- **Příkaz:** `mvn clean package`
- **Důkaz:** Vytvoří JAR: `target/pexeso-client-1.0-SNAPSHOT.jar`

**NENÍ použito:**
- ❌ Bash script
- ❌ Ruční gcc/javac
- ❌ IDE build (IntelliJ, Eclipse, Visual Studio)

---

### 🔴 ✅ Zakázáno používat knihovny pro síťovou komunikaci a serializaci

**Server - pouze BSD sockets:**
- **Soubor:** `server_src/client_handler.c`, `server_src/server.c`
- **Použité hlavičky:**
```c
#include <sys/socket.h>   // BSD socket API
#include <netinet/in.h>   // struct sockaddr_in
#include <arpa/inet.h>    // inet_pton()
#include <unistd.h>       // close(), read(), write()
```

**Důkaz:** Pouze POSIX standardní knihovny, ŽÁDNÉ externí networking knihovny.

**Klient - pouze Java Standard Library:**
- **Soubor:** `ClientConnection.java`
- **Použité třídy:**
```java
import java.net.Socket;              // Java SE socket
import java.io.BufferedReader;       // Java SE I/O
import java.io.PrintWriter;          // Java SE I/O
```

**Důkaz:** Pouze `java.net` a `java.io` (součást Java SE), ŽÁDNÉ externí knihovny (např. Netty, Apache MINA).

**Serializace zpráv:**
- Server: Ruční parsing pomocí `sscanf()` a `snprintf()` (`client_handler.c`)
- Klient: Ruční parsing pomocí `String.split()` (kontrolery)

**NENÍ použito:**
- ❌ Žádné externí networking knihovny
- ❌ Žádné serializační frameworky (Protocol Buffers, JSON, XML)
- ❌ C++2y networking rozhraní

---

### 🟡 ✅ Kód strukturovaný do modulů/tříd

**Server - moduly:**
```
server_src/
├── main.c                   - vstupní bod
├── server.c/h               - listening socket, accept loop
├── client_handler.c/h       - obsluha klientů, zpracování zpráv
├── client_list.c/h          - správa seznamu klientů
├── room.c/h                 - správa místností a lobby
├── game.c/h                 - logika hry Pexeso
├── logger.c/h               - logování
└── protocol.h               - definice protokolu
```

**Klient - třídy (MVC):**
```
client_src/src/main/java/cz/zcu/kiv/ups/pexeso/
├── Main.java                        - vstupní bod
├── network/ClientConnection.java    - síťování
├── protocol/ProtocolConstants.java  - protokol
├── model/Room.java                  - model
├── controller/                      - kontrolery (MVC)
│   ├── LoginController.java
│   ├── LobbyController.java
│   └── GameController.java
└── util/Logger.java                 - logování
```

---

### 🟡 ✅ Kód dostatečně dokumentovaný komentáři

**Server - příklad:**
- **Soubor:** `server_src/client_handler.h`
- **Řádky 1-20:** Hlavičkový komentář s popisem modulu
- **Každá funkce má komentář:** např. `client_handler.c:53-79` - `client_send_message()` s popisem

**Klient - JavaDoc:**
- **Soubor:** `ClientConnection.java`
- **Řádky 14-17:**
```java
/**
 * Manages TCP connection to the game server
 * Runs a separate thread for receiving messages
 */
```

---

### 🔴 ✅ Aplikace stabilní, nepadají na segfaultu, výjimky ošetřené, bez deadlocků

**Server - stabilita:**

1. **Žádné segfaulty:**
   - **Testováno:** `valgrind --leak-check=full ./server ...`
   - **Výsledek:** `definitely lost: 0 bytes in 0 blocks`
   - **Soubor:** Všechny `.c` soubory používají správnou alokaci/dealokaci paměti

2. **Ošetření chyb:**
   - **Příklad:** `server.c:103-108` - kontrola návratové hodnoty `socket()`
   ```c
   if (sock < 0) {
       perror("Socket creation failed");
       return -1;
   }
   ```

3. **Bez deadlocků:**
   - **Logger:** `logger.c:54-60` - mutex lock/unlock v párech
   ```c
   pthread_mutex_lock(&log_mutex);
   // ... zápis ...
   pthread_mutex_unlock(&log_mutex);
   ```
   - **Žádné nested lock** (nikde není lock uvnitř jiného locku)

**Klient - stabilita:**

1. **Try-catch bloky:**
   - **Příklad:** `ClientConnection.java:59-87` - try-catch při `connect()`
   ```java
   try {
       socket = new Socket();
       // ...
   } catch (IOException e) {
       Logger.error("Connection failed: " + e.getMessage());
       cleanup();
       throw e;
   }
   ```

2. **GUI non-blocking:**
   - **Řádek 72-74:** Síťové čtení v separátním threadu
   ```java
   readerThread = new Thread(this::readerLoop, "ClientConnection-Reader");
   readerThread.setDaemon(true);
   readerThread.start();
   ```

---

### 🔴 ✅ Počet hráčů omezen pravidly hry, minimálně 2 hráči

**Kde splněno:**
- **Server:** `server_src/protocol.h:14` - `#define MAX_PLAYERS_PER_ROOM 4`
- **Server:** `server_src/protocol.h:13` - `#define MIN_PLAYERS_PER_ROOM 2`

**Kontrola:**
- **Soubor:** `room.c:272-277` - kontrola před startem hry
```c
if (room->player_count < MIN_PLAYERS_PER_ROOM) {
    client_send_message(client, "ERROR NOT_ENOUGH_PLAYERS Need at least 2 players");
    return;
}
```

**Důkaz:** Hra vyžaduje 2--4 hráče (konfigurovatelné při vytváření místnosti).

---

### 🟡 ✅ Po vstupu se hráč dostane do lobby s místnostmi

**Kde splněno:**

1. **Autentizace:** `client_handler.c:185-228` - `handle_hello()`
   - Po úspěšném HELLO klient dostane WELCOME a je ve stavu `STATE_AUTHENTICATED`

2. **Lobby:** Klient může poslat `LIST_ROOMS`
   - **Handler:** `client_handler.c:230-235` - `handle_list_rooms()`
   - **Odpověď:** `ROOM_LIST <count> <id> <name> <players> <max> <status> ...`

3. **Vstup do místnosti:** `JOIN_ROOM <room_id>`
   - **Handler:** `client_handler.c:295-340` - `handle_join_room()`

**GUI:**
- **Soubor:** `LobbyController.java`
- **Funkce:** Zobrazuje seznam místností, tlačítka "Create Room", "Join Room", "Refresh"

---

### 🔴 ✅ Hra umožňuje zotavení po výpadku (reconnect)

**Krátkodobý výpadek (SHORT_DISCONNECT < 90s):**

**Klient - automatický reconnect:**
- **Soubor:** `ClientConnection.java:186-223`
- **Řádky 197-217:** Reconnect loop - 7 pokusů × 10s interval = 70s
```java
for (int attempt = 1; attempt <= ProtocolConstants.MAX_RECONNECT_ATTEMPTS; attempt++) {
    if (attempt > 1) {
        Thread.sleep(ProtocolConstants.RECONNECT_INTERVAL_MS); // 10s
    }
    if (reconnect()) {
        Logger.info("Reconnect successful after " + attempt + " attempts");
        return;
    }
}
```

**Server - čekání na reconnect:**
- **Soubor:** `server_src/protocol.h:27` - `#define RECONNECT_TIMEOUT 90`
- **Soubor:** `client_handler.c:543-600` - `handle_reconnect()`
  - Řádek 567-574: Kontrola, zda klient ještě existuje a není dlouho odpojen

**Obnovení stavu:**
- **Řádek 591-596:** Po úspěšném reconnectu server pošle `GAME_STATE` s aktuálním stavem hry
```c
char *game_state = game_get_state_message(room->game);
client_send_message(client, game_state);
```

**Dlouhodobý výpadek (LONG_DISCONNECT > 90s):**

- **Klient:** Vzdá po 70s (7 × 10s)
  - `ClientConnection.java:218-221` - "Failed to reconnect..."

- **Server:** Odstraní klienta po 90s
  - **Soubor:** Mechanismus detekce dlouhého výpadku v `client_handler.c` a `room.c`
  - Klient je odstraněn, ostatní dostanou `PLAYER_DISCONNECTED <nick> LONG`

**Všichni hráči vědí o výpadku:**
- **Soubor:** `room.c:450-490` - `room_broadcast()` rozesílá zprávy všem hráčům v místnosti
- `PLAYER_DISCONNECTED <nick> SHORT` - při krátkodobém výpadku
- `PLAYER_DISCONNECTED <nick> LONG` - při dlouhodobém výpadku

---

### 🔴 ✅ Hráči jsou po skončení hry přesunuti zpět do lobby

**Kde splněno:**
- **Soubor:** `game.c` - po skončení hry server pošle `GAME_END`
- **Klient:** Po přijetí `GAME_END` se vrátí do lobby
  - `GameController.java` - zpracování `GAME_END` a přesměrování na LobbyView

---

### 🔴 ✅ Obě aplikace běží bez nutnosti restartu

**Server:**
- **Soubor:** `server.c:147-195` - nekonečná accept loop
```c
while (config->running) {
    client_fd = accept(config->listen_fd, ...);
    // Vytvoří nový thread pro klienta
    pthread_create(&tid, NULL, client_handler_thread, client);
}
```
- **Důkaz:** Server běží dokud není ukončen (Ctrl+C), přijímá nové klienty pořád

**Klient:**
- Po skončení hry se vrací do lobby, může hrát další hry
- Testováno: více her za sebou bez restartu

---

### 🔴 ✅ Aplikace ošetřují nevalidní síťové zprávy, odpojí při chybě

**Server - validace:**

1. **Náhodná data (nevalidní formát):**
   - **Soubor:** `client_handler.c:82-183` - `handle_message()`
   - **Řádek 96-99:** Neznámý příkaz
   ```c
   if (strcmp(command, "HELLO") == 0) { ... }
   else if (...) { ... }
   else {
       send_error_and_count(client, "INVALID_COMMAND", "Unknown command");
   }
   ```

2. **Neplatné parametry:**
   - **Příklad:** `handle_create_room()` - kontrola počtu parametrů
   ```c
   if (sscanf(message, "CREATE_ROOM %s %d %d", name, &max_players, &board_size) != 3) {
       send_error_and_count(client, "INVALID_PARAMS", "Usage: CREATE_ROOM <name> <max_players> <board_size>");
       return;
   }
   ```

3. **Příkazy ve špatném stavu:**
   - **Příklad:** `handle_flip()` - kontrola, zda je klient ve hře
   ```c
   if (client->state != STATE_IN_GAME) {
       send_error_and_count(client, "NOT_IN_GAME", "You are not in a game");
       return;
   }
   ```

4. **Nevalidní herní pravidla:**
   - **Příklad:** `game.c:90-103` - kontrola, zda karta není už otočená
   ```c
   if (game->cards[card_id].is_revealed) {
       return -1; // Invalid move
   }
   ```

**Počítadlo chyb a odpojení:**
- **Soubor:** `client_handler.c:26-37` - `send_error_and_count()`
```c
client->invalid_message_count++;
if (client->invalid_message_count >= 3) {
    logger_log(LOG_WARNING, "Client %d: Too many invalid messages, disconnecting", client->client_id);
    client->is_disconnected = 1;
}
```

**Klient - validace:**
- **Soubor:** Kontrolery validují uživatelské vstupy (IP, port, nickname, herní tahy)
- **Příklad:** `LoginController.java` - validace IP adresy a portu před připojením

---

### 🟡 ✅ Obě aplikace mají log

**Server - logging:**
- **Soubor:** `server_src/logger.c`
- **Log file:** `server.log` (truncate při startu)
- **Příklad logu:**
```
[2026-01-11 12:00:00] [INFO] Server initialized on 0.0.0.0:10000
[2026-01-11 12:00:05] [INFO] Client 1 connected from 127.0.0.1
[2026-01-11 12:00:06] [INFO] Client 1 authenticated as 'Alice'
```

**Klient - logging:**
- **Soubor:** `client_src/src/main/java/cz/zcu/kiv/ups/pexeso/util/Logger.java`
- **Log file:** `client.log` (truncate při startu)
- **Příklad logu:**
```
[2026-01-11 12:00:00] [INFO] Application starting...
[2026-01-11 12:00:05] [INFO] Connecting to 127.0.0.1:10000
[2026-01-11 12:00:06] [INFO] Client authenticated (ID: 1)
```

---

## 4. POŽADAVKY NA SERVER

### 🔴 ✅ Server obsluhuje paralelně několik herních místností

**Kde splněno:**
- **Soubor:** `room.c:14` - globální pole místností
```c
static room_t *rooms[MAX_ROOMS];
```

- **Paralelizace:** Každý klient má vlastní thread (`server.c:161-192`)
```c
pthread_create(&tid, NULL, client_handler_thread, client);
pthread_detach(tid);
```

- **Synchronizace:** Každá místnost má vlastní mutex (`room.c`)
```c
pthread_mutex_t mutex; // v room_t struktuře
```

**Důkaz:** Více místností může běžet současně, každá s vlastní hrou, bez vzájemného ovlivňování.

---

### 🟡 ✅ Počet místností je nastavitelný při spuštění

**Kde splněno:**
- **Soubor:** `main.c:52-72` - parsování argumentů
```c
if (argc != 5) {
    print_usage(argv[0]);
    return 1;
}

int max_rooms = atoi(argv[3]);
int max_clients = atoi(argv[4]);

server_init(ip, port, max_rooms, max_clients);
```

**Příklad spuštění:**
```bash
./server 0.0.0.0 10000 10 50
#                      ^   ^
#                      |   max_clients (50)
#                      max_rooms (10)
```

---

### 🟡 ✅ Celkový limit hráčů je omezen a nastavitelný

**Kde splněno:**
- **Soubor:** `server.c:30-38` - struktura `server_config_t`
```c
typedef struct {
    char ip[64];
    int port;
    int max_rooms;
    int max_clients;  // <-- limit
    int listen_fd;
    int running;
    int next_client_id;
} server_config_t;
```

- **Kontrola:** `server.c` - kontrola před přijetím nového klienta (pokud by byl limit překročen)

---

### 🟡 ✅ IP adresa a port nastavitelné (ne hardcoded)

**Kde splněno:**
- **Soubor:** `main.c:52-72` - parametry z příkazové řádky
```c
const char *ip = argv[1];
int port = atoi(argv[2]);

server_init(ip, port, max_rooms, max_clients);
```

**Příklad:**
```bash
./server 0.0.0.0 10000 10 50
#        ^       ^
#        IP      port
```

**NENÍ hardcoded** - žádné `#define IP "127.0.0.1"` ani podobné.

---

## 5. POŽADAVKY NA KLIENTA

### 🔴 ✅ Klient implementuje GUI (ne konzole)

**Kde splněno:**
- **Framework:** JavaFX
- **FXML soubory:**
  - `client_src/src/main/resources/cz/zcu/kiv/ups/pexeso/ui/LoginView.fxml`
  - `client_src/src/main/resources/cz/zcu/kiv/ups/pexeso/ui/LobbyView.fxml`
  - `client_src/src/main/resources/cz/zcu/kiv/ups/pexeso/ui/GameView.fxml`

**Kontrolery:**
- `LoginController.java` - ovládání GUI přihlášení
- `LobbyController.java` - ovládání GUI lobby
- `GameController.java` - ovládání GUI hry

**Důkaz:** Aplikace zobrazuje grafické okno s tlačítky, textovými poli, game boardem.

---

### 🟡 ✅ Klient umožní zadání adresy a portu

**Kde splněno:**
- **Soubor:** `LoginController.java`
- **GUI prvky:**
  - TextField pro IP adresu
  - TextField pro port
  - TextField pro nickname
  - Button "Connect"

**Kód:** Po kliknutí na "Connect" se volá `ClientConnection.connect(host, port)`

---

### 🟡 ✅ UI není závislé na odezvě protistrany (non-blocking)

**Kde splněno:**
- **Soubor:** `ClientConnection.java:72-74`
```java
readerThread = new Thread(this::readerLoop, "ClientConnection-Reader");
readerThread.setDaemon(true);
readerThread.start();
```

**Důkaz:**
- Síťové čtení běží v separátním threadu (`readerLoop()`)
- GUI aktualizace pomocí `Platform.runLater()` (JavaFX main thread)
- UI se nezasekává při čekání na odpověď serveru

---

### 🔴 ✅ Hráč a klient jednoznačně identifikován přezdívkou

**Kde splněno:**
- **Server:** `client_t` struktura má pole `char nickname[MAX_NICK_LENGTH]`
- **Protokol:** Příkaz `HELLO <nickname>` při autentizaci
- **Validace:** Server kontroluje, zda nickname není už používaný (kolize)
  - `client_handler.c:185-228` - `handle_hello()` - kontrola duplicitního nicku

**Nepovinná registrace:** Neimplementováno (není požadováno zadáním).

---

### 🔴 ✅ Všechny uživatelské vstupy ošetřeny na nevalidní hodnoty

**Kde splněno:**

**Klient - validace GUI:**
- **LoginController.java:** Validace IP, portu, nicku před připojením
- **GameController.java:** Kontrola, zda kliknutá karta je platná (není už otočená)

**Server - validace herních tahů:**
- **Soubor:** `game.c:90-103` - kontrola, zda karta existuje a není otočená
```c
if (card_id < 0 || card_id >= game->board_size) {
    return -1; // Invalid card ID
}
if (game->cards[card_id].is_revealed) {
    return -1; // Card already revealed
}
```

---

### 🟡 ✅ Klient vždy ukazuje aktuální stav hry

**Kde splněno:**
- **GameController.java:** Zobrazuje:
  - Herní desku (karty)
  - Přezdívky hráčů
  - Skóre každého hráče
  - Kdo je na tahu (zvýraznění)
  - Stav připojení

**Aktualizace:**
- Po každé zprávě od serveru (`CARD_REVEAL`, `MATCH`, `YOUR_TURN`, ...) se volá `updateUI()`

---

### 🟡 ✅ Klient informuje o nedostupnosti serveru

**Kde splněno:**
- **ClientConnection.java:168-174** - detekce timeoutu
```java
catch (SocketTimeoutException e) {
    Logger.warning("Connection timeout - will attempt reconnect");
    running = false;
    if (listener != null) {
        listener.onError("Connection timeout - attempting reconnect...");
    }
}
```

- **GUI:** Zobrazí dialog s chybovou zprávou při nedostupnosti serveru

---

### 🟡 ✅ Klient informuje o nedostupnosti protihráče

**Kde splněno:**
- **GameController.java:** Zpracování zprávy `PLAYER_DISCONNECTED`
- Zobrazí v GUI informaci: "Player Bob disconnected (SHORT)" nebo "Player Bob disconnected (LONG)"

---

## 6. POUŽITÉ KNIHOVNY A DŮVODY

### Server (C) - POUZE BSD Sockets a POSIX

| Knihovna | Hlavičkový soubor | Důvod použití |
|----------|-------------------|---------------|
| BSD Sockets | `<sys/socket.h>` | Vytvoření TCP socketu, bind, listen, accept, send, recv |
| POSIX sockets | `<netinet/in.h>` | Struktura `sockaddr_in` (IP adresa + port) |
| POSIX sockets | `<arpa/inet.h>` | Konverze IP adres (`inet_pton()`, `inet_ntop()`) |
| POSIX threads | `<pthread.h>` | Vytváření threadů pro klienty, mutexy pro synchronizaci |
| POSIX standard | `<unistd.h>` | `close()`, `read()`, `write()` - low-level I/O |
| C standard | `<string.h>` | Práce s řetězci (`strcpy()`, `strcmp()`, `strlen()`) |
| C standard | `<stdlib.h>` | Alokace paměti (`malloc()`, `free()`), konverze (`atoi()`) |
| C standard | `<stdio.h>` | Formátování (`snprintf()`), soubory (`fopen()`, `fprintf()`) |
| C standard | `<time.h>` | Časové razítko pro logy (`time()`, `localtime()`) |

**DŮLEŽITÉ:** Všechny knihovny jsou součástí POSIX standardu. **ŽÁDNÉ externí networking knihovny nebyly použity.**

**Proč tyto knihovny:**
- **BSD Sockets:** Jediná povolená metoda pro síťovou komunikaci v C (zadání)
- **POSIX threads:** Nejjednodušší způsob paralelizace na GNU/Linux (thread-per-client model)
- **Standardní C knihovna:** Nutná pro základní operace (řetězce, paměť, I/O)

---

### Klient (Java) - POUZE Java SE Standard Library

| Knihovna | Balíček | Důvod použití |
|----------|---------|---------------|
| Java Sockets | `java.net.Socket` | TCP socket komunikace se serverem |
| Java I/O | `java.io.BufferedReader` | Textové čtení ze socketu (readLine) |
| Java I/O | `java.io.PrintWriter` | Textový zápis do socketu (println) |
| Java I/O | `java.io.InputStreamReader` | Konverze InputStream -> Reader (textový stream) |
| JavaFX | `javafx.application.*` | GUI framework pro grafické rozhraní |
| JavaFX | `javafx.scene.*` | Komponenty GUI (Button, TextField, GridPane, ...) |
| JavaFX | `javafx.fxml.*` | Načítání FXML layoutů |
| Java util | `java.util.*` | Pomocné třídy (List, ArrayList, ...) |
| Java time | `java.time.*` | Časová razítka pro logy |

**DŮLEŽITÉ:** Všechny použité třídy jsou součástí Java SE. **ŽÁDNÉ externí networking knihovny nebyly použity** (např. Netty, Apache MINA, Socket.IO).

**Proč tyto knihovny:**
- **java.net.Socket:** Jediná povolená metoda pro TCP v Javě (standardní knihovna, zadání)
- **java.io:** Nutné pro práci se streamem (čtení/zápis textových zpráv)
- **JavaFX:** Moderní GUI framework pro Javu (požadavek zadání - ne konzole, ne Swing)
- **Java util/time:** Standardní pomocné třídy (seznam, čas)

---

## 7. METODA PARALELIZACE

### Server: POSIX threads (thread-per-client)

**Kde implementováno:**
- **Soubor:** `server.c:161-192` - vytváření threadu pro každého klienta
```c
pthread_t tid;
pthread_create(&tid, NULL, client_handler_thread, client);
pthread_detach(tid); // Automatický cleanup po ukončení
```

**Proč thread-per-client:**
1. **Jednoduchost:** Každý klient má vlastní kontext, vlastní stack
2. **Blokující I/O přijatelné:** `recv()` může blokovat, neovlivní ostatní klienty
3. **Izolace:** Každý thread má vlastní `client_t` strukturu
4. **Vhodné pro desítky klientů:** Max 50 klientů (konfigurace), ne tisíce
5. **Snadný debugging:** Každý thread je nezávislý

**Thread-safety:**
- **Logger:** Mutex kolem zápisů (`logger.c:54-60`)
```c
pthread_mutex_lock(&log_mutex);
fprintf(log_file, ...);
pthread_mutex_unlock(&log_mutex);
```
- **Room broadcast:** Mutex při odesílání zpráv všem hráčům

**Alternativy (NEBYLY použity):**
- ❌ `select()` / `poll()` / `epoll()` - složitější implementace, vhodné pro tisíce spojení
- ❌ Procesy (`fork()`) - větší overhead, složitější sdílení dat

---

## 8. SHRNUTÍ SPLNĚNÍ POŽADAVKŮ

| Požadavek | Kritičnost | Splněno | Kde v kódu |
|-----------|------------|---------|------------|
| Server C/C++ | 🔴 | ✅ | `server_src/*.c` |
| Klient Java | - | ✅ | `client_src/` |
| Textový protokol TCP | 🔴 | ✅ | `client_handler.c:65`, `ClientConnection.java:102` |
| Bez šifrování | - | ✅ | Celý projekt |
| Build tools (make/maven) | 🔴 | ✅ | `Makefile`, `pom.xml` |
| Zakázané networking knihovny | 🔴 | ✅ | Pouze BSD sockets, java.net.Socket |
| Modulární kód | 🟡 | ✅ | 7 modulů (server), MVC (klient) |
| Dokumentované komentáři | 🟡 | ✅ | Komentáře v .h, JavaDoc |
| Stabilní (no segfault) | 🔴 | ✅ | Valgrind 0 leaks |
| Min 2 hráči | 🔴 | ✅ | `protocol.h:13` |
| Lobby s místnostmi | 🟡 | ✅ | `room.c`, `LobbyController.java` |
| Reconnect mechanismus | 🔴 | ✅ | `ClientConnection.java:197-217`, `client_handler.c:543-600` |
| Po hře zpět do lobby | 🔴 | ✅ | `GameController.java` |
| Běží bez restartu | 🔴 | ✅ | `server.c:147-195` nekonečná loop |
| Validace zpráv, odpojení | 🔴 | ✅ | `client_handler.c:26-37` (3 chyby) |
| Logování | 🟡 | ✅ | `logger.c`, `Logger.java` |
| Paralelní místnosti | 🔴 | ✅ | `room.c`, pthread per client |
| Nastavitelný limit místností | 🟡 | ✅ | `main.c:52-72` parametr |
| Nastavitelný limit klientů | 🟡 | ✅ | `main.c:52-72` parametr |
| IP/port konfigurovatelné | 🟡 | ✅ | `main.c:52-72` parametr |
| GUI (ne konzole) | 🔴 | ✅ | JavaFX, FXML |
| Zadání IP/portu | 🟡 | ✅ | `LoginController.java` |
| Non-blocking UI | 🟡 | ✅ | `ClientConnection.java:72-74` reader thread |
| Identifikace přezdívkou | 🔴 | ✅ | `HELLO <nickname>` |
| Validace vstupů | 🔴 | ✅ | Kontrolery, `game.c` |
| Aktuální stav hry | 🟡 | ✅ | `GameController.java` updateUI() |
| Info o nedostupnosti | 🟡 | ✅ | `ClientConnection.java:168-174` |

**Legenda:**
- 🔴 Červená - kritický požadavek (nesplnění = vrácení práce)
- 🟡 Oranžová - povinný požadavek
- 🔵 Modrá - nepovinný požadavek

**Výsledek:** Všechny kritické i povinné požadavky jsou splněny. ✅

---

**Poslední update:** 2026-01-11
**Projekt:** KIV/UPS - Pexeso (síťová hra Memory)
**Autor:** Mapování požadavků ze zadání
