📋 Kompletní přehled implementace - Co máš TERAZ

  Vysvětlím ti detailně každý soubor a jak server funguje od spuštění po ukončení.

  ---
  🗂️ Struktura projektu

  server_src/
  ├── main.c              - Vstupní bod, parsování argumentů
  ├── server.h / .c       - TCP socket, accept loop
  ├── client_handler.h / .c - Thread pro každého klienta
  ├── protocol.h          - Konstanty protokolu
  ├── logger.h / .c       - Thread-safe logování
  ├── Makefile            - Build systém
  └── server              - Binárka (po make)

  ---
  📄 Soubor po souboru

  1️⃣ logger.h / logger.c - Logování

  Co dělá:
  - Thread-safe logování s mutex zámkem
  - Zapisuje do souboru server.log + stdout současně
  - Časová razítka v ISO formátu
  - Tři úrovně: INFO, WARNING, ERROR

  Klíčové funkce:

  int logger_init(const char *filename);
  // Inicializuje logger, otevře soubor
  // Volá se na začátku main()

  void logger_log(log_level_t level, const char *format, ...);
  // Thread-safe logování s variadickými argumenty
  // Příklad: logger_log(LOG_INFO, "Client %d connected", client_id);

  void logger_shutdown(void);
  // Uzavře log soubor
  // Volá se na konci main()

  Jak funguje thread-safety:
  static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

  void logger_log(...) {
      pthread_mutex_lock(&log_mutex);    // ← Zamkni
      // ... zápis do souboru ...
      pthread_mutex_unlock(&log_mutex);  // ← Odemkni
  }

  Proč je to důležité:
  - Když 10 threadů loguje současně, nevznikne zmatek v logu
  - Bez mutexu by se řádky v logu mohly promíchat

  ---
  2️⃣ protocol.h - Protokolové konstanty

  Co obsahuje:
  - Definice všech příkazů (C→S a S→C)
  - Chybové kódy
  - Limity (délka zpráv, nicku, názvu místnosti)
  - Enum client_state_t - stavy klienta

  Příklady:

  // Buffer sizes
  #define MAX_MESSAGE_LENGTH 1024
  #define MAX_NICK_LENGTH 32

  // Client commands
  #define CMD_HELLO "HELLO"
  #define CMD_LIST_ROOMS "LIST_ROOMS"

  // Server commands
  #define CMD_WELCOME "WELCOME"
  #define CMD_ERROR "ERROR"

  // Error codes
  #define ERR_NOT_IMPLEMENTED "NOT_IMPLEMENTED"
  #define ERR_INVALID_COMMAND "INVALID_COMMAND"

  // Client states
  typedef enum {
      STATE_DISCONNECTED,
      STATE_CONNECTED,
      STATE_AUTHENTICATED,
      STATE_IN_LOBBY,
      STATE_IN_ROOM,
      STATE_IN_GAME
  } client_state_t;

  Použití:
  // Místo:
  send(fd, "ERROR INVALID_COMMAND\n", 22, 0);

  // Používáš:
  snprintf(response, sizeof(response), "%s %s\n", CMD_ERROR, ERR_INVALID_COMMAND);
  send(fd, response, strlen(response), 0);

  ---
  3️⃣ client_handler.h / .c - Správa jednoho klienta

  Struktura klienta:

  typedef struct {
      int socket_fd;                    // File descriptor socketu
      char nickname[MAX_NICK_LENGTH];   // Přezdívka (po HELLO)
      client_state_t state;             // Aktuální stav
      time_t last_activity;             // Poslední aktivita (pro timeout)
      int invalid_message_count;        // Počítadlo špatných zpráv
      int client_id;                    // Unikátní ID
  } client_t;

  Hlavní funkce:

  void* client_handler_thread(void *arg)

  Co dělá:
  1. Přijme client_t * jako argument
  2. Pošle úvodní zprávu: ERROR NOT_IMPLEMENTED ...
  3. Smyčka:
    - Volá recv() pro čtení dat
    - Postupně skládá zprávy do line_buffer
    - Když najde \n, zpracuje celý řádek
    - Zaloguje přijatou zprávu
    - Odpoví testovací zprávou
  4. Pokud recv() vrátí 0 → klient se odpojil
  5. Uzavře socket, uvolní paměť, ukončí thread

  Kód (zjednodušeně):

  void* client_handler_thread(void *arg) {
      client_t *client = (client_t *)arg;
      char buffer[MAX_MESSAGE_LENGTH];
      char line_buffer[MAX_MESSAGE_LENGTH];
      int line_pos = 0;

      // Úvodní zpráva
      client_send_message(client, "ERROR NOT_IMPLEMENTED ...");

      // Smyčka čtení
      while (1) {
          int bytes = recv(client->socket_fd, buffer, sizeof(buffer)-1, 0);

          if (bytes <= 0) break;  // Odpojení nebo chyba

          buffer[bytes] = '\0';

          // Zpracuj znak po znaku
          for (int i = 0; i < bytes; i++) {
              if (buffer[i] == '\n') {
                  // Máme kompletní řádek
                  line_buffer[line_pos] = '\0';
                  logger_log(LOG_INFO, "Client %d: Received: '%s'",
                            client->client_id, line_buffer);

                  // Odpověď
                  client_send_message(client, "ERROR NOT_IMPLEMENTED ...");

                  // Reset bufferu
                  line_pos = 0;
              } else if (buffer[i] != '\r') {
                  line_buffer[line_pos++] = buffer[i];
              }
          }
      }

      // Cleanup
      close(client->socket_fd);
      free(client);
      return NULL;
  }

  Proč zpracování znak po znaku?
  - recv() může vrátit část zprávy (díky InTCPtor nebo reálné síti)
  - Například "HELLO Alice\n" může přijít jako:
        i. recv() → "HEL"
        ii. recv() → "LO Al"
        iii. recv() → "ice\n"
  - line_buffer postupně sbírá znaky, dokud nenajde \n

  int client_send_message(client_t *client, const char *message)

  Co dělá:
  - Přidá \n na konec zprávy (pokud tam není)
  - Pošle přes send()
  - Vrátí počet odeslaných bajtů nebo -1 při chybě

  int client_send_message(client_t *client, const char *message) {
      char buffer[MAX_MESSAGE_LENGTH + 2];
      int len = snprintf(buffer, sizeof(buffer), "%s\n", message);

      int sent = send(client->socket_fd, buffer, len, 0);
      return sent;
  }

  ---
  4️⃣ server.h / .c - TCP server a accept loop

  Globální konfigurace:

  typedef struct {
      char ip[64];          // IP adresa (např. "127.0.0.1")
      int port;             // Port (např. 10000)
      int max_rooms;        // Max místností (např. 10)
      int max_clients;      // Max klientů (např. 50)
      int listen_fd;        // File descriptor listening socketu
      int running;          // Flag pro běh serveru (1 = běží, 0 = stop)
      int next_client_id;   // Čítač pro unikátní ID
  } server_config_t;

  Hlavní funkce:

  int server_init(const char *ip, int port, int max_rooms, int max_clients)

  Krok za krokem:

  1. Vytvoř socket:
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  // AF_INET = IPv4, SOCK_STREAM = TCP
  2. Nastav SO_REUSEADDR (umožňuje restart bez "Address already in use"):
  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  3. Připrav adresu:
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);  // Host to Network Short
  inet_pton(AF_INET, ip, &server_addr.sin_addr);  // Text → binární IP
  4. Bind (naváž socket na IP:port):
  bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  5. Listen (začni naslouchat):
  listen(listen_fd, 10);  // 10 = backlog (čekající spojení)

  void server_run(void)

  Nekonečná smyčka:

  void server_run(void) {
      while (server_config.running) {
          // 1. Accept nového klienta
          struct sockaddr_in client_addr;
          socklen_t len = sizeof(client_addr);
          int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &len);

          if (client_fd < 0) continue;  // Chyba nebo server se zastavuje

          // 2. Získej IP adresu klienta
          char client_ip[INET_ADDRSTRLEN];
          inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));

          // 3. Vytvoř strukturu klienta
          client_t *client = malloc(sizeof(client_t));
          client->socket_fd = client_fd;
          client->state = STATE_CONNECTED;
          client->last_activity = time(NULL);
          client->invalid_message_count = 0;
          client->client_id = next_client_id++;

          // 4. Spusť thread
          pthread_t thread_id;
          pthread_create(&thread_id, NULL, client_handler_thread, client);
          pthread_detach(thread_id);  // Automatický cleanup po ukončení
      }
  }

  Co je pthread_detach()?
  - Normálně by sis musel thread uložit a později zavolat pthread_join()
  - pthread_detach() říká: "Když thread skončí, automaticky uvolni jeho zdroje"
  - Proto nemusíš trackovat všechny thready

  void server_shutdown(void)

  Co dělá:
  - Nastaví running = 0 (zastaví accept loop)
  - Uzavře listening socket
  - Existující client thready doběhnou samy

  void server_shutdown(void) {
      server_config.running = 0;
      close(server_config.listen_fd);
  }

  ---
  5️⃣ main.c - Vstupní bod

  Flow programu:

  int main(int argc, char *argv[]) {
      // 1. Zkontroluj argumenty
      if (argc != 5) {
          print_usage(argv[0]);
          return 1;
      }

      // 2. Parsuj argumenty
      const char *ip = argv[1];
      int port = atoi(argv[2]);
      int max_rooms = atoi(argv[3]);
      int max_clients = atoi(argv[4]);

      // 3. Validuj
      if (port <= 0 || port > 65535) {
          fprintf(stderr, "Invalid port\n");
          return 1;
      }

      // 4. Inicializuj logger
      logger_init("server.log");

      // 5. Nastav signal handlery
      signal(SIGINT, signal_handler);   // Ctrl+C
      signal(SIGTERM, signal_handler);  // kill

      // 6. Inicializuj server
      server_init(ip, port, max_rooms, max_clients);

      // 7. Spusť hlavní loop (blokuje dokud není ukončen)
      server_run();

      // 8. Cleanup
      server_shutdown();
      logger_shutdown();

      return 0;
  }

  Signal handler:

  void signal_handler(int signum) {
      logger_log(LOG_INFO, "Received signal %d, shutting down...", signum);

      server_config_t *config = server_get_config();
      config->running = 0;  // Zastaví accept loop
  }

  Proč je to důležité:
  - Když zmáčkneš Ctrl+C, nedojde k násilnému ukončení
  - Server dostane šanci uzavřít sockety a logovat shutdown

  ---
  6️⃣ Makefile - Build systém

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

  Co znamenají flagy:
  - -Wall -Wextra = všechna varování (pomáhá odhalit bugy)
  - -pthread = podpora POSIX threads
  - -g = debug symboly (pro valgrind/gdb)
  - -std=c99 = C99 standard

  Příkazy:
  make          # Sestaví server
  make clean    # Smaže binárku a objekty
  make run      # Sestaví a spustí s výchozími parametry
  make valgrind # Spustí s valgrind kontrolou paměti

  ---
  🔄 Životní cyklus - Jak to celé funguje

  Fáze 1: Spuštění

  ./server 127.0.0.1 10000 10 50
      ↓
  main()
      ↓
  logger_init("server.log")        ← Otevře log
      ↓
  server_init(...)                 ← Vytvoří socket, bind, listen
      ↓
  server_run()                     ← Smyčka

  Fáze 2: Přijetí klienta

  accept() → client_fd = 5
      ↓
  malloc(client_t)                 ← Alokace struktury
      ↓
  client->socket_fd = 5
  client->state = STATE_CONNECTED
  client->client_id = 1
      ↓
  pthread_create(..., client_handler_thread, client)
      ↓
  pthread_detach(...)              ← Thread poběží samostatně

  Fáze 3: Komunikace (v threadu)

  client_handler_thread(client)
      ↓
  send("ERROR NOT_IMPLEMENTED ...")   ← Úvodní zpráva
      ↓
  while (1) {
      recv() → "HEL"                  ← Částečná data
      recv() → "LO Alice\n"            ← Zbytek + newline

      → line_buffer = "HELLO Alice"
      → logger_log("Received: 'HELLO Alice'")
      → send("ERROR NOT_IMPLEMENTED ...")
  }

  Fáze 4: Odpojení

  recv() → 0 bajtů                 ← Klient zavřel spojení
      ↓
  logger_log("Connection closed")
      ↓
  close(socket_fd)                 ← Uzavři socket
      ↓
  free(client)                     ← Uvolni paměť
      ↓
  return NULL                      ← Thread končí (detached → auto-cleanup)

  Fáze 5: Ukončení serveru

  Ctrl+C
      ↓
  signal_handler(SIGINT)
      ↓
  server_config.running = 0        ← Nastav flag
      ↓
  accept() vrátí chybu             ← Loop končí
      ↓
  server_shutdown()                ← Uzavře listening socket
      ↓
  logger_shutdown()                ← Uzavře log soubor
      ↓
  exit(0)

  ---
  🔍 Co NENÍ implementováno (zatím)

  | Funkce                  | Status                                 |
  |-------------------------|----------------------------------------|
  | ❌ Parsování příkazů     | Všechny příkazy vrací NOT_IMPLEMENTED  |
  | ❌ Autentizace (HELLO)   | Není implementováno                    |
  | ❌ Správa místností      | room.c neexistuje                      |
  | ❌ Hra Pexeso            | game.c neexistuje                      |
  | ❌ PING/PONG keepalive   | Není implementováno                    |
  | ❌ Reconnect mechanismus | Není implementováno                    |
  | ❌ Invalid message limit | Počítá se, ale neodpojuje po 3 chybách |
  | ❌ Protokolová validace  | Neověřuje správnost parametrů          |

  ---
  ✅ Co JE implementováno a funguje

  | Funkce                   | Status                         | Testováno         |
  |--------------------------|--------------------------------|-------------------|
  | ✅ TCP socket server      | Plně funkční                   | ✅                 |
  | ✅ Accept loop            | Přijímá neomezeně klientů      | ✅                 |
  | ✅ Thread-per-client      | Každý klient má vlastní thread | ✅                 |
  | ✅ Zpracování fragmentace | Správně skládá částečné zprávy | ✅ InTCPtor        |
  | ✅ Thread-safe logging    | Mutex zamykání                 | ✅                 |
  | ✅ Signal handling        | Graceful shutdown na Ctrl+C    | ✅                 |
  | ✅ Error handling         | Všechny syscalls ošetřeny      | ✅                 |
  | ✅ Memory management      | free() po odpojení klienta     | ⚠️ Needs valgrind |
  | ✅ Line-based protocol    | Parsuje řádky ukončené \n      | ✅                 |

  ---
  📊 Diagram toku dat

  ┌─────────────┐
  │   Klient    │
  └──────┬──────┘
         │ TCP connect
         ↓
  ┌─────────────────────────────────┐
  │  server_run() - accept loop     │
  │  ┌───────────────────────────┐  │
  │  │ accept() → client_fd      │  │
  │  │ malloc(client_t)          │  │
  │  │ pthread_create()          │  │
  │  └───────────┬───────────────┘  │
  └──────────────┼──────────────────┘
                 │
                 ↓
  ┌──────────────────────────────────┐
  │ client_handler_thread (THREAD)   │
  │ ┌──────────────────────────────┐ │
  │ │ while (1) {                  │ │
  │ │   recv() → buffer            │ │
  │ │   for each char:             │ │
  │ │     if (\n) process_line()   │ │
  │ │   send() → response          │ │
  │ │ }                            │ │
  │ └──────────────────────────────┘ │
  └──────────────────────────────────┘
                 │
                 ↓
  ┌──────────────────────────────────┐
  │ logger_log() (THREAD-SAFE)       │
  │ ┌──────────────────────────────┐ │
  │ │ pthread_mutex_lock()         │ │
  │ │ fprintf(log_file, ...)       │ │
  │ │ printf(...)                  │ │
  │ │ pthread_mutex_unlock()       │ │
  │ └──────────────────────────────┘ │
  └──────────────────────────────────┘

   ---