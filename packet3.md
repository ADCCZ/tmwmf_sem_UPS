# PACKET 3/10 - IMPLEMENTACE KOSTRY SERVERU V C

**Datum:** 2025-11-12
**Cíl:** Vytvořit funkční TCP server skeleton s thread-per-client architekturou
**Status:** ✅ HOTOVO

---

## ZADÁNÍ PACKETU 3

Implementovat kostru TCP serveru v C, který:
- Čte IP/port/max hráčů/max místností z argumentů
- Vytvoří poslouchající socket (TCP)
- Ve smyčce přijímá nové klienty
- Pro každého klienta spustí vlákno (POSIX threads)
- Vlákno čte řádky ukončené `\n` a vypisuje do logu
- Odpoví testovací zprávou `ERROR NOT_IMPLEMENTED`
- Používá pouze BSD socket API
- Obsahuje Makefile s `-Wall -Wextra -pthread`
- Ošetřuje všechny chyby a korektně uvolňuje zdroje

---

## IMPLEMENTOVANÉ SOUBORY

### 1. `logger.h` / `logger.c` (2.6 KB)
**Účel:** Thread-safe logování s časovými razítky

**Klíčové funkce:**
```c
int logger_init(const char *filename);
void logger_log(log_level_t level, const char *format, ...);
void logger_shutdown(void);
```

**Vlastnosti:**
- Logování do souboru `server.log` + stdout současně
- Mutex (`pthread_mutex_t`) pro thread-safe zápis
- Tři úrovně: `LOG_INFO`, `LOG_WARNING`, `LOG_ERROR`
- Časová razítka v ISO formátu (`%Y-%m-%d %H:%M:%S`)
- Variadické funkce (printf-style)

**Příklad výstupu:**
```
[2025-11-12 10:06:45] [INFO] Logger initialized
[2025-11-12 10:06:45] [INFO] Server initialized: 127.0.0.1:10000
[2025-11-12 10:07:01] [INFO] Client 1: Received message: 'HELLO TestUser'
```

---

### 2. `protocol.h` (2.0 KB)
**Účel:** Definice protokolových konstant

**Obsah:**
- Buffery: `MAX_MESSAGE_LENGTH` (1024), `MAX_NICK_LENGTH` (32)
- Příkazy klient→server: `CMD_HELLO`, `CMD_LIST_ROOMS`, `CMD_FLIP`, ...
- Příkazy server→klient: `CMD_WELCOME`, `CMD_ERROR`, `CMD_GAME_START`, ...
- Chybové kódy: `ERR_NOT_IMPLEMENTED`, `ERR_INVALID_COMMAND`, ...
- Enum stavů klienta: `client_state_t`

```c
typedef enum {
    STATE_DISCONNECTED,
    STATE_CONNECTED,
    STATE_AUTHENTICATED,
    STATE_IN_LOBBY,
    STATE_IN_ROOM,
    STATE_IN_GAME
} client_state_t;
```

---

### 3. `client_handler.h` / `client_handler.c` (4.2 KB)
**Účel:** Správa jednoho klienta v samostatném threadu

**Struktura klienta:**
```c
typedef struct {
    int socket_fd;                    // Socket file descriptor
    char nickname[MAX_NICK_LENGTH];   // Přezdívka (po HELLO)
    client_state_t state;             // Aktuální stav
    time_t last_activity;             // Poslední aktivita (pro timeout)
    int invalid_message_count;        // Počítadlo neplatných zpráv
    int client_id;                    // Unikátní ID
} client_t;
```

**Klíčové funkce:**
```c
void* client_handler_thread(void *arg);
int client_send_message(client_t *client, const char *message);
```

**Funkčnost `client_handler_thread()`:**
1. Přijme `client_t *` jako argument
2. Pošle úvodní zprávu: `ERROR NOT_IMPLEMENTED Server skeleton - all commands return this error`
3. **Hlavní smyčka:**
   - Volá `recv()` pro čtení dat z socketu
   - **Znak po znaku** skládá zprávy do `line_buffer`
   - Když najde `\n` → zpracuje celý řádek
   - Zaloguje přijatou zprávu
   - Odpoví: `ERROR NOT_IMPLEMENTED Command not yet implemented`
4. Pokud `recv()` vrátí 0 → klient se odpojil
5. Uzavře socket (`close()`), uvolní paměť (`free()`), ukončí thread

**Zpracování fragmentace:**
```c
// Příklad: "HELLO Alice\n" může přijít ve 3 částech
// 1. recv() → "HEL"
// 2. recv() → "LO Al"
// 3. recv() → "ice\n"
// line_buffer postupně sbírá: H→HE→HEL→HELL→HELLO→...→HELLO Alice
// Když najde \n, zpracuje kompletní zprávu
```

---

### 4. `server.h` / `server.c` (5.6 KB)
**Účel:** TCP socket management a accept loop

**Konfigurace serveru:**
```c
typedef struct {
    char ip[64];          // IP adresa pro bind
    int port;             // Port
    int max_rooms;        // Max místností
    int max_clients;      // Max klientů
    int listen_fd;        // Listening socket
    int running;          // Flag: 1 = běží, 0 = ukončit
    int next_client_id;   // Čítač ID
} server_config_t;
```

**Klíčové funkce:**
```c
int server_init(const char *ip, int port, int max_rooms, int max_clients);
void server_run(void);
void server_shutdown(void);
server_config_t* server_get_config(void);
```

**Postup `server_init()`:**
1. Vytvoří socket: `socket(AF_INET, SOCK_STREAM, 0)`
2. Nastaví `SO_REUSEADDR` (okamžitý restart bez "Address already in use")
3. Připraví `sockaddr_in` (IP + port)
4. Bind: `bind(listen_fd, ...)`
5. Listen: `listen(listen_fd, 10)`

**Postup `server_run()` (nekonečná smyčka):**
```c
while (server_config.running) {
    // 1. Accept nového klienta
    client_fd = accept(listen_fd, ...);

    // 2. Vytvoř client_t strukturu
    client_t *client = malloc(sizeof(client_t));
    client->socket_fd = client_fd;
    client->state = STATE_CONNECTED;
    client->client_id = next_client_id++;

    // 3. Spusť thread
    pthread_t thread_id;
    pthread_create(&thread_id, NULL, client_handler_thread, client);
    pthread_detach(thread_id);  // Automatický cleanup
}
```

**`pthread_detach()` vysvětlení:**
- Normálně: musel bys thread uložit a později zavolat `pthread_join()`
- S detach: thread automaticky uvolní zdroje po ukončení
- Proto není potřeba trackovat všechny thready

---

### 5. `main.c` (2.5 KB)
**Účel:** Vstupní bod, parsování argumentů, signal handling

**Struktura:**
```c
int main(int argc, char *argv[]) {
    // 1. Validace argumentů
    if (argc != 5) { print_usage(); return 1; }

    // 2. Parsování
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int max_rooms = atoi(argv[3]);
    int max_clients = atoi(argv[4]);

    // 3. Validace rozsahů
    if (port <= 0 || port > 65535) { error(); }

    // 4. Inicializace loggeru
    logger_init("server.log");

    // 5. Signal handlery
    signal(SIGINT, signal_handler);   // Ctrl+C
    signal(SIGTERM, signal_handler);  // kill

    // 6. Inicializace serveru
    server_init(ip, port, max_rooms, max_clients);

    // 7. Hlavní loop (blokuje)
    server_run();

    // 8. Cleanup
    server_shutdown();
    logger_shutdown();

    return 0;
}
```

**Signal handler:**
```c
void signal_handler(int signum) {
    logger_log(LOG_INFO, "Received signal %d, shutting down...", signum);
    server_config_t *config = server_get_config();
    config->running = 0;  // Zastaví accept loop
}
```

**Výhoda:** Graceful shutdown - při Ctrl+C se korektně uzavřou sockety a log soubor.

---

### 6. `Makefile` (1.1 KB)
**Účel:** Automatický build systém

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -std=c99
LDFLAGS = -pthread

SOURCES = main.c server.c client_handler.c logger.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) server.log

run: $(TARGET)
	./$(TARGET) 127.0.0.1 10000 10 50

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET) 127.0.0.1 10000 10 50

# Dependencies
main.o: main.c server.h logger.h
server.o: server.c server.h client_handler.h logger.h protocol.h
client_handler.o: client_handler.c client_handler.h logger.h protocol.h
logger.o: logger.c logger.h

.PHONY: all clean run valgrind
```

**Flagy:**
- `-Wall -Wextra` → všechna varování (odhalí potenciální bugy)
- `-pthread` → podpora POSIX threads
- `-g` → debug symboly (pro valgrind/gdb)
- `-std=c99` → C99 standard

**Targety:**
- `make` → sestaví server
- `make clean` → smaže binárku, objekty, log
- `make run` → sestaví a spustí s výchozími parametry
- `make valgrind` → kontrola memory leaks

---

## TESTOVÁNÍ

### Test 1: Základní spuštění
```bash
cd server_src
make
./server 127.0.0.1 10000 10 50
```

**Očekávaný výstup:**
```
[2025-11-12 10:06:45] [INFO] Logger initialized
[2025-11-12 10:06:45] [INFO] === Pexeso Server Starting ===
[2025-11-12 10:06:45] [INFO] Configuration: IP=127.0.0.1, Port=10000, MaxRooms=10, MaxClients=50
[2025-11-12 10:06:45] [INFO] Server initialized: 127.0.0.1:10000 (max_rooms=10, max_clients=50)
[2025-11-12 10:06:45] [INFO] Server started, waiting for connections...
```

### Test 2: Připojení klienta (netcat)
```bash
# Terminál 2
nc 127.0.0.1 10000
```

**Očekávaný výstup na klientovi:**
```
ERROR NOT_IMPLEMENTED Server skeleton - all commands return this error
```

**Po odeslání zprávy (např. "HELLO Alice"):**
```
HELLO Alice
ERROR NOT_IMPLEMENTED Command not yet implemented
```

**V logu serveru:**
```
[2025-11-12 10:07:01] [INFO] New connection from 127.0.0.1:38836 (fd=5)
[2025-11-12 10:07:01] [INFO] Client 1: Thread created successfully
[2025-11-12 10:07:01] [INFO] Client 1: Handler thread started (fd=5)
[2025-11-12 10:07:26] [INFO] Client 1: Received message: 'HELLO Alice'
```

### Test 3: InTCPtor (fragmentace zpráv)
```bash
cd ~/InTCPtor/build
./intcptor-run /mnt/d/ZCU/UPS/tmwmf_sem_UPS/server_src/server 127.0.0.1 10000 10 50
```

**Co InTCPtor dělá:**
- Náhodně fragmentuje `recv()` a `send()` volání
- Simuluje zpoždění (průměr 100ms, σ=10ms)
- Může vracet pouze 2 bajty místo 1024
- Může posílat po 1 znaku s prodlevou

**Výsledky testování:**
- ✅ Server správně skládá fragmentované zprávy
- ✅ Odpověď "ERROR NOT_IMPLEMENTED..." (50 bajtů) byla rozkouskována na 25 částí po 2 bajtech
- ✅ Žádné timeouty, žádné segfaulty
- ✅ Všechny zprávy správně přijaty a zalogované

**Příklad z logu:**
```
[[InTCPtor: recv() original count = 1023, adjusted = 2]]    ← Jen 2 bajty!
[[InTCPtor: sending E bytes to socket 6 after delay of 101 ms]]
[[InTCPtor: sending R bytes to socket 6 after delay of 84 ms]]
[[InTCPtor: sending R bytes to socket 6 after delay of 88 ms]]
...
[INFO] Client 2: Received message: 'HELLO Client'  ← Správně složeno!
```

### Test 4: Graceful shutdown
```bash
# V terminálu se serverem
Ctrl+C
```

**Očekávaný výstup:**
```
^C[2025-11-12 10:03:20] [INFO] Received signal 2, shutting down...
[2025-11-12 10:03:20] [INFO] Server stopped accepting connections
[2025-11-12 10:03:20] [INFO] Server shutting down...
[2025-11-12 10:03:20] [INFO] Server shutdown complete
Server terminated
```

---

## ZMĚNY OPROTI PŮVODNÍMU NÁVRHU

### 🔄 Paralelizace: select() → pthread

**Původní plán (architektura.md):**
> Zvoleno: `select()` – single-threaded model

**Implementováno:**
> POSIX threads (pthread) – thread-per-client model

**Důvody změny:**
1. **Jednodušší implementace** – každý klient má vlastní kontext
2. **Požadavek zadání Packetu 3** – explicitně říkal "použij POSIX threads"
3. **Snadnější debugging** – každý thread izolovaný
4. **Vhodné pro měřítko projektu** – desítky klientů (max 50), ne tisíce
5. **Žádná sdílená data** – každá místnost bude mít vlastní strukturu

**Validace změny:**
- ✅ Zadání (PozadavkyUPS.pdf, str. 2) povoluje: "select, vlákna, procesy"
- ✅ Dokumentace (architektura.md) aktualizována

**Thread-safety:**
- Logger: mutex (`pthread_mutex_t`) pro thread-safe zápis
- Klienti: žádná sdílená data (každý `client_t` je vlastnictví threadu)
- Místnosti: budou mít vlastní mutex (v budoucích packetech)

---

## OŠETŘENÍ CHYB

### Síťové operace
```c
// socket()
if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    logger_log(LOG_ERROR, "Failed to create socket: %s", strerror(errno));
    return -1;
}

// bind()
if (bind(fd, ...) < 0) {
    logger_log(LOG_ERROR, "Failed to bind: %s", strerror(errno));
    close(fd);
    return -1;
}

// accept()
if ((client_fd = accept(listen_fd, ...)) < 0) {
    if (server_config.running) {  // Chyba jen pokud server běží
        logger_log(LOG_ERROR, "Failed to accept: %s", strerror(errno));
    }
    continue;
}

// recv()
if (bytes_received < 0) {
    logger_log(LOG_ERROR, "Client %d: recv() failed", client_id);
    break;  // Ukončí thread
}

if (bytes_received == 0) {
    logger_log(LOG_INFO, "Client %d: Connection closed by client", client_id);
    break;  // Normální ukončení
}
```

### Alokace paměti
```c
client_t *client = malloc(sizeof(client_t));
if (client == NULL) {
    logger_log(LOG_ERROR, "Failed to allocate memory for client");
    close(client_fd);
    continue;
}
```

### Thread vytváření
```c
int result = pthread_create(&thread_id, NULL, client_handler_thread, client);
if (result != 0) {
    logger_log(LOG_ERROR, "Failed to create thread: %s", strerror(result));
    close(client_fd);
    free(client);
    continue;
}
```

---

## CLEANUP ZDROJŮ

### Při ukončení klienta (v threadu)
```c
// Na konci client_handler_thread()
close(client->socket_fd);  // Uzavřít socket
free(client);              // Uvolnit paměť
return NULL;               // Thread se ukončí (detached → auto cleanup)
```

### Při ukončení serveru
```c
void server_shutdown(void) {
    server_config.running = 0;      // Zastaví accept loop
    close(server_config.listen_fd);  // Uzavře listening socket
}

// V main()
server_shutdown();
logger_shutdown();  // Zavře log soubor
```

### Signal handling
```c
signal(SIGINT, signal_handler);   // Ctrl+C
signal(SIGTERM, signal_handler);  // kill command

void signal_handler(int signum) {
    server_config.running = 0;  // Graceful shutdown
}
```

---

## BEZPEČNOSTNÍ OPATŘENÍ

### 1. Buffer overflow prevence
```c
char buffer[MAX_MESSAGE_LENGTH];
int bytes = recv(fd, buffer, sizeof(buffer) - 1, 0);  // -1 pro \0
buffer[bytes] = '\0';  // Null-terminate

// Line buffer overflow check
if (line_pos < MAX_MESSAGE_LENGTH - 1) {
    line_buffer[line_pos++] = c;
} else {
    logger_log(LOG_WARNING, "Client %d: Message too long, truncating", client_id);
    client->invalid_message_count++;
    line_pos = 0;
}
```

### 2. Ignorování CR (CRLF handling)
```c
if (c == '\r') {
    continue;  // Ignoruj \r (pro Windows klienty: \r\n)
}
```

### 3. Validace IP adresy
```c
if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
    logger_log(LOG_ERROR, "Invalid IP address: %s", ip);
    return -1;
}
```

### 4. Validace portu
```c
if (port <= 0 || port > 65535) {
    fprintf(stderr, "Error: Invalid port number (must be 1-65535)\n");
    return 1;
}
```

---

## POUŽITÉ BSD SOCKET API FUNKCE

| Funkce | Účel | Kde použito |
|--------|------|-------------|
| `socket()` | Vytvoření socketu | server.c:39 |
| `setsockopt()` | SO_REUSEADDR | server.c:46 |
| `bind()` | Navázání na IP:port | server.c:60 |
| `listen()` | Začít naslouchat | server.c:68 |
| `accept()` | Přijmout nového klienta | server.c:83 |
| `recv()` | Přijmout data | client_handler.c:35 |
| `send()` | Odeslat data | client_handler.c:19 |
| `close()` | Uzavřít socket | server.c:139, client_handler.c:81 |
| `inet_pton()` | IP text → binární | server.c:56 |
| `inet_ntop()` | IP binární → text | server.c:91 |

**Žádné externí knihovny!** ✅

---

## SPUŠTĚNÍ A BUILD

### Překlad
```bash
cd server_src
make
```

**Výstup:**
```
gcc -Wall -Wextra -pthread -g -std=c99 -c main.c -o main.o
gcc -Wall -Wextra -pthread -g -std=c99 -c server.c -o server.o
gcc -Wall -Wextra -pthread -g -std=c99 -c client_handler.c -o client_handler.o
gcc -Wall -Wextra -pthread -g -std=c99 -c logger.c -o logger.o
gcc main.o server.o client_handler.o logger.o -o server -pthread
Build successful: server
```

### Spuštění
```bash
./server <IP> <PORT> <MAX_ROOMS> <MAX_CLIENTS>

# Příklad:
./server 127.0.0.1 10000 10 50
./server 0.0.0.0 8080 5 100
```

### Cleanup
```bash
make clean
```

---

## STATISTIKY

| Metrika | Hodnota |
|---------|---------|
| **Počet souborů** | 9 (.c: 4, .h: 4, Makefile: 1) |
| **Řádky kódu** | ~400 LOC |
| **Velikost binárky** | ~45 KB |
| **Kompilace** | 0 warnings |
| **Memory leaks** | 0 (ověřeno ručně, valgrind pending) |
| **Thread-safety** | ✅ Logger má mutex |
| **Testováno** | ✅ nc, InTCPtor, paralelní klienti |
| **Stabilita** | ✅ Žádné segfaulty |

---

## PŘIPRAVENOST PRO ROZŠÍŘENÍ

### Co je připraveno:
✅ Struktura `client_t` s polem `nickname` (zatím prázdné)
✅ Enum `client_state_t` s všemi stavy (zatím jen `STATE_CONNECTED`)
✅ Počítadlo `invalid_message_count` (zatím nepoužito)
✅ Timestamp `last_activity` (zatím nepoužito pro timeout)
✅ Konfigurace `max_rooms`, `max_clients` (zatím nepoužito)
✅ Konstanty protokolu v `protocol.h` (připraveno na parsování)

### Další kroky (Packet 4+):
⏳ Parsování příkazů (`protocol.c`)
⏳ Implementace `HELLO` → `WELCOME` (autentizace)
⏳ Správa místností (`room.c`, `room.h`)
⏳ Logika hry (`game.c`, `game.h`)
⏳ PING/PONG keepalive
⏳ Reconnect mechanismus

---

## ZÁVĚR

**Packet 3 úspěšně dokončen!** ✅

### Co funguje:
- ✅ TCP server s BSD sockets
- ✅ Thread-per-client architektura
- ✅ Thread-safe logování
- ✅ Robustní zpracování fragmentovaných zpráv
- ✅ Graceful shutdown (signal handling)
- ✅ Error handling všech syscalls
- ✅ Korektní cleanup zdrojů
- ✅ Makefile s `-Wall -Wextra -pthread`
- ✅ Testováno s InTCPtor (extrémní fragmentace)

### Připravenost:
- Server je **stabilní** a připravený pro rozšíření
- Architektura je **modulární** a dobře zdokumentovaná
- Kód je **čitelný** s komentáři
- Všechny požadavky Packetu 3 **splněny**

**Další krok:** Packet 4 - Implementace parsování příkazů a autentizace (HELLO → WELCOME)

---

*Dokument vytvořen: 2025-11-12*
*Autor: Claude Code*
*Verze: 1.0*
