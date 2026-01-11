
# NÁVRH ARCHITEKTURY PROJEKTU PEXESO
**Síťová hra Memory – architektura server–klient (1 : N)**

---

Tento dokument popisuje architekturu celého projektu na úrovni modulů, tříd a datových struktur.
Slouží jako základ pro implementaci v dalších krocích.

**Poznámka k paralelizaci:** Původní návrh počítal s `select()` single-threaded modelem. Během implementace bylo rozhodnuto použít **POSIX threads (thread-per-client)** z důvodu jednodušší implementace a snadnějšího debuggingu. Obě metody jsou povoleny zadáním (PozadavkyUPS.pdf, strana 2).

**Stav implementace:** Projekt je **kompletně implementován** včetně všech modulů serveru i klienta, reconnect mechanismu, loggování a GUI.

---

## ČÁST I: SERVER (C)

### 1. PŘEHLED MODULŮ A SOUBORŮ

```

server_src/
├── main.c                     - vstupní bod serveru
├── server.h / server.c        - inicializace a hlavní smyčka serveru
├── client_handler.h / .c      - obsluha klientských spojení a zpráv
├── client_list.h / client_list.c - správa seznamu připojených klientů
├── room.h / room.c            - správa lobby a herních místností
├── game.h / game.c            - logika hry Pexeso
├── protocol.h                 - definice protokolu a konstant
├── logger.h / logger.c        - logování událostí do souboru
└── Makefile                   - překlad projektu

````

---

### 2. DETAILNÍ POPIS MODULŮ

#### 2.1 main.c
**Odpovědnosti**
- Parsování příkazové řádky (IP adresa, port, max_rooms, max_clients)
- Alternativně načtení z konfiguračního souboru
- Inicializace serveru → `server_init`
- Spuštění hlavní smyčky → `server_run`
- Ukončení a úklid → `server_shutdown`

**Funkce**
- `int main(int argc, char *argv[])`
- `void print_usage(const char *program_name)`
- `void handle_signal(int sig)` – pro SIGINT/SIGTERM

---

#### 2.2 server.h / server.c
**Odpovědnosti**
- Vytvoření a nastavení listening socketu
- Správa hlavní accept loop (přijímání nových spojení)
- Vytváření nových threadů pro každého klienta (`pthread_create`)
- Koordinace mezi všemi klienty a místnostmi
- Správa konfigurace serveru (IP, port, limity)

**Funkce**
```c
int  server_init(const char *ip, int port, int max_rooms, int max_clients);
void server_run(void);
void server_shutdown(void);
server_config_t* server_get_config(void);
````

**Globální struktury**

* `server_config_t` – obsahuje `listen_fd`, `ip`, `port`, `max_rooms`, `max_clients`, `running`, `next_client_id`

---

#### 2.3 client_handler.h / client_handler.c

**Odpovědnosti**

* Správa stavu jednotlivého klienta (socket, přezdívka, stav)
* Čtení a validace příchozích zpráv
* Volání příslušných handlerů dle typu příkazu
* Počítadlo neplatných zpráv (odpojení po 3 chybách)
* Detekce odpojení a timeoutů (SHORT/LONG disconnect)
* Správa PING/PONG keepalive

**Klíčové funkce a handlery**

* `client_t* client_create(int socket_fd)`
* `void client_handle_message(client_t *client, const char *message)`
* `void handle_hello(...)`, `handle_flip(...)`, `handle_reconnect(...)` atd.

---

#### 2.4 room.h / room.c

**Odpovědnosti**

* Správa lobby (seznam místností)
* Vytváření a rušení místností
* Přidávání / odebírání hráčů
* Zahájení hry → inicializace `game_t`
* Broadcast zpráv všem hráčům
* Ošetření výpadků hráčů

**Hlavní funkce**

* `room_t* room_create(...)`
* `int room_add_player(...)`
* `int room_start_game(...)`
* `void room_broadcast(...)`
* `char* room_get_list_message(void)` – vytváří `ROOM_LIST`

---

#### 2.5 game.h / game.c

**Odpovědnosti**

* Implementace logiky Pexesa
* Inicializace herní desky (náhodné rozmístění karet)
* Zpracování tahů, vyhodnocení shody / neshody
* Správa pořadí hráčů a skóre
* Detekce konce hry
* Generování `GAME_STATE` pro reconnect

**Funkce**

* `game_t* game_create(int board_size, client_t **players, int player_count)`
* `int game_flip_card(game_t *game, client_t *player, int card_id)`
* `int game_check_match(game_t *game)`
* `void game_next_turn(game_t *game)`
* `int game_is_finished(game_t *game)`
* `char* game_get_state_message(game_t *game)`

---

#### 2.6 protocol.h

**Odpovědnosti**

* Definice protokolových konstant a příkazů
* Definice error kódů
* Definice maximálních délek zpráv a nicků
* Konstanty pro timeouty (PING/PONG interval, RECONNECT timeout)

---

#### 2.7 logger.h / logger.c

**Odpovědnosti**

* Logování událostí do souboru `server.log` s časem
* Úrovně logů (INFO, WARNING, ERROR)
* Thread-safe zápis pomocí mutex
* Truncate při startu serveru

---

### 3. DATOVÉ STRUKTURY

#### 3.1 `client_t`

Reprezentuje jednoho hráče, včetně socketu, stavu a časových údajů.
Obsahuje např. `socket_fd`, `nickname`, `state`, `last_activity`, `invalid_message_count`.

#### 3.2 `room_t`

Struktura herní místnosti, s polemi `players`, `owner`, `state`, `game`.

#### 3.3 `game_t`

Struktura reprezentující stav hry – karty, skóre, hráče, pořadí tahů.

#### 3.4 `message_t`

Výsledek parsování textové zprávy – typ příkazu a parametry.

#### 3.5 `server_config_t`

Hlavní konfigurace a stav běhu serveru:
```c
typedef struct {
    char ip[64];          // IP adresa pro bind
    int port;             // Port pro listening
    int max_rooms;        // Maximální počet místností
    int max_clients;      // Maximální počet klientů
    int listen_fd;        // File descriptor listening socketu
    int running;          // Flag: 1 = server běží, 0 = ukončit
    int next_client_id;   // Čítač pro unikátní ID klientů
} server_config_t;
```

---

### 4. ROZHODNUTÍ O PARALELIZACI

**Zvoleno:** POSIX threads (pthread) – thread-per-client model

**Důvody pro vlákna**

* Jednoduchá implementace – každý klient má vlastní kontext
* Blokující I/O operace jsou přijatelné (neblokují ostatní klienty)
* Přirozená izolace mezi klienty (vlastní stack a lokální proměnné)
* Vhodné pro desítky klientů (max 50 dle konfigurace serveru)
* Žádná sdílená data mezi klienty – každá místnost bude izolovaná
* Snadnější debugging – každý thread je nezávislý

**Implementace**

* Hlavní thread běží v `server_run()` – nekonečná accept loop
* Pro každého nového klienta:
  * Vytvoří se `client_t` struktura (alokace dynamické paměti)
  * Spustí se nový thread: `pthread_create(&tid, NULL, client_handler_thread, client)`
  * Thread se detachuje: `pthread_detach(tid)` → automatický cleanup po ukončení
* Logger používá mutex (`pthread_mutex_t`) pro thread-safe zápis
* Room broadcast používá synchronizační mechanismy pro bezpečné odesílání zpráv všem hráčům

**Thread-safety**

* Logger: `pthread_mutex_lock()` / `unlock()` kolem zápisů
* Klienti: žádná sdílená data (každý má vlastní `client_t`)
* Místnosti: broadcast operace jsou thread-safe, mutex pro přístup k hernímu stavu

---

### 5. FLOW DIAGRAMY

#### 5.1 Hlavní smyčka serveru

Sekvenční postup od `server_init()` po `server_shutdown()`:
1. `server_init()` – vytvoření socketu, bind, listen
2. `server_run()` – nekonečná smyčka s `accept()`
3. Pro každého klienta → `pthread_create()` → `client_handler_thread()`
4. Signal (SIGINT/SIGTERM) → `server_config.running = 0` → ukončení loop
5. `server_shutdown()` – uzavření listening socketu

#### 5.2 Zpracování příkazu `FLIP`

Popis průběhu tahu: ověření, uložení otočené karty, vyhodnocení shody, broadcast.

#### 5.3 Zpracování výpadku hráče

Mechanismus PING/PONG, detekce SHORT/LONG disconnect a RECONNECT.

---

## ČÁST II: KLIENT (JAVA + JavaFX)

### 1. STRUKTURA BALÍČKŮ A TŘÍD

```
client_src/src/main/java/cz/zcu/kiv/ups/pexeso/
├── Main.java
├── network/
│   ├── ClientConnection.java      - TCP spojení, auto-reconnect
│   └── MessageListener.java       - rozhraní pro příjem zpráv
├── protocol/
│   └── ProtocolConstants.java     - konstanty protokolu a timeouty
├── model/
│   └── Room.java                  - model herní místnosti
├── controller/
│   ├── LoginController.java       - ovládání přihlašovací obrazovky
│   ├── LobbyController.java       - ovládání lobby (seznam místností)
│   └── GameController.java        - ovládání herní obrazovky
├── util/
│   └── Logger.java                - logování do client.log
└── ui/
    ├── LoginView.fxml             - layout přihlášení
    ├── LobbyView.fxml             - layout lobby
    └── GameView.fxml              - layout hry
```

---

### 2. DETAILNÍ POPIS HLAVNÍCH TŘÍD

#### 2.1 Main.java

* Vstupní bod (JavaFX Application)
* Inicializace loggeru
* Správa scén (Login, Lobby, Game)
* Cleanup při ukončení aplikace

#### 2.2 ClientConnection.java

* TCP spojení přes BSD sockets (java.net.Socket)
* Asynchronní čtení v samostatném vlákně
* Odesílání zpráv, automatické reconnecty (7 pokusů × 10s)
* PING/PONG keepalive mechanismus
* Timeout detekce (READ_TIMEOUT 15s)

#### 2.3 Logger.java (util/)

* **Nová komponenta pro splnění požadavku zadání**
* Logování do souboru `client.log` (truncate při startu)
* Thread-safe zápis (synchronized)
* Úrovně: INFO, WARN, ERROR
* Loguje: připojení, odpojení, reconnect, chyby, stavy

#### 2.4 ProtocolConstants.java

* Definice konstant protokolu (příkazy, error kódy)
* Timeouty (CONNECTION_TIMEOUT, READ_TIMEOUT, RECONNECT_INTERVAL, MAX_RECONNECT_ATTEMPTS)

#### 2.5 Model tříd (Room)

* Udržují stav místností
* Stav hry je plně řízen serverem (klient pouze zobrazuje)

#### 2.6 Kontrolery (Login/Lobby/Game)

* Obsluha GUI událostí a reagování na zprávy od serveru
* Implementují rozhraní `MessageListener`
* **Všechny kontrolery** musí zpracovávat PING zprávy a odpovídat PONG
* Při odpojení (onDisconnected) se klient vrací na přihlašovací obrazovku
* Využívají Logger pro zaznamenávání důležitých událostí

---

### 3. ZABEZPEČENÍ NEZAMRZNUTÍ GUI

**Princip:**

* Síťové čtení probíhá v odděleném threadu
* GUI aktualizace pomocí `Platform.runLater()`
* Komunikace → event-driven model

**Schéma:**

```
[GUI Thread] → sendMessage() → socket
[Reader Thread] → readLine() → Platform.runLater() → onMessageReceived()
```

---

### 4. ZÁRUKA AKTUÁLNÍHO STAVU HRY

* Model `GameState` uchovává kompletní stav hry
* Každá zpráva od serveru aktualizuje GameState a volá `updateUI()`
* Server je autorita – klient nevyhodnocuje shody

---

## ČÁST III: DODATEČNÉ POZNÁMKY

### 1. Bezpečnost a validace

* Server validuje všechny vstupy
* Klient validuje uživatelské vstupy (IP, port, nick)
* Po 3 chybách → disconnect
* Logování všech podezřelých událostí

### 2. Testování

* Kompletní hra bez chyb (2-4 hráči)
* Výpadky testovány přes iptables DROP/REJECT
* Nevalidní zprávy (cat /dev/urandom | nc)
* Valgrind pro memory leaks (žádné leaky detekované)
* Paralelní místnosti
* Reconnect mechanismus (odpojení a znovupřipojení)

### 3. Rozšiřitelnost

* Modulární kód, možnost přidat chat, statistiky, žebříčky
* Protocol podporuje budoucí příkazy

### 4. Dokumentace kódu

* Komentáře v `.h` souborech serveru
* JavaDoc komentáře v klíčových třídách klienta
* Technická dokumentace: `protokol_pexeso.md`, `architektura.md`

### 5. Makefile (server)

```make
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g
SOURCES = main.c server.c client_handler.c client_list.c logger.c room.c game.c
TARGET = server

all:
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean
```

**Kompilace:**
```bash
make              # Zkompiluje server
make clean        # Smaže binárku
```

**Spuštění:**
```bash
./server <IP> <PORT> <MAX_ROOMS> <MAX_CLIENTS>
./server 0.0.0.0 10000 10 50
```

### 6. Maven / Gradle (klient)

* Závislosti: `JavaFX`, `JUnit`
* Build: `mvn clean package` → `target/pexeso-client-1.0.jar`
* Spuštění: `java -jar target/pexeso-client-1.0.jar`

---

## SHRNUTÍ

### Architektura Serveru (C)

* **Multi-threaded:** POSIX threads (thread-per-client model)
* **Modularita:** 6 hlavních modulů (main, server, client_handler, client_list, room, game, logger) + protocol.h
* **Datové struktury:** `client_t`, `room_t`, `game_t`, `server_config_t`
* **Thread-safe:** Mutex v loggeru, room broadcast operacích
* **Robustní:** Zpracování fragmentovaných zpráv, validace vstupů
* **Reconnect:** 90s timeout pro čekání na klienta (klient má 70s na reconnect)
* **Logger:** Logování do `server.log` (truncate při startu)

### Architektura Klienta (JavaFX)

* **MVC:** Model / View / Controller
* **Asynchronní:** Síťové vlákno oddělené od GUI
* **Bezpečné GUI:** `Platform.runLater()` pro všechny UI aktualizace
* **Automatický reconnect:** 7 pokusů po 10 sekundách (70s celkem)
* **Logger:** Vlastní logger do `client.log` (nová komponenta)
* **Pouze Java standard library:** java.net.Socket, bez externích knihoven

### Klíčové vlastnosti

✅ Modularita a čitelnost kódu
✅ Bezpečná paralelizace (POSIX threads)
✅ Thread-safe operace (mutexy v loggeru a room broadcast)
✅ Validace všech vstupů (server i klient)
✅ Robustní error handling (3 chyby → disconnect)
✅ **Reconnect mechanismus kompletně implementován**
✅ **Logování na serveru i klientovi**
✅ Zpracování fragmentovaných zpráv
✅ PING/PONG keepalive
✅ Podpora 2-4 hráčů, více paralelních místností
✅ **Žádné memory leaky (ověřeno valgrind)**

---

*Dokument lze použít jako technickou dokumentaci nebo přímý podklad pro implementaci v dalších packetech.*

---