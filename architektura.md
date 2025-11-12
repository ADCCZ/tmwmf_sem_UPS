
# NÁVRH ARCHITEKTURY PROJEKTU PEXESO
**Síťová hra Memory – architektura server–klient (1 : N)**

---

Tento dokument popisuje architekturu celého projektu na úrovni modulů, tříd a datových struktur.  
Slouží jako základ pro implementaci v dalších krocích.

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
- Správa hlavní event loop (`select`/`poll`/`epoll`)  
- Přijímání nových připojení  
- Volání `client_handler` pro zpracování příchozích zpráv  
- Správa timeoutů (PING/PONG mechanismus)  
- Koordinace mezi všemi klienty a místnostmi  

**Funkce**
```c
int  server_init(const char *ip, int port, int max_rooms, int max_clients);
void server_run(void);
void server_shutdown(void);
void server_accept_connection(int listen_fd);
void server_process_client(int client_fd);
void server_check_timeouts(void);
void server_broadcast_to_room(int room_id, const char *message);
void server_send_to_client(int client_fd, const char *message);
````

**Globální struktury**

* `server_context_t` – obsahuje `listen_fd`, `fdset`, seznam klientů, místností.

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

Nastavení serveru (IP, port, limity, timeouty, log soubor).

#### 3.6 `server_context_t`

Hlavní kontext běhu serveru – fd_set, seznam klientů a místností, čítače ID, flag `running`.

---

### 4. ROZHODNUTÍ O PARALELIZACI

**Zvoleno:** `select()` – single-threaded model

**Výhody**

* Jednoduchá synchronizace (vše v jednom vlákně)
* Žádné mutexy – nižší riziko deadlocků
* Nižší overhead a snadnější ladění

**Nevýhody vláken**

* Nutnost locků a komplexnější kód
* Riziko race conditions
* Zbytečné pro tahovou hru

**Architektura:**

* Hlavní smyčka se `select()` a timeoutem 1 s
* Reakce na nové spojení → `accept()`
* Aktivita na client_fd → `read()` → `client_handle_message()`
* Timeout → PING/PONG a detekce výpadků

---

### 5. FLOW DIAGRAMY

#### 5.1 Hlavní smyčka serveru

Sekvenční postup od `server_init()` po `server_shutdown()`
Zahrnuje select(), accept(), zpracování zpráv, PING/PONG a timeouty.

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
CFLAGS = -Wall -Wextra -pthread -g
LDFLAGS = -pthread

SOURCES = main.c server.c client_handler.c room.c game.c protocol.c logger.c config.c
OBJECTS = $(SOURCES:.c=.o)
TARGET  = pexeso_server

all: $(TARGET)
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean
```

### 6. Maven / Gradle (klient)

* Závislosti: `JavaFX`, `JUnit`
* Build: `mvn clean package` → `target/pexeso-client-1.0.jar`
* Spuštění: `java -jar target/pexeso-client-1.0.jar`

---

## SHRNUTÍ

### Architektura Serveru (C)

* Single-threaded (`select()` loop)
* Modularita (8 souborů)
* Datové struktury `client_t`, `room_t`, `game_t`
* Logování a PING/PONG keepalive

### Architektura Klienta (JavaFX)

* MVC – Model / View / Controller
* Asynchronní síťové vlákno
* `Platform.runLater()` pro bezpečné GUI
* Automatické reconnect a aktualizace stavu

### Klíčové vlastnosti

✅ Modularita a čitelnost
✅ Bezpečná paralelizace (select)
✅ Validace všech vstupů
✅ Robustní error handling
✅ Reconnect mechanismus
✅ Logování a debugging

---

*Dokument lze použít jako technickou dokumentaci nebo přímý podklad pro implementaci v dalších packetech.*

---