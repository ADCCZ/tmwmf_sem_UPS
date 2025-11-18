
# NÁVRH ARCHITEKTURY PROJEKTU PEXESO
**Síťová hra Memory – architektura server–klient (1 : N)**

---

Tento dokument popisuje architekturu celého projektu na úrovni modulů, tříd a datových struktur.
Slouží jako základ pro implementaci v dalších krocích.

**Poznámka k paralelizaci:** Původní návrh počítal s `select()` single-threaded modelem. Během implementace bylo rozhodnuto použít **POSIX threads (thread-per-client)** z důvodu jednodušší implementace a snadnějšího debuggingu. Obě metody jsou povoleny zadáním (PozadavkyUPS.pdf, strana 2).

**Stav implementace:** Aktuálně implementována kostra serveru (Packet 3) - `main.c`, `server.c`, `client_handler.c`, `logger.c`, `protocol.h`. Moduly `room.c`, `game.c`, `protocol.c` budou implementovány v dalších packetech.

---

## ČÁST I: SERVER (C)

### 1. PŘEHLED MODULŮ A SOUBORŮ

```

server/
├── main.c                  - vstupní bod serveru
├── server.h / server.c     - inicializace a hlavní smyčka serveru
├── client_handler.h / .c   - správa připojených klientů
├── room.h / room.c         - správa lobby a herních místností
├── game.h / game.c         - logika hry Pexeso
├── protocol.h / protocol.c - parsování a tvorba zpráv
├── logger.h / logger.c     - logování událostí
├── config.h / config.c     - načítání konfigurace
└── Makefile                - překlad projektu

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

#### 2.6 protocol.h / protocol.c

**Odpovědnosti**

* Parsování textových zpráv
* Validace formátu a parametrů
* Tvorba odpovědí dle specifikace protokolu
* Pomocné funkce pro každý typ zprávy

---

#### 2.7 logger.h / logger.c

**Odpovědnosti**

* Logování událostí do souboru s časem
* Úrovně logů (INFO, WARNING, ERROR)
* Thread-safe zápis

---

#### 2.8 config.h / config.c

**Odpovědnosti**

* Načtení konfigurace ze souboru
* Nastavení výchozích hodnot

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
* Místnosti budou mít vlastní synchronizační mechanismy (v budoucích verzích)

**Thread-safety**

* Logger: `pthread_mutex_lock()` / `unlock()` kolem zápisů
* Klienti: žádná sdílená data (každý má vlastní `client_t`)
* Místnosti: budou mít vlastní mutex pro přístup k hernímu stavu

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
client/
└── cz/zcu/kiv/ups/pexeso/
    ├── Main.java
    ├── network/
    │   ├── ClientConnection.java
    │   └── MessageListener.java
    ├── protocol/
    │   ├── MessageParser.java
    │   ├── MessageBuilder.java
    │   └── ProtocolConstants.java
    ├── model/
    │   ├── GameState.java
    │   ├── Room.java
    │   ├── Player.java
    │   └── Card.java
    ├── controller/
    │   ├── LoginController.java
    │   ├── LobbyController.java
    │   └── GameController.java
    └── ui/
        ├── LoginView.fxml
        ├── LobbyView.fxml
        └── GameView.fxml
```

---

### 2. DETAILNÍ POPIS HLAVNÍCH TŘÍD

#### 2.1 Main.java

* Vstupní bod (JavaFX Application)
* Správa scén (Login, Lobby, Game)

#### 2.2 ClientConnection.java

* TCP spojení, asynchronní čtení (zvláštní vlákno)
* Odesílání zpráv, automatické reconnecty, PING/PONG

#### 2.3 MessageParser / MessageBuilder

* Rozklad a tvorba zpráv dle specifikace protokolu

#### 2.4 Model tříd (GameState, Room, Player, Card)

* Udržují stav hry, místností a hráčů
* GameState = „single source of truth“

#### 2.5 Kontrolery (Login/Lobby/Game)

* Obsluha GUI událostí a reagování na zprávy od serveru
* Implementují rozhraní `MessageListener`
* **Všechny kontrolery** (LoginController, LobbyController, GameController) musí zpracovávat PING zprávy a odpovídat PONG
* Při odpojení (onDisconnected) se klient vrací na přihlašovací obrazovku

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

* Unit testy `protocol.c`, `game.c`
* Integrační testy s více klienty
* Výpadky testovány přes iptables DROP/REJECT
* Valgrind → memory leaks

### 3. Rozšiřitelnost

* Modulární kód, možnost přidat chat, statistiky, žebříčky

### 4. Dokumentace kódu

* Komentáře ve všech `.h` a JavaDoc v Javě

### 5. Makefile (server)

```make
CC = gcc
CFLAGS = -Wall -Wextra -pthread -g -std=c99
LDFLAGS = -pthread

# Aktuálně implementované soubory
SOURCES = main.c server.c client_handler.c logger.c
# V budoucnu: + room.c game.c protocol.c config.c
OBJECTS = $(SOURCES:.c=.o)
TARGET  = server

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET) server.log
	@echo "Clean complete"

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

### 6. Maven / Gradle (klient)

* Závislosti: `JavaFX`, `JUnit`
* Build: `mvn clean package` → `target/pexeso-client-1.0.jar`
* Spuštění: `java -jar target/pexeso-client-1.0.jar`

---

## SHRNUTÍ

### Architektura Serveru (C)

* Multi-threaded (POSIX threads, thread-per-client)
* Modularita (8 modulů plánováno, 5 implementováno)
* Datové struktury `client_t`, `server_config_t`, (+ `room_t`, `game_t` v budoucnu)
* Thread-safe logování s mutex
* Robustní zpracování fragmentovaných zpráv

### Architektura Klienta (JavaFX)

* MVC – Model / View / Controller
* Asynchronní síťové vlákno
* `Platform.runLater()` pro bezpečné GUI
* Automatické reconnect a aktualizace stavu

### Klíčové vlastnosti

✅ Modularita a čitelnost
✅ Bezpečná paralelizace (POSIX threads)
✅ Thread-safe operace (mutex v loggeru)
✅ Validace všech vstupů (plánováno)
✅ Robustní error handling
✅ Reconnect mechanismus (plánováno)
✅ Logování a debugging
✅ Zpracování fragmentovaných zpráv (testováno s InTCPtor)

---

*Dokument lze použít jako technickou dokumentaci nebo přímý podklad pro implementaci v dalších packetech.*

---