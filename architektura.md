================================================================================
                    NÁVRH ARCHITEKTURY PROJEKTU PEXESO
           Síťová hra Memory - architektura server-klient (1:N)
================================================================================

Tento dokument popisuje architekturu celého projektu na úrovni modulů, tříd
a datových struktur. Slouží jako základ pro implementaci v dalších krocích.

================================================================================
ČÁST I: SERVER (C)
================================================================================

1. PŘEHLED MODULŮ A SOUBORŮ
--------------------------------------------------------------------------------

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


2. DETAILNÍ POPIS MODULŮ
--------------------------------------------------------------------------------

2.1 main.c
----------
Odpovědnosti:
  - Parsování příkazové řádky (IP adresa, port, max_rooms, max_clients)
  - Alternativně načtení z konfiguračního souboru
  - Inicializace serveru (volání server_init)
  - Spuštění hlavní smyčky (volání server_run)
  - Ukončení a úklid (volání server_shutdown)

Funkce:
  - int main(int argc, char *argv[])
  - void print_usage(const char *program_name)
  - void handle_signal(int sig) - pro SIGINT/SIGTERM


2.2 server.h / server.c
-----------------------
Odpovědnosti:
  - Vytvoření a nastavení listening socketu
  - Správa hlavní event loop (select/poll/epoll)
  - Přijímání nových připojení
  - Volání client_handler pro zpracování příchozích zpráv
  - Správa timeoutů (PING/PONG mechanismus)
  - Koordinace mezi všemi klienty a místnostmi

Funkce:
  - int server_init(const char *ip, int port, int max_rooms, int max_clients)
  - void server_run(void)
  - void server_shutdown(void)
  - void server_accept_connection(int listen_fd)
  - void server_process_client(int client_fd)
  - void server_check_timeouts(void)
  - void server_broadcast_to_room(int room_id, const char *message)
  - void server_send_to_client(int client_fd, const char *message)

Globální struktury:
  - server_context_t (obsahuje listen_fd, fdset, seznam klientů, seznam místností)


2.3 client_handler.h / client_handler.c
----------------------------------------
Odpovědnosti:
  - Správa stavu jednotlivého klienta (socket, přezdívka, stav)
  - Čtení a validace příchozích zpráv od klienta
  - Volání příslušných handlerů dle typu příkazu
  - Sledování počtu neplatných zpráv (odpojení po 3 chybách)
  - Detekce odpojení a timeout (SHORT/LONG disconnect)
  - Správa PING/PONG keepalive

Funkce:
  - client_t* client_create(int socket_fd)
  - void client_destroy(client_t *client)
  - void client_handle_message(client_t *client, const char *message)
  - void client_disconnect(client_t *client, disconnect_type_t type)
  - int client_validate_message(client_t *client, const char *message)
  - void client_send_ping(client_t *client)
  - void client_handle_pong(client_t *client)
  - void client_handle_timeout(client_t *client)

Handlery pro jednotlivé příkazy:
  - void handle_hello(client_t *client, const char *nickname)
  - void handle_list_rooms(client_t *client)
  - void handle_create_room(client_t *client, const char *name, int max_players, int board_size)
  - void handle_join_room(client_t *client, int room_id)
  - void handle_leave_room(client_t *client)
  - void handle_start_game(client_t *client)
  - void handle_flip(client_t *client, int card_id)
  - void handle_reconnect(client_t *client, const char *nickname)
  - void handle_disconnect(client_t *client)


2.4 room.h / room.c
-------------------
Odpovědnosti:
  - Správa lobby (seznam místností)
  - Vytváření a rušení místností
  - Přidávání a odebírání hráčů z místností
  - Zahájení hry (kontrola podmínek, inicializace game_t)
  - Broadcast zpráv všem hráčům v místnosti
  - Ošetření výpadků hráčů (pozastavení/ukončení hry)

Funkce:
  - void room_manager_init(int max_rooms)
  - room_t* room_create(const char *name, int max_players, int board_size, client_t *owner)
  - void room_destroy(room_t *room)
  - room_t* room_find_by_id(int room_id)
  - int room_add_player(room_t *room, client_t *client)
  - void room_remove_player(room_t *room, client_t *client)
  - int room_start_game(room_t *room, client_t *requester)
  - void room_end_game(room_t *room, const char *winner)
  - void room_broadcast(room_t *room, const char *message, client_t *exclude)
  - void room_handle_player_disconnect(room_t *room, client_t *client, disconnect_type_t type)
  - char* room_get_list_message(void) - vytvoří ROOM_LIST zprávu


2.5 game.h / game.c
-------------------
Odpovědnosti:
  - Implementace logiky hry Pexeso
  - Inicializace herní desky (náhodné rozmístění karet)
  - Zpracování tahů (otočení karet)
  - Vyhodnocení shody/neshody karet
  - Správa pořadí hráčů a skóre
  - Detekce konce hry (všechny páry nalezeny)
  - Generování GAME_STATE pro reconnect

Funkce:
  - game_t* game_create(int board_size, client_t **players, int player_count)
  - void game_destroy(game_t *game)
  - void game_init_board(game_t *game) - náhodně zamíchá karty
  - int game_flip_card(game_t *game, client_t *player, int card_id)
  - int game_check_match(game_t *game) - kontrola po otočení 2. karty
  - void game_next_turn(game_t *game)
  - int game_is_finished(game_t *game)
  - client_t* game_get_winner(game_t *game)
  - char* game_get_state_message(game_t *game) - pro GAME_STATE


2.6 protocol.h / protocol.c
----------------------------
Odpovědnosti:
  - Parsování příchozích textových zpráv
  - Validace formátu a parametrů zpráv
  - Tvorba odchozích zpráv (dle specifikace protokolu)
  - Pomocné funkce pro jednotlivé typy zpráv

Funkce:
  - int protocol_parse_message(const char *raw_message, message_t *parsed)
  - int protocol_validate_message(const message_t *msg, client_state_t client_state)

Funkce pro tvorbu odpovědí (každá vrací dynamicky alokovaný string):
  - char* protocol_make_welcome(int client_id)
  - char* protocol_make_error(int error_code, const char *message)
  - char* protocol_make_room_list(room_t **rooms, int count)
  - char* protocol_make_room_created(int room_id)
  - char* protocol_make_room_joined(int room_id, client_t **players, int count)
  - char* protocol_make_game_start(int board_size, client_t **players, int count)
  - char* protocol_make_card_reveal(int card_id, int card_value, const char *nickname)
  - char* protocol_make_match(const char *nickname, int score)
  - char* protocol_make_mismatch(void)
  - char* protocol_make_turn(const char *nickname)
  - char* protocol_make_your_turn(void)
  - char* protocol_make_game_end(const char *winner, client_t **players, int *scores, int count)
  - char* protocol_make_ping(void)
  - char* protocol_make_player_disconnected(const char *nickname, disconnect_type_t type)
  - char* protocol_make_player_reconnected(const char *nickname)
  - char* protocol_make_game_state(game_t *game)

Pomocné funkce:
  - void protocol_free_message(char *msg)


2.7 logger.h / logger.c
-----------------------
Odpovědnosti:
  - Logování událostí do souboru
  - Timestamp pro každý záznam
  - Různé úrovně logování (INFO, WARNING, ERROR)
  - Thread-safe zápis (pokud použijeme vlákna)

Funkce:
  - int logger_init(const char *log_file)
  - void logger_close(void)
  - void logger_log(log_level_t level, const char *format, ...)
  - void logger_log_connection(const char *ip, int port, const char *nickname)
  - void logger_log_disconnection(const char *nickname, disconnect_type_t type)
  - void logger_log_error(const char *nickname, const char *error_message)
  - void logger_log_game_event(int room_id, const char *event)


2.8 config.h / config.c
-----------------------
Odpovědnosti:
  - Načtení konfigurace ze souboru (volitelná funkcionalita)
  - Výchozí hodnoty, pokud není specifikováno

Funkce:
  - int config_load(const char *config_file, server_config_t *config)
  - void config_set_defaults(server_config_t *config)


3. DATOVÉ STRUKTURY
--------------------------------------------------------------------------------

3.1 Klient (client_t)
---------------------
typedef enum {
    STATE_CONNECTED,        // TCP spojení, čeká na HELLO
    STATE_AUTHENTICATED,    // Identifikován, v lobby
    STATE_IN_ROOM,          // V místnosti, čeká na start
    STATE_IN_GAME,          // Hraje hru
    STATE_DISCONNECTED_SHORT, // Krátkodobý výpadek
    STATE_DISCONNECTED_LONG   // Dlouhodobý výpadek (vyhozen)
} client_state_t;

typedef struct client_s {
    int socket_fd;
    int client_id;              // Unikátní ID přidělené serverem
    char nickname[17];          // Přezdívka (max 16 znaků + \0)
    client_state_t state;

    struct room_s *current_room; // Pointer na místnost (NULL = v lobby)

    char read_buffer[4096];     // Buffer pro příchozí data
    int buffer_pos;             // Pozice v bufferu

    time_t last_activity;       // Timestamp poslední aktivity (pro timeout)
    time_t ping_sent;           // Timestamp odeslaného PING
    int invalid_message_count;  // Počítadlo neplatných zpráv

    struct client_s *next;      // Linked list další klientů
} client_t;


3.2 Místnost (room_t)
---------------------
typedef enum {
    ROOM_WAITING,    // Čeká na hráče
    ROOM_PLAYING     // Probíhá hra
} room_state_t;

typedef struct room_s {
    int room_id;
    char name[21];              // Název místnosti (max 20 + \0)
    room_state_t state;

    int max_players;            // Maximální počet hráčů (2-4)
    int current_player_count;   // Aktuální počet hráčů
    client_t *players[4];       // Pole pointerů na hráče
    client_t *owner;            // Tvůrce místnosti (může spustit hru)

    struct game_s *game;        // Pointer na aktuální hru (NULL = nehraje se)

    struct room_s *next;        // Linked list další místností
} room_t;


3.3 Hra Pexeso (game_t)
-----------------------
typedef struct {
    int card_id;
    int card_value;    // Hodnota symbolu (0 až board_size/2 - 1)
    int revealed;      // 1 = karta je trvale odhalená (pár nalezen), 0 = skrytá
} card_t;

typedef struct game_s {
    int board_size;           // Celkový počet karet (16, 24, 32, 36)
    card_t *board;            // Dynamicky alokované pole karet

    int player_count;
    client_t **players;       // Pole pointerů na hráče
    int *scores;              // Pole skóre pro každého hráče

    int current_turn;         // Index hráče, který je na tahu (0..player_count-1)
    int flipped_count;        // Počet otočených karet v aktuálním tahu (0, 1, 2)
    int flipped_cards[2];     // ID otočených karet v aktuálním tahu

    int pairs_found;          // Počet nalezených párů
    int total_pairs;          // Celkový počet párů (board_size / 2)
} game_t;


3.4 Zpráva (message_t)
----------------------
typedef enum {
    CMD_HELLO,
    CMD_LIST_ROOMS,
    CMD_CREATE_ROOM,
    CMD_JOIN_ROOM,
    CMD_LEAVE_ROOM,
    CMD_START_GAME,
    CMD_FLIP,
    CMD_PONG,
    CMD_DISCONNECT,
    CMD_RECONNECT,
    CMD_UNKNOWN
} command_type_t;

typedef struct {
    command_type_t type;
    char params[10][256];  // Max 10 parametrů, každý max 256 znaků
    int param_count;
} message_t;


3.5 Konfigurace serveru (server_config_t)
------------------------------------------
typedef struct {
    char ip[16];          // IP adresa (např. "127.0.0.1")
    int port;             // Port (např. 10000)
    int max_rooms;        // Max počet místností
    int max_clients;      // Max počet klientů celkem
    int ping_interval;    // Interval PING (sekundy, výchozí 15)
    int pong_timeout;     // Timeout pro PONG (sekundy, výchozí 5)
    int disconnect_timeout; // Timeout pro SHORT->LONG (sekundy, výchozí 60)
    char log_file[256];   // Cesta k log souboru
} server_config_t;


3.6 Kontext serveru (server_context_t)
---------------------------------------
typedef struct {
    int listen_fd;            // Listening socket
    fd_set master_fds;        // Master set pro select()
    int max_fd;               // Nejvyšší FD pro select()

    server_config_t config;

    client_t *clients_head;   // Linked list všech klientů
    room_t *rooms_head;       // Linked list všech místností

    int next_client_id;       // Počítadlo pro přidělování client_id
    int next_room_id;         // Počítadlo pro přidělování room_id

    int running;              // Flag pro ukončení hlavní smyčky
} server_context_t;


4. ROZHODNUTÍ O PARALELIZACI
--------------------------------------------------------------------------------

DOPORUČENÍ: select() s single-threaded architekturou

ZDŮVODNĚNÍ:

PRO select():
  + Jednodušší synchronizace - veškerý kód běží v jednom vlákně
  + Není potřeba mutex/lock pro sdílená data (clients_head, rooms_head)
  + Nižší overhead než vlákna
  + Snazší debugging (lineární průběh)
  + Dostatečný výkon pro herní aplikaci (není potřeba heavy computation)

PROTI vláknům (thread per client):
  - Nutnost lockovat všechna sdílená data (místnosti, klienti, hry)
  - Riziko deadlocků, race conditions
  - Vyšší složitost kódu
  - Overkill pro tahovou hru (není potřeba paralelní zpracování)

ARCHITEKTURA S select():

1. Hlavní smyčka (server_run):
   - Inicializace fd_set (FD_ZERO, FD_SET)
   - select() s timeoutem (např. 1 sekunda)
   - Po návratu ze select():
     a) Nové spojení na listen_fd → accept() → client_create()
     b) Data na client_fd → read() → client_handle_message()
     c) Timeout → server_check_timeouts() → PING, detekce výpadků

2. Neblokující operace:
   - Všechny sockety nastaveny jako non-blocking (fcntl O_NONBLOCK)
   - read() čte do bufferu, hledá '\n'
   - Pokud kompletní zpráva → zpracování
   - send() odesílá odpověď (pokud EAGAIN, buffer do fronty)

3. Výhody tohoto přístupu:
   - Jeden select() obsluhuje všechny sockety
   - Zpracování zpráv je sekvenční → konzistentní stav
   - Timeout v select() umožňuje pravidelné kontroly (PING)

ALTERNATIVA (pokud by byla potřeba):
  - Hybridní model: hlavní vlákno pro select(), worker pool pro těžké úlohy
  - Pro Pexeso není potřeba (žádné náročné výpočty)


5. FLOW DIAGRAMY
--------------------------------------------------------------------------------

5.1 Hlavní smyčka serveru (select() loop)
------------------------------------------

  [START]
     |
     v
  server_init() - vytvoř listen socket, bind, listen
     |
     v
  [HLAVNÍ SMYČKA - while(running)]
     |
     v
  FD_ZERO, přidej listen_fd a všechny client_fd do fd_set
     |
     v
  select() s timeout 1s
     |
     +------> [TIMEOUT] -> server_check_timeouts()
     |                      - odeslat PING klientům
     |                      - kontrola PONG timeout (SHORT disconnect)
     |                      - kontrola SHORT disconnect timeout (LONG disconnect)
     |
     +------> [ACTIVITY NA listen_fd] -> accept()
     |                                   -> client_create()
     |                                   -> přidat do seznamu klientů
     |
     +------> [ACTIVITY NA client_fd] -> read()
                                         -> protocol_parse_message()
                                         -> client_handle_message()
                                            |
                                            +-> handle_hello()
                                            +-> handle_list_rooms()
                                            +-> handle_create_room()
                                            +-> handle_join_room()
                                            +-> handle_flip()
                                            +-> atd.
                                         |
                                         +-> odeslat odpověď
                                         |
                                         +-> pokud disconnect -> client_destroy()
     |
     v
  [KONEC SMYČKY]
     |
     v
  server_shutdown() - uzavřít sockety, uvolnit paměť
     |
     v
  [END]


5.2 Zpracování FLIP příkazu (příklad průběhu hry)
--------------------------------------------------

  client pošle: FLIP 10
     |
     v
  protocol_parse_message() -> CMD_FLIP, params[0]="10"
     |
     v
  client_handle_message() -> handle_flip(client, 10)
     |
     v
  Kontroly:
    - je client ve stavu IN_GAME? ANO
    - je na tahu? ANO
    - je card_id platné (0-board_size)? ANO
    - není karta už odhalená? ANO
     |
     v
  game_flip_card(game, client, 10)
     |
     v
  game->flipped_count == 1? (první karta v tahu)
     |
     +----> ANO: uložit kartu, flipped_count=1
     |           broadcast: CARD_REVEAL 10 7 Alice
     |           čekat na druhou kartu
     |
     +----> NE (druhá karta):
             uložit kartu, flipped_count=2
             broadcast: CARD_REVEAL 14 7 Alice
             |
             v
            game_check_match()
             |
             +----> MATCH (hodnoty stejné):
             |       - player->score++
             |       - game->pairs_found++
             |       - karty zůstanou revealed=1
             |       - broadcast: MATCH Alice 5
             |       - stejný hráč zůstává na tahu
             |       - flipped_count=0
             |       |
             |       +-> pairs_found == total_pairs?
             |            |
             |            +-> ANO: hra končí
             |                 broadcast: GAME_END Alice ...
             |                 všichni hráči -> IN_LOBBY
             |
             +----> MISMATCH (hodnoty různé):
                     - karty vrátit na revealed=0
                     - broadcast: MISMATCH
                     - game_next_turn() -> další hráč
                     - broadcast: TURN Bob / YOUR_TURN
                     - flipped_count=0


5.3 Zpracování výpadku hráče
-----------------------------

  [PING/PONG mechanismus]
     |
     v
  server_check_timeouts() - každou sekundu
     |
     v
  Pro každého klienta:
    - time(NULL) - last_activity > 15s ?
       |
       +-> ANO: odeslat PING
                ping_sent = time(NULL)
     |
    - ping_sent != 0 && time(NULL) - ping_sent > 5s ?
       |
       +-> ANO: SHORT DISCONNECT
                client->state = STATE_DISCONNECTED_SHORT
                |
                +-> pokud je ve hře:
                    - broadcast: PLAYER_DISCONNECTED Alice SHORT
                    - game->paused = 1 (hra se pozastaví)
                    - nastavit timeout (60s)
     |
    - state == SHORT && time(NULL) - ping_sent > 60s ?
       |
       +-> ANO: LONG DISCONNECT
                client->state = STATE_DISCONNECTED_LONG
                |
                +-> broadcast: PLAYER_DISCONNECTED Alice LONG
                +-> room_remove_player(room, client)
                +-> pokud < 2 hráči: room_end_game()
                +-> client_destroy(client)

  [RECONNECT]
     |
  Nové TCP spojení, klient pošle: RECONNECT Alice
     |
     v
  Hledání klienta s nickname="Alice" a state=SHORT
     |
     +-> NALEZEN:
          - nový socket_fd
          - state = IN_GAME (původní stav)
          - odeslat: WELCOME 1001
          - odeslat: GAME_STATE ... (aktuální stav hry)
          - broadcast: PLAYER_RECONNECTED Alice
          - game->paused = 0 (hra pokračuje)
     |
     +-> NENALEZEN (timeout vypršel):
          - odeslat: ERROR 500 Reconnect_failed


================================================================================
ČÁST II: KLIENT (JAVA + JavaFX)
================================================================================

1. STRUKTURA BALÍČKŮ A TŘÍD
--------------------------------------------------------------------------------

client/
├── src/
│   └── cz/zcu/kiv/ups/pexeso/
│       ├── Main.java                    - vstupní bod (Application)
│       │
│       ├── network/
│       │   ├── ClientConnection.java    - TCP spojení, čtení/zápis
│       │   └── MessageListener.java     - interface pro callback
│       │
│       ├── protocol/
│       │   ├── MessageParser.java       - parsování příchozích zpráv
│       │   ├── MessageBuilder.java      - tvorba odchozích zpráv
│       │   └── ProtocolConstants.java   - konstanty (chybové kódy, příkazy)
│       │
│       ├── model/
│       │   ├── GameState.java           - stav hry (karty, skóre, tahy)
│       │   ├── Room.java                - info o místnosti
│       │   ├── Player.java              - info o hráči
│       │   └── Card.java                - stav jedné karty
│       │
│       ├── controller/
│       │   ├── LoginController.java     - logika pro přihlašovací obrazovku
│       │   ├── LobbyController.java     - logika pro lobby
│       │   └── GameController.java      - logika pro herní obrazovku
│       │
│       └── ui/
│           ├── LoginView.fxml           - FXML pro login
│           ├── LobbyView.fxml           - FXML pro lobby
│           └── GameView.fxml            - FXML pro hru
│
├── pom.xml / build.gradle               - Maven nebo Gradle build
└── resources/
    └── images/                          - obrázky karet (volitelné)


2. DETAILNÍ POPIS TŘÍD
--------------------------------------------------------------------------------

2.1 Main.java
-------------
Odpovědnosti:
  - Vstupní bod aplikace (extends Application)
  - Načtení FXML a zobrazení prvního okna (LoginView)
  - Správa přepínání mezi scénami (login -> lobby -> game)

Metody:
  - void start(Stage primaryStage)
  - void showLoginView()
  - void showLobbyView()
  - void showGameView()


2.2 network/ClientConnection.java
----------------------------------
Odpovědnosti:
  - Správa TCP spojení (Socket)
  - Asynchronní čtení zpráv ze socketu (separátní vlákno)
  - Odesílání zpráv na server
  - Detekce odpojení a automatické znovupřipojení (při SHORT disconnect)
  - Notifikace posluchačů o příchozích zprávách (MessageListener)
  - PONG odpovědi na PING

Atributy:
  - Socket socket
  - BufferedReader reader
  - PrintWriter writer
  - Thread readerThread
  - List<MessageListener> listeners
  - String serverHost
  - int serverPort
  - boolean connected
  - boolean reconnecting

Metody:
  - void connect(String host, int port) throws IOException
  - void disconnect()
  - void sendMessage(String message)
  - void addMessageListener(MessageListener listener)
  - void removeMessageListener(MessageListener listener)

Privátní metody:
  - void startReaderThread() - spustí vlákno pro čtení
  - void readerLoop() - smyčka ve vlákně, čte řádky, volá notifyListeners
  - void notifyListeners(String message)
  - void handleDisconnect() - pokus o reconnect
  - void attemptReconnect() - volá RECONNECT příkaz


2.3 network/MessageListener.java (interface)
---------------------------------------------
interface MessageListener {
    void onMessageReceived(String message);
    void onConnected();
    void onDisconnected();
    void onError(String error);
}


2.4 protocol/MessageParser.java
--------------------------------
Odpovědnosti:
  - Parsování příchozích textových zpráv
  - Rozdělení na příkaz a parametry
  - Validace formátu

Metody:
  - static String getCommand(String message)
  - static String[] getParams(String message)
  - static boolean isValidMessage(String message)


2.5 protocol/MessageBuilder.java
---------------------------------
Odpovědnosti:
  - Tvorba odchozích zpráv (dle specifikace protokolu)
  - Zajištění správného formátu (mezery, \n)

Metody:
  - static String buildHello(String nickname)
  - static String buildListRooms()
  - static String buildCreateRoom(String name, int maxPlayers, int boardSize)
  - static String buildJoinRoom(int roomId)
  - static String buildLeaveRoom()
  - static String buildStartGame()
  - static String buildFlip(int cardId)
  - static String buildPong()
  - static String buildDisconnect()
  - static String buildReconnect(String nickname)


2.6 model/GameState.java
-------------------------
Odpovědnosti:
  - Udržování aktuálního stavu hry
  - Seznam karet, jejich hodnot a stavu (revealed/hidden)
  - Seznam hráčů a jejich skóre
  - Informace o tom, kdo je na tahu
  - Dočasně otočené karty (během tahu)

Atributy:
  - int boardSize
  - List<Card> cards
  - List<Player> players
  - int currentTurnPlayerIndex
  - boolean myTurn
  - List<Integer> flippedCardsThisTurn

Metody:
  - void initBoard(int size)
  - void revealCard(int cardId, int cardValue)
  - void hideCards(List<Integer> cardIds)
  - void updateScore(String nickname, int newScore)
  - void setCurrentTurn(String nickname)
  - void addFlippedCard(int cardId)
  - void clearFlippedCards()
  - boolean isCardRevealed(int cardId)
  - void reset()


2.7 model/Room.java
-------------------
Odpovědnosti:
  - Reprezentace jedné místnosti v lobby

Atributy:
  - int roomId
  - String roomName
  - int currentPlayers
  - int maxPlayers
  - String status (WAITING/PLAYING)


2.8 model/Player.java
---------------------
Odpovědnosti:
  - Reprezentace jednoho hráče

Atributy:
  - String nickname
  - int score
  - boolean disconnected


2.9 model/Card.java
-------------------
Odpovědnosti:
  - Reprezentace jedné karty na herní desce

Atributy:
  - int cardId
  - int cardValue
  - boolean revealed


2.10 controller/LoginController.java
-------------------------------------
Odpovědnosti:
  - Obsluha přihlašovací obrazovky
  - Validace vstupu (IP, port, přezdívka)
  - Připojení k serveru
  - Odeslání HELLO příkazu

FXML komponenty:
  - TextField nicknameField
  - TextField hostField
  - TextField portField
  - Button connectButton
  - Label statusLabel

Metody:
  - void onConnectButtonClicked()
  - void handleWelcome(int clientId) - callback po WELCOME
  - void handleError(String error)


2.11 controller/LobbyController.java
-------------------------------------
Odpovědnosti:
  - Zobrazení seznamu místností
  - Obsluha tlačítek (Refresh, Create, Join, Leave)
  - Pravidelná aktualizace seznamu místností
  - Implementace MessageListener pro příjem zpráv

FXML komponenty:
  - TableView<Room> roomTable
  - Button refreshButton
  - Button createButton
  - Button joinButton
  - Label connectedPlayersLabel

Metody:
  - void initialize() - inicializace po načtení FXML
  - void onRefreshClicked()
  - void onCreateClicked() - zobrazí dialog pro vytvoření místnosti
  - void onJoinClicked()
  - void handleRoomList(List<Room> rooms) - callback po ROOM_LIST
  - void handleRoomJoined(int roomId, List<String> players)
  - void handlePlayerJoined(String nickname)
  - void handlePlayerLeft(String nickname)
  - void handleGameStart(int boardSize, List<String> players) - přepne na GameView
  - void onMessageReceived(String message) - implementace MessageListener


2.12 controller/GameController.java
------------------------------------
Odpovědnosti:
  - Zobrazení herní desky (GridPane s kartami)
  - Obsluha kliknutí na karty
  - Aktualizace skóre a stavu hry
  - Zobrazení, kdo je na tahu
  - Animace otáčení kart
  - Zobrazení výpadků ostatních hráčů
  - Implementace MessageListener pro příjem zpráv

FXML komponenty:
  - GridPane gameBoard
  - Label turnLabel
  - ListView<Player> playerList
  - Label statusLabel

Atributy:
  - GameState gameState
  - ClientConnection connection

Metody:
  - void initialize()
  - void setupBoard(int boardSize)
  - void onCardClicked(int cardId)
  - void handleYourTurn() - callback po YOUR_TURN
  - void handleTurn(String nickname) - callback po TURN
  - void handleCardReveal(int cardId, int cardValue, String nickname)
  - void handleMatch(String nickname, int score)
  - void handleMismatch()
  - void handleGameEnd(String winner, Map<String, Integer> finalScores)
  - void handlePlayerDisconnected(String nickname, String type)
  - void handlePlayerReconnected(String nickname)
  - void updateUI() - aktualizace GUI podle gameState
  - void animateCardFlip(int cardId, int cardValue)
  - void onMessageReceived(String message) - implementace MessageListener


3. ZAJIŠTĚNÍ NEZAMRZNUTÍ GUI
--------------------------------------------------------------------------------

PROBLÉM:
  JavaFX běží na Application Thread (JavaFX Application Thread).
  Pokud na tomto vlákně provedeme blokující operaci (např. socket.read()),
  GUI zamrzne a nebude reagovat na uživatelský vstup.

ŘEŠENÍ:

1. Síťová komunikace v separátním vlákně:
   ----------------------------------------
   ClientConnection spustí readerThread (Thread nebo ExecutorService),
   který běží neustále a čte ze socketu:

   private void startReaderThread() {
       readerThread = new Thread(() -> {
           try {
               String line;
               while (connected && (line = reader.readLine()) != null) {
                   String message = line;
                   notifyListeners(message);
               }
           } catch (IOException e) {
               handleDisconnect();
           }
       });
       readerThread.setDaemon(true);
       readerThread.start();
   }

2. Aktualizace GUI přes Platform.runLater():
   -------------------------------------------
   Když příjde zpráva ze serveru (v readerThread), nemůžeme přímo měnit GUI.
   Musíme použít Platform.runLater(), které spustí kód na JavaFX vlákně:

   private void notifyListeners(String message) {
       for (MessageListener listener : listeners) {
           Platform.runLater(() -> {
               listener.onMessageReceived(message);
           });
       }
   }

   Díky tomu se callback (např. handleCardReveal) provede na správném vlákně
   a může bezpečně měnit GUI komponenty (setText, setVisible, atd.).

3. Odesílání zpráv:
   -----------------
   Odesílání (sendMessage) může být volané z JavaFX vlákna a je rychlé
   (writer.println je non-blocking). Pokud by bylo potřeba, lze i toto
   přesunout do separátního vlákna nebo použít ExecutorService.


SCHÉMA:

  [JavaFX Application Thread]            [Reader Thread]
          |                                     |
          | uživatel klikne na kartu           |
          v                                     |
     onCardClicked(10)                          |
          |                                     |
          v                                     |
     connection.sendMessage("FLIP 10")          |
          |                                     |
          +-----------------------------------> socket.println("FLIP 10")
                                                |
                                                | čeká na odpověď...
                                                v
                                         line = reader.readLine()
                                         "CARD_REVEAL 10 5 Alice"
                                                |
                                                v
                                         notifyListeners(message)
                                                |
                                                v
                                         Platform.runLater(() -> {
                                             listener.onMessageReceived(message);
                                         })
                                                |
          +-------------------------------------+
          |
          v
     controller.onMessageReceived("CARD_REVEAL 10 5 Alice")
          |
          v
     MessageParser.getCommand(message) -> "CARD_REVEAL"
          |
          v
     handleCardReveal(10, 5, "Alice")
          |
          v
     gameState.revealCard(10, 5)
          |
          v
     updateUI() - animace otočení karty, zobrazení hodnoty


4. ZAJIŠTĚNÍ AKTUÁLNÍHO STAVU HRY
--------------------------------------------------------------------------------

PŘÍSTUP:

1. Model (GameState) jako single source of truth:
   -----------------------------------------------
   - GameState obsahuje kompletní stav hry (karty, skóre, tah, ...)
   - Veškeré změny stavu procházejí přes GameState
   - Controller jen reaguje na zprávy a aktualizuje GameState
   - GUI se renderuje podle GameState

2. Event-driven aktualizace:
   --------------------------
   - Když přijde zpráva od serveru (např. CARD_REVEAL), controller:
     a) Parsuje zprávu
     b) Aktualizuje GameState (gameState.revealCard(...))
     c) Zavolá updateUI(), která překreslí GUI podle nového stavu

3. Synchronizace při reconnect:
   ------------------------------
   - Po úspěšném RECONNECT server pošle GAME_STATE s kompletním stavem
   - Controller naparsuje GAME_STATE a kompletně přepíše GameState
   - Zavolá updateUI() -> GUI se znovu vykreslí s aktuálním stavem

4. Konzistence:
   -------------
   - Server je autoritou (server-authoritative model)
   - Klient nikdy neprovádí logiku sám (např. vyhodnocení shody karet)
   - Klient jen zobrazuje stav, který dostal od serveru
   - Díky tomu nemůže dojít k desynchronizaci

PŘÍKLAD FLOW:

  1. Hra začíná -> přijde GAME_START 24 2 Alice Bob
     -> gameState.initBoard(24)
     -> setupBoard(24) vykreslí 24 karet lícem dolů

  2. Přijde YOUR_TURN
     -> turnLabel.setText("Your turn!")
     -> karty se stanou klikatelné

  3. Hráč klikne na kartu 10
     -> connection.sendMessage("FLIP 10")
     -> karta se dočasně animuje (feedback pro uživatele)

  4. Přijde CARD_REVEAL 10 5 Alice
     -> gameState.revealCard(10, 5)
     -> gameState.addFlippedCard(10)
     -> animateCardFlip(10, 5) - otočí kartu lícem nahoru, zobrazí hodnotu

  5. Hráč klikne na kartu 14
     -> connection.sendMessage("FLIP 14")

  6. Přijde CARD_REVEAL 14 5 Alice
     -> gameState.revealCard(14, 5)
     -> gameState.addFlippedCard(14)
     -> animateCardFlip(14, 5)

  7. Přijde MATCH Alice 1
     -> gameState.updateScore("Alice", 1)
     -> gameState.clearFlippedCards() - obě karty zůstanou viditelné
     -> updateUI() - aktualizuje skóre v GUI

  8. Přijde YOUR_TURN (hráč má další tah)
     -> turnLabel.setText("Your turn!")

  9. ... hra pokračuje ...

  10. Přijde PLAYER_DISCONNECTED Bob SHORT
      -> Player bob = gameState.findPlayer("Bob")
      -> bob.setDisconnected(true)
      -> statusLabel.setText("Bob disconnected, waiting for reconnect...")

  11. Přijde PLAYER_RECONNECTED Bob
      -> bob.setDisconnected(false)
      -> statusLabel.setText("")

  12. Přijde GAME_END Alice 2 Alice 8 Bob 5
      -> zobrazit dialog s výsledky
      -> přepnout zpět na LobbyView


================================================================================
ČÁST III: DODATEČNÉ POZNÁMKY
================================================================================

1. BEZPEČNOST A VALIDACE
-------------------------
- Server musí validovat VŠECHNY vstupy od klienta (rozsahy, formát, stav)
- Klient by měl validovat uživatelský vstup (přezdívka, IP, port)
- Počítadlo neplatných zpráv: po 3 chybách -> disconnect
- Logování všech podezřelých aktivit (neplatné zprávy, pokusy o cheat)

2. TESTOVÁNÍ
------------
- Unit testy pro protocol.c (parsování, tvorba zpráv)
- Unit testy pro game.c (logika pexesa, vyhodnocení tahů)
- Integrační testy: spustit server, připojit testovací klienty (skript)
- Zátěžové testy: 10+ klientů, více místností současně
- Testy s nevalidními daty (cat /dev/urandom | nc)
- Testy s výpadky (iptables DROP/REJECT)
- Memory leak testy (valgrind)

3. ROZŠIŘITELNOST
-----------------
- Protokol je navržen pro snadné rozšíření (nové příkazy)
- Modularita umožňuje snadno změnit např. implementaci game logiky
- Možnost přidat chat, statistiky, žebříčky v budoucnu

4. DOKUMENTACE KÓDU
-------------------
- Všechny hlavičkové soubory (.h) musí obsahovat komentáře funkcí
- Složitější algoritmy (např. game_init_board) musí mít popis
- JavaDoc komentáře pro všechny public metody v Javě

5. MAKEFILE (server)
--------------------
Struktura:

  CC = gcc
  CFLAGS = -Wall -Wextra -pthread -g
  LDFLAGS = -pthread

  SOURCES = main.c server.c client_handler.c room.c game.c protocol.c logger.c config.c
  OBJECTS = $(SOURCES:.c=.o)
  TARGET = pexeso_server

  all: $(TARGET)

  $(TARGET): $(OBJECTS)
      $(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

  %.o: %.c
      $(CC) $(CFLAGS) -c $< -o $@

  clean:
      rm -f $(OBJECTS) $(TARGET)

  .PHONY: all clean

6. MAVEN / GRADLE (klient)
---------------------------
Závislosti:
  - JavaFX (org.openjfx:javafx-controls, javafx-fxml)
  - JUnit (pro unit testy)

Build:
  mvn clean package
  -> vytvoří JAR v target/

Run:
  java -jar target/pexeso-client-1.0.jar


================================================================================
SHRNUTÍ
================================================================================

ARCHITEKTURA SERVERU (C):
  - Single-threaded s select() event loop
  - Modularita: 8 souborů (.c/.h) s jasným rozdělením odpovědností
  - Datové struktury: client_t, room_t, game_t s linked lists
  - Protocol parsing/building v samostatném modulu
  - Logování všech událostí
  - PING/PONG keepalive, SHORT/LONG disconnect handling

ARCHITEKTURA KLIENTA (Java + JavaFX):
  - MVC architektura: Model (GameState), View (FXML), Controller
  - Separátní vlákno pro síťovou komunikaci
  - Platform.runLater() pro aktualizaci GUI
  - Event-driven aktualizace stavu hry
  - Automatické reconnect při výpadku

KLÍČOVÉ VLASTNOSTI:
  ✓ Modularita a čitelnost kódu
  ✓ Jasné rozhraní mezi moduly
  ✓ Bezpečná paralelizace (select) / thread-safe GUI (runLater)
  ✓ Validace všech vstupů
  ✓ Robustní error handling
  ✓ Kompletní disconnect/reconnect mechanismus
  ✓ Logování pro debugging a audit

Tento návrh lze přímo použít pro implementaci v dalších packetech.

================================================================================
KONEC DOKUMENTU
================================================================================
