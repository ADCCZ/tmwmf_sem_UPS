# MANUÁLNÍ PRŮVODCE - PEXESO SERVER-KLIENT

**Účel:** Krok-po-kroku návod, jak implementovat projekt Pexeso bez AI
**Pro:** Studenty KIV/UPS, vývojáře učící se síťové programování
**Poslední aktualizace:** 2025-11-12 (Packety 1-7)

---

## ÚVOD

Tento dokument obsahuje detailní návod, jak postupně vytvořit kompletní síťovou hru Pexeso s architekturou server-klient. Každý packet je nezávislý krok, který můžeš implementovat samostatně.

**Prerekvizity:**
- Základy C (pointery, struktury, funkce)
- Základy Javy (OOP, threads)
- Linux prostředí (pro server)
- Základy síťového programování (TCP/IP, sockety)

---

## PACKET 1: NÁVRH PROTOKOLU

### Cíl:
Definovat textový protokol pro komunikaci mezi klientem a serverem

### Krok za krokem:

1. **Vytvoř soubor `protokol_pexeso.md`**

2. **Navrhni formát zpráv:**
   - Textový protokol, řádky ukončené `\n`
   - ASCII only, žádná diakritika
   - Formát: `COMMAND [PARAM1] [PARAM2] ...\n`

3. **Definuj příkazy klient → server:**
   ```
   HELLO <nickname>           - Autentizace
   LIST_ROOMS                 - Seznam místností
   CREATE_ROOM <name> <max>   - Vytvoření místnosti
   JOIN_ROOM <id>             - Vstup do místnosti
   LEAVE_ROOM                 - Opuštění místnosti
   START_GAME                 - Zahájení hry
   FLIP <card_id>             - Otočení karty
   PONG                       - Odpověď na keepalive
   RECONNECT <client_id>      - Znovupřipojení
   QUIT                       - Odpojení
   ```

4. **Definuj příkazy server → klient:**
   ```
   WELCOME <client_id>        - Potvrzení autentizace
   ROOM_LIST <data>           - Seznam místností
   ROOM_CREATED <id> <name>   - Místnost vytvořena
   ROOM_JOINED <id> <name>    - Vstup do místnosti
   PLAYER_JOINED <nick>       - Hráč vstoupil (broadcast)
   PLAYER_LEFT <nick>         - Hráč odešel (broadcast)
   GAME_START <data>          - Hra začala
   YOUR_TURN                  - Tvůj tah
   CARD_REVEAL <id> <val>     - Karta otočena
   MATCH <nick> <score>       - Shoda nalezena
   MISMATCH                   - Neshoda
   GAME_END <data>            - Hra skončila
   PING                       - Keepalive check
   ERROR <code> <msg>         - Chyba
   ```

5. **Nakresli stavový diagram:**
   - Stavy klienta: DISCONNECTED → CONNECTED → AUTHENTICATED → IN_LOBBY → IN_ROOM → IN_GAME
   - Přechody mezi stavy (např. HELLO → AUTHENTICATED)

6. **Definuj chybové kódy:**
   ```
   INVALID_COMMAND      - Neznámý příkaz
   INVALID_PARAMS       - Chybné parametry
   NOT_AUTHENTICATED    - Není autentifikován
   ROOM_NOT_FOUND       - Místnost neexistuje
   ROOM_FULL            - Místnost plná
   NOT_YOUR_TURN        - Není tvůj tah
   ...
   ```

7. **Specifikuj mechanismus znovupřipojení:**
   - Krátký výpadek (< 60s): hra čeká
   - Dlouhý výpadek (> 60s): hráč odstraněn
   - PING/PONG každých 15s

**Výstup:** Dokument s kompletní specifikací protokolu

---

## PACKET 2: NÁVRH ARCHITEKTURY

### Cíl:
Navrhnout strukturu serverové a klientské aplikace

### Server v C:

1. **Vytvoř soubor `architektura.md`**

2. **Navrhni rozdělení do modulů:**
   ```
   main.c              - Vstupní bod, parsování argumentů
   server.c/h          - Socket, accept loop, koordinace
   client_handler.c/h  - Správa jednoho klienta (thread)
   room.c/h            - Správa místností a lobby
   game.c/h            - Logika Pexesa
   protocol.c/h        - Parsování a tvorba zpráv
   logger.c/h          - Thread-safe logování
   config.c/h          - Načítání konfigurace
   ```

3. **Navrhni datové struktury:**
   ```c
   // Klient
   typedef struct {
       int socket_fd;
       char nickname[32];
       client_state_t state;
       time_t last_activity;
       int invalid_message_count;
       int client_id;
       room_t *room;
   } client_t;

   // Místnost
   typedef struct {
       int room_id;
       char name[64];
       client_t *players[4];
       int player_count;
       int max_players;
       room_state_t state;
       game_t *game;
   } room_t;

   // Hra
   typedef struct {
       int *cards;
       int *revealed;
       int board_size;
       int current_player;
       int *scores;
   } game_t;
   ```

4. **Rozhodni o paralelizaci:**
   - **Volby:** select(), vlákna, procesy
   - **Doporučení:** POSIX threads (pthread) - thread-per-client
   - **Důvody:**
     - Jednoduchá implementace
     - Blokující I/O je OK
     - Vhodné pro desítky klientů
     - Každý klient izolovaný

### Klient v Javě:

5. **Navrhni balíčky:**
   ```
   cz.zcu.kiv.ups.pexeso/
   ├── Main.java
   ├── network/
   │   ├── ClientConnection.java
   │   └── MessageListener.java
   ├── protocol/
   │   ├── MessageParser.java
   │   └── ProtocolConstants.java
   ├── model/
   │   ├── GameState.java
   │   └── ...
   ├── controller/
   │   ├── LoginController.java
   │   └── ...
   └── ui/
       ├── LoginView.fxml
       └── ...
   ```

6. **Navrhni thread model:**
   - GUI thread (JavaFX) - UI updates
   - Network thread - čtení zpráv
   - Použití `Platform.runLater()` pro UI updates

**Výstup:** Dokument `architektura.md` s návrhem

---

## PACKET 3: KOSTRA SERVERU V C

### Cíl:
Vytvořit funkční TCP server, který přijímá spojení a čte zprávy

### Krok za krokem:

#### 1. Vytvoř `logger.h`

```c
#ifndef LOGGER_H
#define LOGGER_H

typedef enum {
    LOG_INFO,
    LOG_WARNING,
    LOG_ERROR
} log_level_t;

int logger_init(const char *filename);
void logger_log(log_level_t level, const char *format, ...);
void logger_shutdown(void);

#endif
```

#### 2. Implementuj `logger.c`

```c
#include "logger.h"
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>

static FILE *log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

int logger_init(const char *filename) {
    pthread_mutex_lock(&log_mutex);

    if (filename != NULL) {
        log_file = fopen(filename, "a");
        if (log_file == NULL) {
            pthread_mutex_unlock(&log_mutex);
            return -1;
        }
    }

    pthread_mutex_unlock(&log_mutex);
    return 0;
}

void logger_log(log_level_t level, const char *format, ...) {
    pthread_mutex_lock(&log_mutex);

    // Získej aktuální čas
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);

    // Urči level string
    const char *level_str = (level == LOG_INFO) ? "INFO" :
                           (level == LOG_WARNING) ? "WARN" : "ERROR";

    // Print do stdout
    printf("[%s] [%s] ", time_buf, level_str);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");

    // Print do souboru
    if (log_file != NULL) {
        fprintf(log_file, "[%s] [%s] ", time_buf, level_str);
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }

    pthread_mutex_unlock(&log_mutex);
}

void logger_shutdown(void) {
    pthread_mutex_lock(&log_mutex);
    if (log_file != NULL) {
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}
```

**Klíčové body:**
- Mutex pro thread-safety
- Zápis do souboru i stdout
- Variadické funkce (va_list)

#### 3. Vytvoř `protocol.h`

```c
#ifndef PROTOCOL_H
#define PROTOCOL_H

#define MAX_MESSAGE_LENGTH 1024
#define MAX_NICK_LENGTH 32

#define CMD_HELLO "HELLO"
#define CMD_LIST_ROOMS "LIST_ROOMS"
// ... další příkazy

typedef enum {
    STATE_DISCONNECTED,
    STATE_CONNECTED,
    STATE_AUTHENTICATED,
    STATE_IN_LOBBY,
    STATE_IN_ROOM,
    STATE_IN_GAME
} client_state_t;

#endif
```

#### 4. Vytvoř `client_handler.h`

```c
#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include "protocol.h"
#include <time.h>

struct room_s;  // Forward declaration

typedef struct {
    int socket_fd;
    char nickname[MAX_NICK_LENGTH];
    client_state_t state;
    time_t last_activity;
    int invalid_message_count;
    int client_id;
    struct room_s *room;
} client_t;

void* client_handler_thread(void *arg);
int client_send_message(client_t *client, const char *message);

#endif
```

#### 5. Implementuj `client_handler.c` (kostra)

```c
#include "client_handler.h"
#include "logger.h"
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>

int client_send_message(client_t *client, const char *message) {
    if (client == NULL || message == NULL) return -1;

    char buffer[MAX_MESSAGE_LENGTH + 2];
    int len = snprintf(buffer, sizeof(buffer), "%s\n", message);

    return send(client->socket_fd, buffer, len, 0);
}

void* client_handler_thread(void *arg) {
    client_t *client = (client_t *)arg;
    char buffer[MAX_MESSAGE_LENGTH];
    char line_buffer[MAX_MESSAGE_LENGTH];
    int line_pos = 0;

    logger_log(LOG_INFO, "Client %d: Handler started", client->client_id);

    while (1) {
        int bytes = recv(client->socket_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytes <= 0) break;  // Disconnect

        buffer[bytes] = '\0';

        // Zpracuj znak po znaku
        for (int i = 0; i < bytes; i++) {
            char c = buffer[i];

            if (c == '\n') {
                line_buffer[line_pos] = '\0';
                if (line_pos > 0) {
                    logger_log(LOG_INFO, "Client %d: '%s'",
                              client->client_id, line_buffer);

                    // TODO: handle_message(client, line_buffer);
                    client_send_message(client,
                        "ERROR NOT_IMPLEMENTED Command not yet implemented");
                }
                line_pos = 0;
            } else if (c != '\r') {
                if (line_pos < MAX_MESSAGE_LENGTH - 1) {
                    line_buffer[line_pos++] = c;
                }
            }
        }
    }

    logger_log(LOG_INFO, "Client %d: Closing", client->client_id);
    close(client->socket_fd);
    free(client);

    return NULL;
}
```

**Klíčové body:**
- Zpracování znak po znaku (fragmentace zpráv)
- Ignorování `\r`
- Parsování řádků ukončených `\n`

#### 6. Vytvoř `server.h`

```c
#ifndef SERVER_H
#define SERVER_H

typedef struct {
    char ip[64];
    int port;
    int max_rooms;
    int max_clients;
    int listen_fd;
    int running;
    int next_client_id;
} server_config_t;

int server_init(const char *ip, int port, int max_rooms, int max_clients);
void server_run(void);
void server_shutdown(void);
server_config_t* server_get_config(void);

#endif
```

#### 7. Implementuj `server.c`

```c
#include "server.h"
#include "client_handler.h"
#include "logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

static server_config_t server_config;

server_config_t* server_get_config(void) {
    return &server_config;
}

int server_init(const char *ip, int port, int max_rooms, int max_clients) {
    // Inicializuj config
    strncpy(server_config.ip, ip, sizeof(server_config.ip) - 1);
    server_config.port = port;
    server_config.max_rooms = max_rooms;
    server_config.max_clients = max_clients;
    server_config.running = 1;
    server_config.next_client_id = 1;

    // Vytvoř socket
    server_config.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_config.listen_fd < 0) {
        logger_log(LOG_ERROR, "Failed to create socket");
        return -1;
    }

    // SO_REUSEADDR
    int opt = 1;
    setsockopt(server_config.listen_fd, SOL_SOCKET, SO_REUSEADDR,
              &opt, sizeof(opt));

    // Bind
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (bind(server_config.listen_fd, (struct sockaddr *)&server_addr,
            sizeof(server_addr)) < 0) {
        logger_log(LOG_ERROR, "Failed to bind");
        close(server_config.listen_fd);
        return -1;
    }

    // Listen
    if (listen(server_config.listen_fd, 10) < 0) {
        logger_log(LOG_ERROR, "Failed to listen");
        close(server_config.listen_fd);
        return -1;
    }

    logger_log(LOG_INFO, "Server initialized: %s:%d", ip, port);
    return 0;
}

void server_run(void) {
    logger_log(LOG_INFO, "Server started");

    while (server_config.running) {
        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);

        int client_fd = accept(server_config.listen_fd,
                              (struct sockaddr *)&client_addr, &len);

        if (client_fd < 0) continue;

        // Vytvoř client strukturu
        client_t *client = (client_t *)malloc(sizeof(client_t));
        client->socket_fd = client_fd;
        client->state = STATE_CONNECTED;
        client->last_activity = time(NULL);
        client->invalid_message_count = 0;
        client->client_id = server_config.next_client_id++;
        client->room = NULL;

        // Spusť thread
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, client_handler_thread, client);
        pthread_detach(thread_id);

        logger_log(LOG_INFO, "Client %d connected", client->client_id);
    }
}

void server_shutdown(void) {
    server_config.running = 0;
    if (server_config.listen_fd >= 0) {
        close(server_config.listen_fd);
    }
    logger_log(LOG_INFO, "Server shutdown");
}
```

**Klíčové funkce:**
- `socket()` - vytvoření socketu
- `setsockopt()` - SO_REUSEADDR
- `bind()` - navázání na IP:port
- `listen()` - začít naslouchat
- `accept()` - přijmout spojení
- `pthread_create()` - vytvořit thread
- `pthread_detach()` - automatický cleanup

#### 8. Vytvoř `main.c`

```c
#include "server.h"
#include "logger.h"
#include <signal.h>
#include <stdlib.h>

void signal_handler(int sig) {
    logger_log(LOG_INFO, "Signal %d received", sig);
    server_config_t *cfg = server_get_config();
    cfg->running = 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: %s <IP> <PORT> <MAX_ROOMS> <MAX_CLIENTS>\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    int max_rooms = atoi(argv[3]);
    int max_clients = atoi(argv[4]);

    // Inicializuj logger
    logger_init("server.log");

    // Signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Inicializuj server
    if (server_init(ip, port, max_rooms, max_clients) != 0) {
        return 1;
    }

    // Hlavní loop
    server_run();

    // Cleanup
    server_shutdown();
    logger_shutdown();

    return 0;
}
```

#### 9. Vytvoř `Makefile`

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

.PHONY: all clean run
```

#### 10. Zkompiluj a otestuj

```bash
make
./server 127.0.0.1 10000 10 50

# V druhém terminálu
nc 127.0.0.1 10000
HELLO Alice
# → ERROR NOT_IMPLEMENTED Command not yet implemented
```

**Výstup:** Funkční server, který přijímá spojení a čte zprávy

---

## PACKET 4: KOSTRA KLIENTA V JAVĚ

### Cíl:
Vytvořit JavaFX aplikaci s GUI a TCP spojením

### Krok za krokem:

#### 1. Vytvoř Maven projekt

```bash
cd client_src
```

#### 2. Vytvoř `pom.xml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0
                            http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>cz.zcu.kiv.ups</groupId>
    <artifactId>pexeso-client</artifactId>
    <version>1.0-SNAPSHOT</version>

    <properties>
        <maven.compiler.source>17</maven.compiler.source>
        <maven.compiler.target>17</maven.compiler.target>
        <javafx.version>17.0.2</javafx.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.openjfx</groupId>
            <artifactId>javafx-controls</artifactId>
            <version>${javafx.version}</version>
        </dependency>
        <dependency>
            <groupId>org.openjfx</groupId>
            <artifactId>javafx-fxml</artifactId>
            <version>${javafx.version}</version>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <plugin>
                <groupId>org.openjfx</groupId>
                <artifactId>javafx-maven-plugin</artifactId>
                <version>0.0.8</version>
                <configuration>
                    <mainClass>cz.zcu.kiv.ups.pexeso.Main</mainClass>
                </configuration>
            </plugin>
        </plugins>
    </build>
</project>
```

#### 3. Vytvoř balíčkovou strukturu

```bash
mkdir -p src/main/java/cz/zcu/kiv/ups/pexeso/{network,protocol,controller}
mkdir -p src/main/resources/cz/zcu/kiv/ups/pexeso/ui
```

#### 4. Vytvoř `ProtocolConstants.java`

```java
package cz.zcu.kiv.ups.pexeso.protocol;

public class ProtocolConstants {
    public static final String CMD_HELLO = "HELLO";
    public static final String CMD_LIST_ROOMS = "LIST_ROOMS";
    public static final String CMD_WELCOME = "WELCOME";
    public static final String CMD_ERROR = "ERROR";
    public static final String CMD_PING = "PING";
    public static final String CMD_PONG = "PONG";

    public static final int CONNECTION_TIMEOUT_MS = 5000;
    public static final int MAX_NICK_LENGTH = 32;
}
```

#### 5. Vytvoř `MessageListener.java`

```java
package cz.zcu.kiv.ups.pexeso.network;

public interface MessageListener {
    void onMessageReceived(String message);
    void onConnected();
    void onDisconnected(String reason);
    void onError(String error);
}
```

#### 6. Vytvoř `ClientConnection.java`

```java
package cz.zcu.kiv.ups.pexeso.network;

import java.io.*;
import java.net.Socket;

public class ClientConnection {
    private Socket socket;
    private PrintWriter out;
    private BufferedReader in;
    private Thread readerThread;
    private volatile boolean running = false;
    private MessageListener listener;

    public ClientConnection(MessageListener listener) {
        this.listener = listener;
    }

    public void connect(String host, int port) throws IOException {
        socket = new Socket(host, port);
        out = new PrintWriter(socket.getOutputStream(), true);
        in = new BufferedReader(new InputStreamReader(socket.getInputStream()));

        running = true;
        readerThread = new Thread(this::readerLoop);
        readerThread.setDaemon(true);
        readerThread.start();

        if (listener != null) {
            listener.onConnected();
        }
    }

    public boolean sendMessage(String message) {
        if (out == null) return false;
        out.println(message);
        return !out.checkError();
    }

    private void readerLoop() {
        try {
            String line;
            while (running && (line = in.readLine()) != null) {
                if (listener != null) {
                    listener.onMessageReceived(line);
                }
            }
        } catch (IOException e) {
            if (running && listener != null) {
                listener.onError("Connection lost: " + e.getMessage());
            }
        } finally {
            running = false;
            cleanup();
        }
    }

    public void disconnect() {
        running = false;
        cleanup();
        if (listener != null) {
            listener.onDisconnected("User requested disconnect");
        }
    }

    private void cleanup() {
        try {
            if (out != null) out.close();
            if (in != null) in.close();
            if (socket != null) socket.close();
        } catch (IOException e) {
            // Ignore
        }
    }

    public boolean isConnected() {
        return socket != null && socket.isConnected() && !socket.isClosed();
    }
}
```

**Klíčové body:**
- Samostatný thread pro čtení (`readerLoop`)
- Non-blocking pro GUI
- Callback přes `MessageListener`

#### 7. Vytvoř `LoginController.java`

```java
package cz.zcu.kiv.ups.pexeso.controller;

import cz.zcu.kiv.ups.pexeso.network.ClientConnection;
import cz.zcu.kiv.ups.pexeso.network.MessageListener;
import cz.zcu.kiv.ups.pexeso.protocol.ProtocolConstants;
import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.scene.control.*;

public class LoginController implements MessageListener {
    @FXML private TextField ipField;
    @FXML private TextField portField;
    @FXML private TextField nicknameField;
    @FXML private Button connectButton;
    @FXML private TextArea logArea;

    private ClientConnection connection;
    private boolean connected = false;

    @FXML
    public void initialize() {
        ipField.setText("127.0.0.1");
        portField.setText("10000");
        nicknameField.setText("Player" + (int)(Math.random() * 1000));

        connection = new ClientConnection(this);
        log("Client initialized");
    }

    @FXML
    private void handleConnect() {
        if (connected) {
            disconnect();
        } else {
            connect();
        }
    }

    private void connect() {
        String ip = ipField.getText();
        int port = Integer.parseInt(portField.getText());
        String nickname = nicknameField.getText();

        setInputsEnabled(false);
        log("Connecting to " + ip + ":" + port);

        new Thread(() -> {
            try {
                connection.connect(ip, port);
                connection.sendMessage("HELLO " + nickname);
            } catch (Exception e) {
                Platform.runLater(() -> {
                    log("ERROR: " + e.getMessage());
                    setInputsEnabled(true);
                });
            }
        }).start();
    }

    private void disconnect() {
        connection.disconnect();
    }

    private void setInputsEnabled(boolean enabled) {
        ipField.setDisable(!enabled);
        portField.setDisable(!enabled);
        nicknameField.setDisable(!enabled);
    }

    private void log(String message) {
        if (Platform.isFxApplicationThread()) {
            logArea.appendText(message + "\n");
        } else {
            Platform.runLater(() -> logArea.appendText(message + "\n"));
        }
    }

    @Override
    public void onMessageReceived(String message) {
        log("Received: " + message);

        if (message.startsWith(ProtocolConstants.CMD_PING)) {
            connection.sendMessage(ProtocolConstants.CMD_PONG);
        }
    }

    @Override
    public void onConnected() {
        Platform.runLater(() -> {
            connected = true;
            connectButton.setText("DISCONNECT");
            log("Connected!");
        });
    }

    @Override
    public void onDisconnected(String reason) {
        Platform.runLater(() -> {
            connected = false;
            connectButton.setText("CONNECT");
            setInputsEnabled(true);
            log("Disconnected: " + reason);
        });
    }

    @Override
    public void onError(String error) {
        log("ERROR: " + error);
    }
}
```

**Klíčové body:**
- `Platform.runLater()` pro UI updates
- Background thread pro connect
- Automatická odpověď na PING

#### 8. Vytvoř `LoginView.fxml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<?import javafx.scene.control.*?>
<?import javafx.scene.layout.*?>
<?import javafx.geometry.Insets?>

<VBox xmlns:fx="http://javafx.com/fxml/1"
      fx:controller="cz.zcu.kiv.ups.pexeso.controller.LoginController"
      spacing="15" style="-fx-background-color: #f5f5f5;">

    <padding><Insets top="20" right="20" bottom="20" left="20"/></padding>

    <Label text="Pexeso Client" style="-fx-font-size: 24; -fx-font-weight: bold;"/>

    <GridPane hgap="10" vgap="10">
        <Label text="Server IP:" GridPane.columnIndex="0" GridPane.rowIndex="0"/>
        <TextField fx:id="ipField" GridPane.columnIndex="1" GridPane.rowIndex="0"/>

        <Label text="Port:" GridPane.columnIndex="0" GridPane.rowIndex="1"/>
        <TextField fx:id="portField" GridPane.columnIndex="1" GridPane.rowIndex="1"/>

        <Label text="Nickname:" GridPane.columnIndex="0" GridPane.rowIndex="2"/>
        <TextField fx:id="nicknameField" GridPane.columnIndex="1" GridPane.rowIndex="2"/>

        <Button fx:id="connectButton" text="CONNECT" onAction="#handleConnect"
                GridPane.columnIndex="0" GridPane.rowIndex="3" GridPane.columnSpan="2"/>
    </GridPane>

    <Label text="Log:"/>
    <TextArea fx:id="logArea" editable="false" VBox.vgrow="ALWAYS"/>
</VBox>
```

#### 9. Vytvoř `Main.java`

```java
package cz.zcu.kiv.ups.pexeso;

import javafx.application.Application;
import javafx.fxml.FXMLLoader;
import javafx.scene.Scene;
import javafx.stage.Stage;

public class Main extends Application {
    @Override
    public void start(Stage stage) throws Exception {
        FXMLLoader loader = new FXMLLoader(
            getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/LoginView.fxml")
        );
        Scene scene = new Scene(loader.load(), 600, 500);

        stage.setTitle("Pexeso Client");
        stage.setScene(scene);
        stage.show();
    }

    public static void main(String[] args) {
        launch(args);
    }
}
```

#### 10. Zkompiluj a spusť

```bash
mvn clean compile
mvn javafx:run

# Připoj se k serveru z Packetu 3
```

**Výstup:** Funkční JavaFX klient s GUI

---

## PACKET 5: LOBBY A MÍSTNOSTI

### Cíl:
Implementovat správu místností na serveru

### Krok za krokem:

#### 1. Vytvoř `room.h`

*(Viz implementace v packet5.md - kompletní kód)*

**Klíčové body:**
- Struktura `room_t` s polem hráčů
- Globální pole místností s mutexem
- Funkce pro create, join, leave

#### 2. Implementuj `room.c`

*(Viz implementace v packet5.md)*

**Klíčové funkce:**
- `room_system_init()` - alokace pole
- `room_create()` - vytvoření místnosti
- `room_add_player()` - přidání hráče
- `room_remove_player()` - odebrání + auto-destroy
- `room_broadcast()` - broadcast zpráv

#### 3. Rozšiř `client_handler.c`

Přidej handlery:
- `handle_hello()`
- `handle_list_rooms()`
- `handle_create_room()`
- `handle_join_room()`
- `handle_leave_room()`

*(Viz kompletní implementace v packet5.md)*

#### 4. Aktualizuj `server.c`

Přidej do `server_init()`:
```c
room_system_init(max_rooms);
```

Přidej do `server_shutdown()`:
```c
room_system_shutdown();
```

#### 5. Aktualizuj `Makefile`

```makefile
SOURCES = main.c server.c client_handler.c logger.c room.c
```

#### 6. Zkompiluj a testuj

```bash
make clean && make
./server 127.0.0.1 10000 10 50

# Terminál 2
nc 127.0.0.1 10000
HELLO Alice
LIST_ROOMS
CREATE_ROOM MyRoom 4
```

**Výstup:** Server s funkční správou místností

---

## PACKET 6: HERNÍ LOGIKA PEXESA

### Cíl:
Implementovat kompletní herní logiku Memory (Pexeso) hry

### Co implementujeme:
- Vytváření hry s náhodným rozmístěním karet
- Otáčení karet hráčem
- Detekce shody/neshody párů
- Střídání hráčů a počítání bodů
- Detekce konce hry a vítězů

### Krok za krokem:

#### 1. Vytvoř `game.h`

```c
#ifndef GAME_H
#define GAME_H

#include "client_handler.h"

#define MIN_BOARD_SIZE 4    // 4x4 = 16 cards (8 pairs)
#define MAX_BOARD_SIZE 6    // 6x6 = 36 cards (18 pairs)

typedef enum {
    CARD_HIDDEN,     // Card is face down
    CARD_REVEALED,   // Card is face up
    CARD_MATCHED     // Card has been matched
} card_state_t;

typedef struct {
    int value;           // Card value (1-18, pairs have same value)
    card_state_t state;  // Current state
} card_t;

typedef enum {
    GAME_STATE_WAITING,   // Waiting for players to be ready
    GAME_STATE_PLAYING,   // Game in progress
    GAME_STATE_FINISHED   // Game ended
} game_state_t;

typedef struct game_s {
    int board_size;              // Board size (4, 5, or 6)
    int total_cards;             // board_size * board_size
    int total_pairs;             // total_cards / 2
    card_t *cards;               // Array of cards

    client_t *players[MAX_PLAYERS_PER_ROOM];
    int player_count;
    int player_scores[MAX_PLAYERS_PER_ROOM];
    int player_ready[MAX_PLAYERS_PER_ROOM];

    int current_player_index;    // Index of player whose turn it is
    int first_card_index;        // First flipped card (-1 if none)
    int second_card_index;       // Second flipped card (-1 if none)
    int flips_this_turn;         // 0, 1, or 2

    int matched_pairs;           // Number of pairs matched
    game_state_t state;
} game_t;

// Function declarations
game_t* game_create(int board_size, client_t **players, int player_count);
void game_destroy(game_t *game);
int game_player_ready(game_t *game, client_t *client);
int game_all_players_ready(game_t *game);
int game_start(game_t *game);
int game_flip_card(game_t *game, client_t *client, int card_index);
int game_check_match(game_t *game);
client_t* game_get_current_player(game_t *game);
int game_is_finished(game_t *game);
int game_get_winners(game_t *game, client_t **winners);
int game_format_start_message(game_t *game, char *buffer, int buffer_size);
int game_format_state_message(game_t *game, char *buffer, int buffer_size);

#endif // GAME_H
```

**Klíčové body:**
- `card_t` - jednotlivá karta (hodnota + stav)
- `game_t` - celá hra (karty, hráči, skóre, aktuální tah)
- Fisher-Yates shuffle pro náhodné rozmístění

#### 2. Implementuj `game.c` - Vytvoření hry

```c
#include "game.h"
#include "logger.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Fisher-Yates shuffle
static void shuffle_array(int *array, int size) {
    for (int i = size - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        int temp = array[i];
        array[i] = array[j];
        array[j] = temp;
    }
}

game_t* game_create(int board_size, client_t **players, int player_count) {
    if (board_size < MIN_BOARD_SIZE || board_size > MAX_BOARD_SIZE) {
        logger_log(LOG_ERROR, "Invalid board size: %d", board_size);
        return NULL;
    }

    game_t *game = (game_t *)calloc(1, sizeof(game_t));
    if (game == NULL) {
        logger_log(LOG_ERROR, "Failed to allocate memory for game");
        return NULL;
    }

    // Initialize basic properties
    game->board_size = board_size;
    game->total_cards = board_size * board_size;
    game->total_pairs = game->total_cards / 2;
    game->player_count = player_count;
    game->current_player_index = 0;
    game->first_card_index = -1;
    game->second_card_index = -1;
    game->flips_this_turn = 0;
    game->matched_pairs = 0;
    game->state = GAME_STATE_WAITING;

    // Copy players
    for (int i = 0; i < player_count; i++) {
        game->players[i] = players[i];
        game->player_scores[i] = 0;
        game->player_ready[i] = 0;
    }

    // Allocate cards
    game->cards = (card_t *)calloc(game->total_cards, sizeof(card_t));
    if (game->cards == NULL) {
        logger_log(LOG_ERROR, "Failed to allocate memory for cards");
        free(game);
        return NULL;
    }

    // Create pairs: [1,1,2,2,3,3,...]
    int *values = (int *)malloc(game->total_cards * sizeof(int));
    for (int i = 0; i < game->total_pairs; i++) {
        values[i * 2] = i + 1;
        values[i * 2 + 1] = i + 1;
    }

    // Shuffle
    srand(time(NULL));
    shuffle_array(values, game->total_cards);

    // Assign to cards
    for (int i = 0; i < game->total_cards; i++) {
        game->cards[i].value = values[i];
        game->cards[i].state = CARD_HIDDEN;
    }

    free(values);
    logger_log(LOG_INFO, "Game created: board=%d, cards=%d, pairs=%d",
               board_size, game->total_cards, game->total_pairs);

    return game;
}

void game_destroy(game_t *game) {
    if (game == NULL) return;
    if (game->cards != NULL) free(game->cards);
    free(game);
    logger_log(LOG_INFO, "Game destroyed");
}
```

#### 3. Implementuj `game.c` - Logika otáčení karet

```c
int game_flip_card(game_t *game, client_t *client, int card_index) {
    if (game == NULL || client == NULL) return -1;

    // Check game is playing
    if (game->state != GAME_STATE_PLAYING) {
        logger_log(LOG_WARNING, "Cannot flip, game not playing");
        return -1;
    }

    // Check it's player's turn
    if (game->players[game->current_player_index] != client) {
        logger_log(LOG_WARNING, "Not %s's turn", client->nickname);
        return -1;
    }

    // Check not already flipped 2 cards
    if (game->flips_this_turn >= 2) {
        logger_log(LOG_WARNING, "%s already flipped 2 cards", client->nickname);
        return -1;
    }

    // Validate card index
    if (card_index < 0 || card_index >= game->total_cards) {
        logger_log(LOG_WARNING, "Invalid card index: %d", card_index);
        return -1;
    }

    // Check card is hidden
    if (game->cards[card_index].state != CARD_HIDDEN) {
        logger_log(LOG_WARNING, "Card %d already revealed/matched", card_index);
        return -1;
    }

    // Reveal the card
    game->cards[card_index].state = CARD_REVEALED;
    game->flips_this_turn++;

    logger_log(LOG_INFO, "%s flipped card %d (value=%d), flip %d/2",
               client->nickname, card_index, game->cards[card_index].value,
               game->flips_this_turn);

    // Store indices
    if (game->flips_this_turn == 1) {
        game->first_card_index = card_index;
    } else {
        game->second_card_index = card_index;
    }

    return 0;
}

int game_check_match(game_t *game) {
    if (game == NULL || game->flips_this_turn != 2) return 0;

    int first_value = game->cards[game->first_card_index].value;
    int second_value = game->cards[game->second_card_index].value;

    if (first_value == second_value) {
        // MATCH!
        game->cards[game->first_card_index].state = CARD_MATCHED;
        game->cards[game->second_card_index].state = CARD_MATCHED;
        game->player_scores[game->current_player_index]++;
        game->matched_pairs++;

        logger_log(LOG_INFO, "MATCH! %s matched cards %d and %d, score=%d",
                   game->players[game->current_player_index]->nickname,
                   game->first_card_index, game->second_card_index,
                   game->player_scores[game->current_player_index]);

        // Reset for next turn (same player continues)
        game->first_card_index = -1;
        game->second_card_index = -1;
        game->flips_this_turn = 0;

        // Check if game finished
        if (game->matched_pairs == game->total_pairs) {
            game->state = GAME_STATE_FINISHED;
            logger_log(LOG_INFO, "Game finished! All pairs matched");
        }

        return 1; // Match
    } else {
        // MISMATCH
        game->cards[game->first_card_index].state = CARD_HIDDEN;
        game->cards[game->second_card_index].state = CARD_HIDDEN;

        logger_log(LOG_INFO, "MISMATCH! %s cards %d and %d",
                   game->players[game->current_player_index]->nickname,
                   game->first_card_index, game->second_card_index);

        // Move to next player
        game->current_player_index = (game->current_player_index + 1) % game->player_count;

        logger_log(LOG_INFO, "Turn passed to %s",
                   game->players[game->current_player_index]->nickname);

        // Reset
        game->first_card_index = -1;
        game->second_card_index = -1;
        game->flips_this_turn = 0;

        return 0; // Mismatch
    }
}
```

#### 4. Implementuj pomocné funkce v `game.c`

```c
int game_player_ready(game_t *game, client_t *client) {
    if (game == NULL || client == NULL) return -1;
    if (game->state != GAME_STATE_WAITING) return -1;

    for (int i = 0; i < game->player_count; i++) {
        if (game->players[i] == client) {
            game->player_ready[i] = 1;
            logger_log(LOG_INFO, "Player %s ready", client->nickname);
            return 0;
        }
    }
    return -1;
}

int game_all_players_ready(game_t *game) {
    if (game == NULL) return 0;
    for (int i = 0; i < game->player_count; i++) {
        if (game->player_ready[i] == 0) return 0;
    }
    return 1;
}

int game_start(game_t *game) {
    if (game == NULL || game->state != GAME_STATE_WAITING) return -1;
    game->state = GAME_STATE_PLAYING;
    game->current_player_index = 0;
    logger_log(LOG_INFO, "Game started, first player: %s",
               game->players[0]->nickname);
    return 0;
}

client_t* game_get_current_player(game_t *game) {
    if (game == NULL || game->state != GAME_STATE_PLAYING) return NULL;
    return game->players[game->current_player_index];
}

int game_is_finished(game_t *game) {
    if (game == NULL) return 0;
    return (game->state == GAME_STATE_FINISHED);
}

int game_get_winners(game_t *game, client_t **winners) {
    if (game == NULL || winners == NULL) return 0;

    // Find max score
    int max_score = 0;
    for (int i = 0; i < game->player_count; i++) {
        if (game->player_scores[i] > max_score) {
            max_score = game->player_scores[i];
        }
    }

    // Collect all with max score (handles ties)
    int winner_count = 0;
    for (int i = 0; i < game->player_count; i++) {
        if (game->player_scores[i] == max_score) {
            winners[winner_count++] = game->players[i];
        }
    }

    logger_log(LOG_INFO, "Winners: %d player(s) with score %d",
               winner_count, max_score);
    return winner_count;
}

int game_format_start_message(game_t *game, char *buffer, int buffer_size) {
    if (game == NULL || buffer == NULL) return -1;

    int offset = snprintf(buffer, buffer_size, "GAME_START %d", game->board_size);
    for (int i = 0; i < game->player_count; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, " %s",
                           game->players[i]->nickname);
    }
    return 0;
}

int game_format_state_message(game_t *game, char *buffer, int buffer_size) {
    if (game == NULL || buffer == NULL) return -1;

    int offset = snprintf(buffer, buffer_size, "GAME_STATE %d %s",
                          game->board_size,
                          game->players[game->current_player_index]->nickname);

    // Add scores
    for (int i = 0; i < game->player_count; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, " %d",
                           game->player_scores[i]);
    }

    // Add card states (0=hidden, 1=matched)
    for (int i = 0; i < game->total_cards; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset, " %d",
                           (game->cards[i].state == CARD_MATCHED) ? 1 : 0);
    }
    return 0;
}
```

#### 5. Aktualizuj `protocol.h`

Přidej nové příkazy:
```c
// Client → Server
#define CMD_READY "READY"
#define CMD_START_GAME "START_GAME"
#define CMD_FLIP "FLIP"

// Server → Client
#define CMD_PLAYER_READY "PLAYER_READY"
#define CMD_GAME_START "GAME_START"
#define CMD_CARD_REVEAL "CARD_REVEAL"
#define CMD_MATCH "MATCH"
#define CMD_MISMATCH "MISMATCH"
#define CMD_YOUR_TURN "YOUR_TURN"
#define CMD_GAME_END "GAME_END"
```

#### 6. Aktualizuj `room.h`

```c
// Forward declaration
struct game_s;

typedef struct room_s {
    // ... existující fieldy ...
    struct game_s *game;  // Game instance (NULL if no game)
} room_t;
```

#### 7. Aktualizuj `room.c`

V `room_create()`:
```c
room->game = NULL;  // No game initially
```

#### 8. Přidej handlery do `client_handler.c`

Na začátek souboru:
```c
#include "game.h"

// Forward declarations
static void handle_ready(client_t *client);
static void handle_start_game(client_t *client);
static void handle_flip(client_t *client, const char *params);
```

Do `handle_message()` přidej:
```c
// V sekci s parametry
else if (strcmp(command, CMD_FLIP) == 0) {
    handle_flip(client, params);
}

// V sekci bez parametrů
else if (strcmp(command, CMD_READY) == 0) {
    handle_ready(client);
} else if (strcmp(command, CMD_START_GAME) == 0) {
    handle_start_game(client);
}
```

Implementace `handle_start_game()`:
```c
static void handle_start_game(client_t *client) {
    if (client->room == NULL) {
        client_send_message(client, "ERROR NOT_IN_ROOM");
        return;
    }

    room_t *room = client->room;

    // Only owner can start
    if (room->owner != client) {
        client_send_message(client, "ERROR NOT_ROOM_OWNER");
        return;
    }

    // Check game doesn't exist
    if (room->game != NULL) {
        client_send_message(client, "ERROR Game already started");
        return;
    }

    // Need at least 2 players
    if (room->player_count < 2) {
        client_send_message(client, "ERROR NEED_MORE_PLAYERS");
        return;
    }

    // Create game (4x4 board)
    int board_size = 4;
    room->game = game_create(board_size, room->players, room->player_count);

    if (room->game == NULL) {
        client_send_message(client, "ERROR Failed to create game");
        return;
    }

    // Broadcast to all
    char broadcast[MAX_MESSAGE_LENGTH];
    snprintf(broadcast, sizeof(broadcast),
             "GAME_CREATED %d Send READY when prepared", board_size);
    room_broadcast(room, broadcast);

    logger_log(LOG_INFO, "Room %d: Game created by %s",
               room->room_id, client->nickname);
}
```

Implementace `handle_ready()`:
```c
static void handle_ready(client_t *client) {
    if (client->room == NULL) {
        client_send_message(client, "ERROR NOT_IN_ROOM");
        return;
    }

    room_t *room = client->room;

    if (room->game == NULL) {
        client_send_message(client, "ERROR GAME_NOT_STARTED");
        return;
    }

    game_t *game = (game_t *)room->game;

    if (game_player_ready(game, client) != 0) {
        client_send_message(client, "ERROR Already ready");
        return;
    }

    // Confirm
    client_send_message(client, "READY_OK");

    // Broadcast
    char broadcast[MAX_MESSAGE_LENGTH];
    snprintf(broadcast, sizeof(broadcast), "PLAYER_READY %s", client->nickname);
    room_broadcast(room, broadcast);

    // Check if all ready
    if (game_all_players_ready(game)) {
        logger_log(LOG_INFO, "All players ready, starting game");

        game_start(game);
        room->state = ROOM_STATE_PLAYING;

        // Send GAME_START to all
        char start_msg[MAX_MESSAGE_LENGTH];
        game_format_start_message(game, start_msg, sizeof(start_msg));
        room_broadcast(room, start_msg);

        // Send YOUR_TURN to first player
        client_t *first = game_get_current_player(game);
        if (first != NULL) {
            client_send_message(first, "YOUR_TURN");
        }
    }
}
```

Implementace `handle_flip()`:
```c
static void handle_flip(client_t *client, const char *params) {
    if (client->room == NULL) {
        client_send_message(client, "ERROR NOT_IN_ROOM");
        return;
    }

    room_t *room = client->room;

    if (room->game == NULL) {
        client_send_message(client, "ERROR GAME_NOT_STARTED");
        return;
    }

    game_t *game = (game_t *)room->game;

    if (params == NULL) {
        client_send_message(client, "ERROR INVALID_PARAMS");
        return;
    }

    int card_index = atoi(params);

    // Attempt flip
    if (game_flip_card(game, client, card_index) != 0) {
        client_send_message(client, "ERROR INVALID_CARD");
        return;
    }

    // Broadcast CARD_REVEAL
    char reveal[MAX_MESSAGE_LENGTH];
    snprintf(reveal, sizeof(reveal), "CARD_REVEAL %d %d %s",
             card_index,
             game->cards[card_index].value,
             client->nickname);
    room_broadcast(room, reveal);

    // If second card, check match
    if (game->flips_this_turn == 2) {
        int is_match = game_check_match(game);

        if (is_match) {
            // MATCH!
            char match_msg[MAX_MESSAGE_LENGTH];
            snprintf(match_msg, sizeof(match_msg), "MATCH %s %d",
                     client->nickname,
                     game->player_scores[game->current_player_index]);
            room_broadcast(room, match_msg);

            // Check if finished
            if (game_is_finished(game)) {
                client_t *winners[MAX_PLAYERS_PER_ROOM];
                int winner_count = game_get_winners(game, winners);

                // Send GAME_END
                char end_msg[MAX_MESSAGE_LENGTH];
                int offset = snprintf(end_msg, sizeof(end_msg), "GAME_END");
                for (int i = 0; i < winner_count; i++) {
                    offset += snprintf(end_msg + offset, sizeof(end_msg) - offset,
                                       " %s %d", winners[i]->nickname,
                                       game->player_scores[i]);
                }
                room_broadcast(room, end_msg);

                // Cleanup
                game_destroy(game);
                room->game = NULL;
                room->state = ROOM_STATE_WAITING;
            } else {
                // Same player continues
                client_send_message(client, "YOUR_TURN");
            }
        } else {
            // MISMATCH
            room_broadcast(room, "MISMATCH");

            // Next player
            client_t *next = game_get_current_player(game);
            if (next != NULL) {
                client_send_message(next, "YOUR_TURN");
            }
        }
    }
}
```

#### 9. Aktualizuj `Makefile`

```makefile
SOURCES = main.c server.c client_handler.c logger.c room.c game.c

game.o: game.c game.h client_handler.h logger.h protocol.h
client_handler.o: client_handler.c client_handler.h room.h game.h logger.h protocol.h
```

#### 10. Zkompiluj a testuj

```bash
make clean && make
./server 127.0.0.1 10000 10 50
```

**Test herního flow:**

Terminál 2 (Alice):
```
nc 127.0.0.1 10000
HELLO Alice
CREATE_ROOM GameRoom 2
START_GAME
READY
```

Terminál 3 (Bob):
```
nc 127.0.0.1 10000
HELLO Bob
JOIN_ROOM 1
READY
```

Po tom, co jsou oba ready, Alice dostane `YOUR_TURN`.

Alice hraje:
```
FLIP 0
```
→ All: `CARD_REVEAL 0 3 Alice`

```
FLIP 5
```
→ All: `CARD_REVEAL 5 3 Alice`
→ All: `MATCH Alice 1`
→ Alice: `YOUR_TURN`

Alice zkusí další:
```
FLIP 1
FLIP 2
```
→ All: `CARD_REVEAL 1 7 Alice`
→ All: `CARD_REVEAL 2 2 Alice`
→ All: `MISMATCH`
→ Bob: `YOUR_TURN`

**Výstup:** Plně funkční Pexeso hra!

---

## PACKET 7: JAVAFX GUI - LOBBY A HRA

### Cíl:
Implementovat kompletní JavaFX GUI s lobby, místnostmi a herním boardem

### Co implementujeme:
- Přepínání mezi scénami (Login → Lobby → Game)
- LobbyView s výpisem místností
- GameView s grid kartiček
- Asynchronní zpracování zpráv ze serveru

### Krok za krokem:

#### 1. Vytvoř model Room.java

```java
package cz.zcu.kiv.ups.pexeso.model;

public class Room {
    private final int id;
    private final String name;
    private final int currentPlayers;
    private final int maxPlayers;
    private final String state;

    public Room(int id, String name, int currentPlayers, int maxPlayers, String state) {
        this.id = id;
        this.name = name;
        this.currentPlayers = currentPlayers;
        this.maxPlayers = maxPlayers;
        this.state = state;
    }

    public int getId() { return id; }
    public String getName() { return name; }
    public int getCurrentPlayers() { return currentPlayers; }
    public int getMaxPlayers() { return maxPlayers; }
    public String getState() { return state; }

    @Override
    public String toString() {
        return String.format("%s (%d/%d) - %s", name, currentPlayers, maxPlayers, state);
    }
}
```

**Klíčové body:**
- Immutable data class
- toString() pro zobrazení v ListView

#### 2. Aktualizuj LoginController.java

Přidej do existujícího LoginController:

```java
private Stage stage;
private String authenticatedNickname;

public void setStage(Stage stage) {
    this.stage = stage;
}

@Override
public void onMessageReceived(String message) {
    log("Received: " + message);

    if (message.startsWith(ProtocolConstants.CMD_WELCOME)) {
        log("Successfully authenticated!");
        authenticatedNickname = nicknameField.getText().trim();
        switchToLobby();
    } else if (message.startsWith(ProtocolConstants.CMD_ERROR)) {
        log("Server error: " + message);
    } else if (message.startsWith(ProtocolConstants.CMD_PING)) {
        connection.sendMessage(ProtocolConstants.CMD_PONG);
        log("Sent: " + ProtocolConstants.CMD_PONG);
    }
}

private void switchToLobby() {
    Platform.runLater(() -> {
        try {
            FXMLLoader loader = new FXMLLoader(
                getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/LobbyView.fxml")
            );
            Parent root = loader.load();

            LobbyController lobbyController = loader.getController();
            lobbyController.setConnection(connection, authenticatedNickname);
            lobbyController.setStage(stage);

            Scene scene = new Scene(root, 800, 600);
            stage.setScene(scene);
            stage.setTitle("Pexeso - Lobby");
        } catch (IOException e) {
            e.printStackTrace();
            log("ERROR: Failed to load lobby view: " + e.getMessage());
        }
    });
}
```

**Klíčové body:**
- Automatický přechod na WELCOME
- Předání connection a nickname do LobbyController

#### 3. Vytvoř LobbyController.java

```java
package cz.zcu.kiv.ups.pexeso.controller;

import cz.zcu.kiv.ups.pexeso.model.Room;
import cz.zcu.kiv.ups.pexeso.network.ClientConnection;
import cz.zcu.kiv.ups.pexeso.network.MessageListener;
import javafx.application.Platform;
import javafx.collections.FXCollections;
import javafx.collections.ObservableList;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.stage.Stage;

import java.io.IOException;
import java.util.Optional;

public class LobbyController implements MessageListener {

    @FXML private Label statusLabel;
    @FXML private ListView<Room> roomListView;
    @FXML private Button createRoomButton;
    @FXML private Button joinRoomButton;
    @FXML private Button refreshButton;

    private ClientConnection connection;
    private String nickname;
    private Stage stage;
    private ObservableList<Room> rooms = FXCollections.observableArrayList();

    @FXML
    public void initialize() {
        roomListView.setItems(rooms);
        updateStatus("Welcome to Pexeso lobby!");
    }

    public void setConnection(ClientConnection connection, String nickname) {
        this.connection = connection;
        this.nickname = nickname;
        this.connection.setMessageListener(this);
        refreshRooms();
    }

    public void setStage(Stage stage) {
        this.stage = stage;
    }

    @FXML
    private void handleCreateRoom() {
        // Dialog pro vytvoření místnosti
        Dialog<ButtonType> dialog = new Dialog<>();
        dialog.setTitle("Create Room");
        dialog.setHeaderText("Create a new game room");

        TextField nameField = new TextField("Room" + (int)(Math.random() * 1000));
        TextField maxPlayersField = new TextField("2");
        TextField boardSizeField = new TextField("4");

        GridPane grid = new GridPane();
        grid.add(new Label("Room Name:"), 0, 0);
        grid.add(nameField, 1, 0);
        grid.add(new Label("Max Players (2-4):"), 0, 1);
        grid.add(maxPlayersField, 1, 1);
        grid.add(new Label("Board Size (4-6):"), 0, 2);
        grid.add(boardSizeField, 1, 2);

        dialog.getDialogPane().setContent(grid);
        dialog.getDialogPane().getButtonTypes().addAll(ButtonType.OK, ButtonType.CANCEL);

        Optional<ButtonType> result = dialog.showAndWait();
        if (result.isPresent() && result.get() == ButtonType.OK) {
            String name = nameField.getText().trim();
            int maxPlayers = Integer.parseInt(maxPlayersField.getText());
            int boardSize = Integer.parseInt(boardSizeField.getText());

            connection.sendMessage("CREATE_ROOM " + name + " " + maxPlayers + " " + boardSize);
            updateStatus("Creating room...");
        }
    }

    @FXML
    private void handleJoinRoom() {
        Room selectedRoom = roomListView.getSelectionModel().getSelectedItem();
        if (selectedRoom == null) {
            showError("No Room Selected", "Please select a room to join");
            return;
        }

        connection.sendMessage("JOIN_ROOM " + selectedRoom.getId());
        updateStatus("Joining room...");
    }

    @FXML
    private void handleRefresh() {
        refreshRooms();
    }

    private void refreshRooms() {
        connection.sendMessage("LIST_ROOMS");
        updateStatus("Refreshing room list...");
    }

    private void updateStatus(String message) {
        Platform.runLater(() -> statusLabel.setText(message));
    }

    private void showError(String title, String message) {
        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.ERROR);
            alert.setTitle(title);
            alert.setHeaderText(null);
            alert.setContentText(message);
            alert.showAndWait();
        });
    }

    @Override
    public void onMessageReceived(String message) {
        String[] parts = message.split(" ", 2);
        String command = parts[0];

        switch (command) {
            case "ROOM_LIST":
                handleRoomList(message);
                break;
            case "ROOM_CREATED":
                handleRoomCreated(message);
                break;
            case "ROOM_JOINED":
                handleRoomJoined(message);
                break;
            case "ERROR":
                handleError(message);
                break;
        }
    }

    private void handleRoomList(String message) {
        // Format: ROOM_LIST <count> [<id> <name> <current> <max> <state>]*
        String[] parts = message.split(" ");
        if (parts.length < 2) return;

        int count = Integer.parseInt(parts[1]);

        Platform.runLater(() -> {
            rooms.clear();

            if (count == 0) {
                updateStatus("No active rooms available. Create one!");
                return;
            }

            int index = 2;
            int activeRooms = 0;
            for (int i = 0; i < count; i++) {
                if (index + 4 >= parts.length) break;

                try {
                    int id = Integer.parseInt(parts[index++]);
                    String name = parts[index++];
                    int current = Integer.parseInt(parts[index++]);
                    int max = Integer.parseInt(parts[index++]);
                    String state = parts[index++];

                    // Skip finished games
                    if (!"FINISHED".equals(state)) {
                        rooms.add(new Room(id, name, current, max, state));
                        activeRooms++;
                    }
                } catch (Exception e) {
                    System.err.println("Error parsing room: " + e.getMessage());
                }
            }

            if (activeRooms == 0) {
                updateStatus("No active rooms available. Create one!");
            } else {
                updateStatus(String.format("Found %d active room(s)", activeRooms));
            }
        });
    }

    private void handleRoomCreated(String message) {
        // Format: ROOM_CREATED <id> <name>
        String[] parts = message.split(" ");
        if (parts.length < 3) return;

        int roomId = Integer.parseInt(parts[1]);
        String roomName = parts[2];

        Platform.runLater(() -> {
            updateStatus("Room created: " + roomName);
            switchToGameView(roomId, roomName, true);
        });
    }

    private void handleRoomJoined(String message) {
        // Format: ROOM_JOINED <id> <name>
        String[] parts = message.split(" ");
        if (parts.length < 3) return;

        int roomId = Integer.parseInt(parts[1]);
        String roomName = parts[2];

        Platform.runLater(() -> {
            updateStatus("Joined: " + roomName);
            switchToGameView(roomId, roomName, false);
        });
    }

    private void handleError(String message) {
        String error = message.substring(6);
        showError("Error", error);
    }

    private void switchToGameView(int roomId, String roomName, boolean isOwner) {
        Platform.runLater(() -> {
            try {
                FXMLLoader loader = new FXMLLoader(
                    getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/GameView.fxml")
                );
                Parent root = loader.load();

                GameController gameController = loader.getController();
                gameController.setConnection(connection, nickname);
                gameController.setRoomInfo(roomId, roomName, isOwner);
                gameController.setStage(stage);

                Scene scene = new Scene(root, 800, 600);
                stage.setScene(scene);
                stage.setTitle("Pexeso - " + roomName);
            } catch (IOException e) {
                e.printStackTrace();
                showError("Error", "Failed to load game view: " + e.getMessage());
            }
        });
    }

    @Override
    public void onConnected() { }

    @Override
    public void onDisconnected(String reason) {
        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.ERROR);
            alert.setTitle("Disconnected");
            alert.setHeaderText("Lost connection to server");
            alert.setContentText(reason);
            alert.showAndWait();
            System.exit(0);
        });
    }

    @Override
    public void onError(String error) {
        updateStatus("Error: " + error);
    }
}
```

**Klíčové body:**
- ObservableList pro ListView
- Dialog pro vytvoření místnosti
- Filtrování FINISHED rooms
- Scene switching na GameView

#### 4. Vytvoř LobbyView.fxml

Vytvoř soubor: `src/main/resources/cz/zcu/kiv/ups/pexeso/ui/LobbyView.fxml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<?import javafx.scene.control.*?>
<?import javafx.scene.layout.*?>
<?import javafx.geometry.Insets?>

<BorderPane xmlns:fx="http://javafx.com/fxml/1"
            fx:controller="cz.zcu.kiv.ups.pexeso.controller.LobbyController"
            style="-fx-background-color: #f5f5f5;">

    <top>
        <VBox spacing="10">
            <padding><Insets top="10" right="10" bottom="10" left="10"/></padding>
            <Label text="Pexeso - Lobby" style="-fx-font-size: 24; -fx-font-weight: bold;"/>
            <Label fx:id="statusLabel" text="Loading..."/>
        </VBox>
    </top>

    <center>
        <ListView fx:id="roomListView">
            <padding><Insets top="10" right="10" bottom="10" left="10"/></padding>
        </ListView>
    </center>

    <right>
        <VBox spacing="10">
            <padding><Insets top="10" right="10" bottom="10" left="10"/></padding>
            <Button fx:id="createRoomButton" text="Create Room" onAction="#handleCreateRoom"
                    prefWidth="120"/>
            <Button fx:id="joinRoomButton" text="Join Room" onAction="#handleJoinRoom"
                    prefWidth="120"/>
            <Button fx:id="refreshButton" text="Refresh" onAction="#handleRefresh"
                    prefWidth="120"/>
        </VBox>
    </right>

</BorderPane>
```

#### 5. Vytvoř GameController.java (část 1 - struktura)

```java
package cz.zcu.kiv.ups.pexeso.controller;

import cz.zcu.kiv.ups.pexeso.network.ClientConnection;
import cz.zcu.kiv.ups.pexeso.network.MessageListener;
import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.fxml.FXMLLoader;
import javafx.geometry.Pos;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.*;
import javafx.scene.layout.GridPane;
import javafx.scene.layout.VBox;
import javafx.stage.Stage;

import java.io.IOException;
import java.util.*;

public class GameController implements MessageListener {

    @FXML private Label roomNameLabel;
    @FXML private Label statusLabel;
    @FXML private Label turnLabel;
    @FXML private VBox playersVBox;
    @FXML private GridPane cardGrid;
    @FXML private Button startGameButton;
    @FXML private Button readyButton;
    @FXML private Button leaveRoomButton;

    private ClientConnection connection;
    private String nickname;
    private int roomId;
    private String roomName;
    private boolean isOwner;
    private Stage stage;

    // Game state
    private int boardSize = 0;
    private Button[] cardButtons;
    private boolean[] cardMatched;
    private int[] cardValues;
    private boolean myTurn = false;
    private int flippedThisTurn = 0;
    private List<Integer> flippedIndices = new ArrayList<>();

    private Map<String, Integer> playerScores = new LinkedHashMap<>();
    private List<String> players = new ArrayList<>();
    private boolean gameStarted = false;
    private boolean isReady = false;

    public void initialize() {
        startGameButton.setVisible(false);
        readyButton.setVisible(false);
        cardGrid.setVisible(false);
    }

    public void setConnection(ClientConnection connection, String nickname) {
        this.connection = connection;
        this.nickname = nickname;
        this.connection.setMessageListener(this);
    }

    public void setRoomInfo(int roomId, String roomName, boolean isOwner) {
        this.roomId = roomId;
        this.roomName = roomName;
        this.isOwner = isOwner;

        roomNameLabel.setText("Room: " + roomName);
        startGameButton.setVisible(isOwner);

        updateStatus("Waiting for players...");
    }

    public void setStage(Stage stage) {
        this.stage = stage;
    }

    // ... (pokračování v další části)
```

#### 6. Vytvoř GameController.java (část 2 - event handlery)

```java
    @FXML
    private void handleStartGame() {
        if (!isOwner) return;
        connection.sendMessage("START_GAME");
        updateStatus("Creating game...");
        startGameButton.setDisable(true);
    }

    @FXML
    private void handleReady() {
        if (isReady) return;
        connection.sendMessage("READY");
        readyButton.setDisable(true);
        isReady = true;
        updateStatus("Ready! Waiting for others...");
    }

    @FXML
    private void handleLeaveRoom() {
        Alert confirm = new Alert(Alert.AlertType.CONFIRMATION);
        confirm.setTitle("Leave Room");
        confirm.setHeaderText("Are you sure you want to leave?");
        confirm.setContentText("The game will end if you leave.");

        Optional<ButtonType> result = confirm.showAndWait();
        if (result.isPresent() && result.get() == ButtonType.OK) {
            connection.sendMessage("LEAVE_ROOM");
        }
    }

    private void updateStatus(String message) {
        Platform.runLater(() -> statusLabel.setText(message));
    }

    private void updateTurnLabel(String player) {
        Platform.runLater(() -> {
            if (player.equals(nickname)) {
                turnLabel.setText("YOUR TURN!");
                turnLabel.setStyle("-fx-font-size: 18px; -fx-font-weight: bold; -fx-text-fill: green;");
                myTurn = true;
            } else {
                turnLabel.setText("Turn: " + player);
                turnLabel.setStyle("-fx-font-size: 16px; -fx-font-weight: normal; -fx-text-fill: black;");
                myTurn = false;
            }
        });
    }

    private void updatePlayers() {
        Platform.runLater(() -> {
            playersVBox.getChildren().clear();

            Label title = new Label("Players:");
            title.setStyle("-fx-font-weight: bold;");
            playersVBox.getChildren().add(title);

            for (String player : players) {
                Integer score = playerScores.getOrDefault(player, 0);
                Label playerLabel = new Label(String.format("%s: %d", player, score));

                if (player.equals(nickname)) {
                    playerLabel.setStyle("-fx-font-weight: bold; -fx-text-fill: blue;");
                }

                playersVBox.getChildren().add(playerLabel);
            }
        });
    }
```

#### 7. Vytvoř GameController.java (část 3 - herní logika)

```java
    private void initializeBoard(int size) {
        this.boardSize = size;
        int totalCards = size * size;
        cardButtons = new Button[totalCards];
        cardMatched = new boolean[totalCards];
        cardValues = new int[totalCards];
        Arrays.fill(cardMatched, false);
        Arrays.fill(cardValues, -1);

        Platform.runLater(() -> {
            cardGrid.getChildren().clear();
            cardGrid.setVisible(true);
            cardGrid.setHgap(5);
            cardGrid.setVgap(5);
            cardGrid.setAlignment(Pos.CENTER);

            for (int i = 0; i < totalCards; i++) {
                final int index = i;
                Button btn = new Button("?");
                btn.setPrefSize(80, 80);
                btn.setStyle("-fx-font-size: 24px; -fx-font-weight: bold;");
                btn.setOnAction(e -> handleCardClick(index));

                cardButtons[i] = btn;
                cardGrid.add(btn, i % size, i / size);
            }
        });
    }

    private void handleCardClick(int index) {
        if (!myTurn) {
            showAlert("Not your turn!");
            return;
        }

        if (cardMatched[index]) {
            showAlert("Card already matched!");
            return;
        }

        if (flippedIndices.contains(index)) {
            showAlert("Card already flipped this turn!");
            return;
        }

        if (flippedThisTurn >= 2) {
            showAlert("Already flipped 2 cards!");
            return;
        }

        connection.sendMessage("FLIP " + index);
    }

    private void revealCard(int index, int value, String playerName) {
        Platform.runLater(() -> {
            if (index < 0 || index >= cardButtons.length) return;

            cardValues[index] = value;
            cardButtons[index].setText(String.valueOf(value));
            cardButtons[index].setStyle("-fx-font-size: 24px; -fx-font-weight: bold; " +
                                        "-fx-background-color: #FFC107;");

            flippedIndices.add(index);
            flippedThisTurn++;
        });
    }

    private void handleMatch(String playerName, int score) {
        Platform.runLater(() -> {
            for (int index : flippedIndices) {
                cardMatched[index] = true;
                cardButtons[index].setStyle("-fx-font-size: 24px; -fx-font-weight: bold; " +
                                           "-fx-background-color: #4CAF50; -fx-text-fill: white;");
                cardButtons[index].setDisable(true);
            }

            playerScores.put(playerName, score);
            updatePlayers();

            flippedIndices.clear();
            flippedThisTurn = 0;

            updateStatus(playerName + " found a match!");
        });
    }

    private void handleMismatch() {
        Platform.runLater(() -> {
            // Clear turn state immediately
            myTurn = false;
            turnLabel.setText("Waiting for other player...");
            turnLabel.setStyle("-fx-font-size: 16px; -fx-font-weight: normal; -fx-text-fill: gray;");

            new Thread(() -> {
                try {
                    Thread.sleep(1000); // Show cards for 1 second
                } catch (InterruptedException e) {
                    e.printStackTrace();
                }

                Platform.runLater(() -> {
                    for (int index : flippedIndices) {
                        if (!cardMatched[index]) {
                            cardButtons[index].setText("?");
                            cardButtons[index].setStyle("-fx-font-size: 24px; -fx-font-weight: bold;");
                        }
                    }

                    flippedIndices.clear();
                    flippedThisTurn = 0;
                });
            }).start();

            updateStatus("No match! Next player's turn.");
        });
    }

    private void showAlert(String message) {
        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.INFORMATION);
            alert.setTitle("Info");
            alert.setHeaderText(null);
            alert.setContentText(message);
            alert.show();
        });
    }
```

#### 8. Vytvoř GameController.java (část 4 - message handling)

```java
    @Override
    public void onMessageReceived(String message) {
        String[] parts = message.split(" ");
        String command = parts[0];

        switch (command) {
            case "GAME_CREATED":
                if (parts.length > 1) {
                    try {
                        boardSize = Integer.parseInt(parts[1]);
                        Platform.runLater(() -> {
                            readyButton.setVisible(true);
                            startGameButton.setVisible(false);
                        });
                        updateStatus("Game created! Click READY when prepared.");
                    } catch (NumberFormatException e) {
                        e.printStackTrace();
                    }
                }
                break;

            case "PLAYER_READY":
                if (parts.length > 1) {
                    String player = parts[1];
                    updateStatus(player + " is ready");
                }
                break;

            case "GAME_START":
                handleGameStart(message);
                break;

            case "YOUR_TURN":
                updateTurnLabel(nickname);
                updateStatus("Your turn! Click on a card.");
                break;

            case "CARD_REVEAL":
                if (parts.length >= 4) {
                    try {
                        int index = Integer.parseInt(parts[1]);
                        int value = Integer.parseInt(parts[2]);
                        String player = parts[3];
                        revealCard(index, value, player);
                    } catch (NumberFormatException e) {
                        e.printStackTrace();
                    }
                }
                break;

            case "MATCH":
                if (parts.length >= 3) {
                    String player = parts[1];
                    int score = Integer.parseInt(parts[2]);
                    handleMatch(player, score);
                }
                break;

            case "MISMATCH":
                handleMismatch();
                break;

            case "GAME_END":
                handleGameEnd(message);
                break;

            case "LEFT_ROOM":
                returnToLobby();
                break;

            case "ERROR":
                String error = message.substring(6);
                updateStatus("Error: " + error);
                break;
        }
    }

    private void handleGameStart(String message) {
        // Format: GAME_START <board_size> <player1> <player2> ...
        String[] parts = message.split(" ");
        if (parts.length < 3) return;

        try {
            int size = Integer.parseInt(parts[1]);

            players.clear();
            playerScores.clear();

            for (int i = 2; i < parts.length; i++) {
                String player = parts[i];
                players.add(player);
                playerScores.put(player, 0);
            }

            initializeBoard(size);
            updatePlayers();

            gameStarted = true;
            updateStatus("Game started! Waiting for first turn...");

            Platform.runLater(() -> {
                readyButton.setVisible(false);
            });

        } catch (NumberFormatException e) {
            e.printStackTrace();
        }
    }

    private void handleGameEnd(String message) {
        // Format: GAME_END <player1> <score1> <player2> <score2> ...
        String[] parts = message.split(" ");

        StringBuilder resultMsg = new StringBuilder("Game Over!\n\nFinal Scores:\n");
        int maxScore = -1;
        List<String> winners = new ArrayList<>();

        for (int i = 1; i < parts.length; i += 2) {
            if (i + 1 < parts.length) {
                String player = parts[i];
                int score = Integer.parseInt(parts[i + 1]);

                resultMsg.append(String.format("%s: %d\n", player, score));

                if (score > maxScore) {
                    maxScore = score;
                    winners.clear();
                    winners.add(player);
                } else if (score == maxScore) {
                    winners.add(player);
                }
            }
        }

        resultMsg.append("\nWinner(s): ");
        resultMsg.append(String.join(", ", winners));

        String finalMsg = resultMsg.toString();

        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.INFORMATION);
            alert.setTitle("Game Over");
            alert.setHeaderText(null);
            alert.setContentText(finalMsg);
            alert.showAndWait();

            Alert lobbyAlert = new Alert(Alert.AlertType.CONFIRMATION);
            lobbyAlert.setTitle("Return to Lobby");
            lobbyAlert.setHeaderText("Game finished!");
            lobbyAlert.setContentText("Return to lobby?");

            Optional<ButtonType> result = lobbyAlert.showAndWait();
            if (result.isPresent() && result.get() == ButtonType.OK) {
                returnToLobby();
            }
        });
    }

    private void returnToLobby() {
        Platform.runLater(() -> {
            try {
                FXMLLoader loader = new FXMLLoader(
                    getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/LobbyView.fxml")
                );
                Parent root = loader.load();

                LobbyController lobbyController = loader.getController();
                lobbyController.setConnection(connection, nickname);
                lobbyController.setStage(stage);

                Scene scene = new Scene(root, 800, 600);
                stage.setScene(scene);
                stage.setTitle("Pexeso - Lobby");
            } catch (IOException e) {
                e.printStackTrace();
            }
        });
    }

    @Override
    public void onConnected() { }

    @Override
    public void onDisconnected(String reason) {
        Platform.runLater(() -> {
            Alert alert = new Alert(Alert.AlertType.ERROR);
            alert.setTitle("Disconnected");
            alert.setHeaderText("Lost connection to server");
            alert.setContentText(reason);
            alert.showAndWait();
            System.exit(0);
        });
    }

    @Override
    public void onError(String error) {
        updateStatus("Error: " + error);
    }
}
```

#### 9. Vytvoř GameView.fxml

Vytvoř soubor: `src/main/resources/cz/zcu/kiv/ups/pexeso/ui/GameView.fxml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<?import javafx.scene.control.*?>
<?import javafx.scene.layout.*?>
<?import javafx.geometry.Insets?>

<BorderPane xmlns:fx="http://javafx.com/fxml/1"
            fx:controller="cz.zcu.kiv.ups.pexeso.controller.GameController"
            style="-fx-background-color: #f5f5f5;">

    <top>
        <VBox spacing="10">
            <padding><Insets top="10" right="10" bottom="10" left="10"/></padding>
            <Label fx:id="roomNameLabel" text="Room: ..."
                   style="-fx-font-size: 20; -fx-font-weight: bold;"/>
            <Label fx:id="statusLabel" text="Waiting..."/>
            <Label fx:id="turnLabel" text=""/>
        </VBox>
    </top>

    <left>
        <VBox fx:id="playersVBox" spacing="5">
            <padding><Insets top="10" right="10" bottom="10" left="10"/></padding>
        </VBox>
    </left>

    <center>
        <GridPane fx:id="cardGrid"/>
    </center>

    <bottom>
        <HBox spacing="10">
            <padding><Insets top="10" right="10" bottom="10" left="10"/></padding>
            <Button fx:id="startGameButton" text="START GAME"
                    onAction="#handleStartGame"/>
            <Button fx:id="readyButton" text="READY"
                    onAction="#handleReady"/>
            <Button fx:id="leaveRoomButton" text="LEAVE ROOM"
                    onAction="#handleLeaveRoom"/>
        </HBox>
    </bottom>

</BorderPane>
```

#### 10. Aktualizuj ClientConnection.java

Přidej metodu pro změnu message listeneru:

```java
public void setMessageListener(MessageListener listener) {
    this.listener = listener;
}
```

#### 11. Aktualizuj Main.java

```java
@Override
public void start(Stage primaryStage) throws Exception {
    FXMLLoader loader = new FXMLLoader(
        getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/LoginView.fxml")
    );
    Parent root = loader.load();

    LoginController controller = loader.getController();
    controller.setStage(primaryStage);  // Přidej tuto řádku

    Scene scene = new Scene(root, 600, 500);
    primaryStage.setTitle("Pexeso Client - Login");
    primaryStage.setScene(scene);
    primaryStage.setResizable(true);  // Změň na true
    primaryStage.show();

    primaryStage.setOnCloseRequest(event -> {
        System.out.println("Application closing...");
        System.exit(0);
    });
}
```

#### 12. Aktualizuj ProtocolConstants.java

Zvětši timeout pro lobby:

```java
public static final int READ_TIMEOUT_MS = 120000;  // 2 minutes (bylo 30000)
```

#### 13. Zkompiluj a testuj

```bash
cd client_src
mvn clean compile
mvn javafx:run
```

**Test flow:**
1. Spusť server
2. Spusť klienta, přihlas se → Lobby
3. Vytvoř místnost → Game view
4. Druhý klient se připojí a joinne
5. Owner dá START_GAME
6. Oba kliknou READY
7. Hrajte hru!

**Výstup:** Kompletní GUI klient s lobby a hrou!

---

## TIPY A TRIKY

### Debugging:

1. **Valgrind (memory leaks):**
   ```bash
   valgrind --leak-check=full ./server 127.0.0.1 10000 10 50
   ```

2. **GDB (segfaults):**
   ```bash
   gdb ./server
   (gdb) run 127.0.0.1 10000 10 50
   (gdb) bt  # Po pádu
   ```

3. **Logování:**
   ```bash
   tail -f server.log
   ```

### Testování fragmentace:

Použij InTCPtor:
```bash
git clone https://github.com/MartinUbl/InTCPtor
cd InTCPtor && ./build_and_env.sh
./intcptor-run /path/to/server 127.0.0.1 10000 10 50
```

### Časté chyby:

1. **"Address already in use"**
   - Řešení: Použij `SO_REUSEADDR` v `setsockopt()`

2. **GUI zamrzá**
   - Řešení: Síťové operace v background threadu

3. **Race condition**
   - Řešení: Použij mutex pro sdílená data

---

## DALŠÍ KROKY

Po dokončení Packetu 5 můžeš pokračovat:

- **Packet 6:** START_GAME a inicializace hry
- **Packet 7:** Logika Pexesa (FLIP, MATCH, MISMATCH)
- **Packet 8:** PING/PONG keepalive a timeout detection
- **Packet 9:** RECONNECT mechanismus
- **Packet 10:** Finální testování a dokumentace

---

*Tento průvodce bude rozšiřován o další packety.*
*Poslední aktualizace: 2025-11-12*
