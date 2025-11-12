# MANUÁLNÍ PRŮVODCE - PEXESO SERVER-KLIENT

**Účel:** Krok-po-kroku návod, jak implementovat projekt Pexeso bez AI
**Pro:** Studenty KIV/UPS, vývojáře učící se síťové programování
**Poslední aktualizace:** 2025-11-12 (Packety 1-5)

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
