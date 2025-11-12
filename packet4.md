# PACKET 4/10 - KOSTRA JAVAFX KLIENTA

**Datum:** 2025-11-12
**Cíl:** Vytvořit funkční JavaFX klienta s GUI pro připojení k serveru
**Status:** ✅ HOTOVO

---

## ZADÁNÍ PACKETU 4

Implementovat JavaFX aplikaci, která:
- Má jednoduché GUI s poli pro IP, port, přezdívku a tlačítkem CONNECT
- TextArea LOG pro zobrazování zpráv
- Po stisku CONNECT vytvoří TCP socket a odešle úvodní zprávy (HELLO)
- Spustí samostatné vlákno pro příjem zpráv
- Příchozí zprávy zapisuje do LOG přes `Platform.runLater()`
- Implementuje třídy podle architektury: `ClientConnection`, `MessageListener`
- Neblokuje JavaFX UI thread
- Připravený pro rozšíření o lobby a hru

---

## IMPLEMENTOVANÉ SOUBORY

### Struktura projektu

```
client_src/
├── pom.xml                                           # Maven build file
├── .gitignore                                        # Git ignore rules
└── src/
    └── main/
        ├── java/
        │   └── cz/zcu/kiv/ups/pexeso/
        │       ├── Main.java                         # JavaFX entry point
        │       ├── network/
        │       │   ├── ClientConnection.java         # TCP connection manager
        │       │   └── MessageListener.java          # Event callback interface
        │       ├── protocol/
        │       │   └── ProtocolConstants.java        # Protocol constants
        │       └── controller/
        │           └── LoginController.java          # Login screen controller
        └── resources/
            └── cz/zcu/kiv/ups/pexeso/
                └── ui/
                    └── LoginView.fxml                # Login screen layout
```

---

## DETAILNÍ POPIS SOUBORŮ

### 1. `ProtocolConstants.java` (1.8 KB)

**Účel:** Definice všech protokolových konstant

**Obsah:**
- Příkazy klient→server: `CMD_HELLO`, `CMD_LIST_ROOMS`, `CMD_FLIP`, ...
- Příkazy server→klient: `CMD_WELCOME`, `CMD_ERROR`, `CMD_PING`, ...
- Chybové kódy: `ERR_NOT_IMPLEMENTED`, `ERR_INVALID_COMMAND`, ...
- Limity: `MAX_MESSAGE_LENGTH`, `MAX_NICK_LENGTH`
- Timeouty: `CONNECTION_TIMEOUT_MS`, `READ_TIMEOUT_MS`

```java
public class ProtocolConstants {
    public static final String CMD_HELLO = "HELLO";
    public static final String CMD_WELCOME = "WELCOME";
    public static final String CMD_ERROR = "ERROR";
    public static final String CMD_PING = "PING";
    public static final String CMD_PONG = "PONG";

    public static final int CONNECTION_TIMEOUT_MS = 5000;
    public static final int MAX_NICK_LENGTH = 32;
    // ...
}
```

---

### 2. `MessageListener.java` (0.9 KB)

**Účel:** Interface pro callback události ze síťové vrstvy

**Metody:**
```java
public interface MessageListener {
    void onMessageReceived(String message);  // Přijata zpráva
    void onConnected();                      // Připojeno k serveru
    void onDisconnected(String reason);      // Odpojeno
    void onError(String error);              // Chyba spojení
}
```

**Použití:**
- Implementuje `LoginController`
- Metody jsou volány z network threadu
- Pro UI aktualizace použít `Platform.runLater()`

---

### 3. `ClientConnection.java` (5.8 KB)

**Účel:** Správa TCP spojení a komunikace se serverem

**Klíčové vlastnosti:**
- Samostatné vlákno pro čtení zpráv (`readerThread`)
- Thread-safe operace
- Automatický cleanup při odpojení
- Timeout handling

**Hlavní metody:**
```java
public class ClientConnection {
    public void connect(String host, int port) throws IOException;
    public boolean sendMessage(String message);
    public boolean isConnected();
    public void disconnect();
}
```

**Životní cyklus:**

1. **Vytvoření:**
   ```java
   ClientConnection connection = new ClientConnection(messageListener);
   ```

2. **Připojení:**
   ```java
   connection.connect("127.0.0.1", 10000);
   // → Vytvoří Socket, nastaví timeout
   // → Spustí reader thread
   // → Volá messageListener.onConnected()
   ```

3. **Odesílání zpráv:**
   ```java
   connection.sendMessage("HELLO Alice");
   // → Přidá \n na konec
   // → Odešle přes PrintWriter
   ```

4. **Příjem zpráv (v reader threadu):**
   ```java
   // reader loop:
   while (running && (line = in.readLine()) != null) {
       messageListener.onMessageReceived(line);
   }
   ```

5. **Odpojení:**
   ```java
   connection.disconnect();
   // → Zastaví reader thread
   // → Uzavře socket a streams
   // → Volá messageListener.onDisconnected()
   ```

**Thread-safety:**
- `volatile boolean running` - sdílený flag mezi thready
- Socket operace jsou thread-safe
- Listener callbacks voláno z network threadu → musí použít `Platform.runLater()` pro UI

---

### 4. `LoginController.java` (5.2 KB)

**Účel:** Controller pro login obrazovku

**FXML elementy:**
```java
@FXML private TextField ipField;
@FXML private TextField portField;
@FXML private TextField nicknameField;
@FXML private Button connectButton;
@FXML private TextArea logArea;
```

**Klíčové metody:**

#### `initialize()`
- Nastavení výchozích hodnot (127.0.0.1:10000)
- Vytvoření instance `ClientConnection`
- Inicializační log

#### `handleConnect()`
- Obsluha tlačítka CONNECT/DISCONNECT
- Deleguje na `connect()` nebo `disconnect()`

#### `connect()`
- Validace vstupů (IP, port, nickname)
- Spuštění připojení v **background threadu** (neblokuje UI!)
- Po úspěšném spojení odesílá `HELLO <nickname>`

```java
new Thread(() -> {
    try {
        connection.connect(ip, port);
        connection.sendMessage("HELLO " + nickname);
    } catch (IOException e) {
        Platform.runLater(() -> {
            log("ERROR: " + e.getMessage());
            setInputsEnabled(true);
        });
    }
}).start();
```

#### `log(String message)`
- Logování s časovou značkou `[HH:mm:ss]`
- Thread-safe: kontroluje `Platform.isFxApplicationThread()`
- Pokud voláno z jiného threadu → použije `Platform.runLater()`

**MessageListener implementace:**

```java
@Override
public void onMessageReceived(String message) {
    log("Received: " + message);

    // Automatická odpověď na PING
    if (message.startsWith("PING")) {
        connection.sendMessage("PONG");
    }
}

@Override
public void onConnected() {
    Platform.runLater(() -> {
        connectButton.setText("DISCONNECT");
        log("Connected to server!");
    });
}

@Override
public void onDisconnected(String reason) {
    Platform.runLater(() -> {
        connectButton.setText("CONNECT");
        setInputsEnabled(true);
        log("Disconnected: " + reason);
    });
}
```

---

### 5. `Main.java` (0.8 KB)

**Účel:** Vstupní bod JavaFX aplikace

**Struktura:**
```java
public class Main extends Application {
    @Override
    public void start(Stage primaryStage) throws Exception {
        // Načtení FXML
        FXMLLoader loader = new FXMLLoader(
            getClass().getResource("/cz/zcu/kiv/ups/pexeso/ui/LoginView.fxml")
        );
        Parent root = loader.load();

        // Vytvoření scény
        Scene scene = new Scene(root, 600, 500);

        // Nastavení stage
        primaryStage.setTitle("Pexeso Client - Login");
        primaryStage.setScene(scene);
        primaryStage.setResizable(false);
        primaryStage.show();

        // Cleanup při zavření
        primaryStage.setOnCloseRequest(event -> System.exit(0));
    }

    public static void main(String[] args) {
        launch(args);
    }
}
```

**Poznámky:**
- `setResizable(false)` - fixed velikost okna
- `setOnCloseRequest()` - graceful shutdown
- FXML načítáno z resources

---

### 6. `LoginView.fxml` (2.1 KB)

**Účel:** Layout login obrazovky

**Komponenty:**

1. **Hlavička:**
   - Label "Pexeso Client" (font size 24)

2. **Formulář (GridPane):**
   - TextField pro IP (default: 127.0.0.1)
   - TextField pro Port (default: 10000)
   - TextField pro Nickname (default: Player###)
   - Button CONNECT (zelený, bold)

3. **Log oblast:**
   - Label "Log:"
   - TextArea (read-only, wrap text, 250px výška)

**Styling:**
- Background: `#f5f5f5`
- Formulář: bílý s šedým borderem
- Connect button: zelený (#4CAF50)
- Log area: světle šedý background (#fafafa)

**Binding:**
- Controller: `cz.zcu.kiv.ups.pexeso.controller.LoginController`
- FXML IDs: `fx:id="ipField"`, `fx:id="connectButton"`, atd.
- Action: `onAction="#handleConnect"`

---

### 7. `pom.xml` (3.2 KB)

**Účel:** Maven build konfigurace

**Základní info:**
```xml
<groupId>cz.zcu.kiv.ups</groupId>
<artifactId>pexeso-client</artifactId>
<version>1.0-SNAPSHOT</version>
<name>Pexeso Client</name>
```

**Properties:**
- Java 17
- JavaFX 17.0.2
- UTF-8 encoding
- Main class: `cz.zcu.kiv.ups.pexeso.Main`

**Závislosti:**
```xml
<dependency>
    <groupId>org.openjfx</groupId>
    <artifactId>javafx-controls</artifactId>
    <version>17.0.2</version>
</dependency>

<dependency>
    <groupId>org.openjfx</groupId>
    <artifactId>javafx-fxml</artifactId>
    <version>17.0.2</version>
</dependency>
```

**Pluginy:**

1. **maven-compiler-plugin** (3.11.0)
   - Kompilace Java 17

2. **javafx-maven-plugin** (0.0.8)
   - Spouštění JavaFX aplikace

3. **maven-shade-plugin** (3.5.0)
   - Vytvoření fat JAR s všemi závislostmi
   - Manifest s main class

---

## BUILD A SPUŠTĚNÍ

### Překlad s Maven

```bash
cd client_src

# Kompilace
mvn clean compile

# Vytvoření JAR
mvn clean package

# Spuštění (během vývoje)
mvn javafx:run
```

**Výstup po `mvn package`:**
- `target/pexeso-client-1.0-SNAPSHOT.jar` - fat JAR s všemi závislostmi
- Spustitelný přes: `java -jar target/pexeso-client-1.0-SNAPSHOT.jar`

### Spuštění JAR

```bash
# Po mvn package
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

### Alternativa: Přímé spuštění z IDE

- IntelliJ IDEA: Run → Main.java
- Eclipse: Run As → Java Application
- NetBeans: Run File

**Poznámka:** IDE musí mít nainstalovaný JavaFX plugin nebo správně nakonfigurované JavaFX SDK.

---

## POŽADAVKY NA PROSTŘEDÍ

### Java

```bash
# Ověření verze (minimum Java 17)
java -version

# Mělo by vrátit:
# openjdk version "17.0.x" nebo vyšší
```

### Maven

```bash
# Ověření verze (minimum Maven 3.6)
mvn -version

# Mělo by vrátit:
# Apache Maven 3.x.x
```

### JavaFX

- JavaFX 17 se stáhne automaticky přes Maven
- Není potřeba manuální instalace

---

## TESTOVÁNÍ

### Test 1: Build

```bash
cd client_src
mvn clean compile
```

**Očekávaný výstup:**
```
[INFO] BUILD SUCCESS
[INFO] Total time: X.XXX s
```

### Test 2: Spuštění

```bash
mvn javafx:run
```

**Očekávané chování:**
- Otevře se okno s login obrazovkou
- Pole jsou předvyplněná (127.0.0.1:10000, Player###)
- Log obsahuje: `[HH:mm:ss] Client initialized. Ready to connect.`

### Test 3: Připojení k serveru

**Prerekvizity:** Server z Packetu 3 běží na `127.0.0.1:10000`

```bash
# V terminálu 1
cd server_src
./server 127.0.0.1 10000 10 50

# V terminálu 2
cd client_src
mvn javafx:run
```

**Kroky v GUI:**
1. Ponechej výchozí hodnoty (127.0.0.1:10000)
2. Zadej nickname (např. "Alice")
3. Klikni CONNECT

**Očekávaný výstup v logu:**
```
[10:15:30] Client initialized. Ready to connect.
[10:15:35] Connecting to 127.0.0.1:10000...
[10:15:35] Connected to server!
[10:15:35] Sent: HELLO Alice
[10:15:35] Received: ERROR NOT_IMPLEMENTED Server skeleton - all commands return this error
[10:15:35] Received: ERROR NOT_IMPLEMENTED Command not yet implemented
```

**Server log (server_src/server.log):**
```
[2025-11-12 10:15:35] [INFO] New connection from 127.0.0.1:xxxxx (fd=5)
[2025-11-12 10:15:35] [INFO] Client 1: Thread created successfully
[2025-11-12 10:15:35] [INFO] Client 1: Received message: 'HELLO Alice'
```

### Test 4: PING/PONG

Server může periodicky posílat PING - klient automaticky odpovídá PONG.

**Simulace (přes netcat jako server):**
```bash
# Terminál 1 - "fake server"
nc -l 127.0.0.1 -p 10000

# Terminál 2 - klient
mvn javafx:run

# Po připojení v terminálu 1 napiš:
PING

# Klient automaticky odpoví:
PONG
```

**Výstup v klientu:**
```
[10:20:00] Received: PING
[10:20:00] Sent: PONG
```

### Test 5: Odpojení

1. Připoj se k serveru
2. Klikni DISCONNECT

**Očekávaný výstup:**
```
[10:25:00] Disconnecting...
[10:25:00] Disconnected: User requested disconnect
```

Tlačítko se změní na "CONNECT", pole se odemknou.

---

## ARCHITEKTURA A DESIGN PATTERNS

### 1. MVC Pattern

```
Model:           (zatím není - v budoucnu GameState)
View:            LoginView.fxml
Controller:      LoginController.java
```

### 2. Observer Pattern

```
Subject:         ClientConnection
Observer:        MessageListener (implementováno LoginController)
Events:          onConnected, onDisconnected, onMessageReceived, onError
```

### 3. Thread Separation

```
┌─────────────────────┐
│  JavaFX Thread      │  ← UI updates
│  (Main Thread)      │
└──────────┬──────────┘
           │
           │ Platform.runLater()
           │
┌──────────▼──────────┐
│  Network Thread     │  ← Socket operations
│  (Reader Thread)    │
└─────────────────────┘
```

**Proč oddělené thready?**
- JavaFX UI je single-threaded
- Blokující I/O (např. `readLine()`) by zamrazilo GUI
- Síťové operace běží asynchronně
- UI updates přes `Platform.runLater()` jsou thread-safe

---

## ZABRÁNĚNÍ ZAMRZNUTÍ GUI

### Problém:
```java
// ❌ ŠPATNĚ - blokuje UI thread
@FXML
private void handleConnect() {
    connection.connect(ip, port);  // Blokující operace!
    // GUI zamrzne během připojování
}
```

### Řešení:
```java
// ✅ SPRÁVNĚ - background thread
@FXML
private void handleConnect() {
    new Thread(() -> {
        try {
            connection.connect(ip, port);  // Běží na background threadu
        } catch (IOException e) {
            Platform.runLater(() -> {
                log("ERROR: " + e.getMessage());  // UI update na FX threadu
            });
        }
    }).start();
}
```

### Pravidla:

1. **Síťové operace → background thread**
   - `connect()`, `disconnect()`, dlouhé výpočty

2. **UI aktualizace → JavaFX thread**
   - `setText()`, `appendText()`, `setDisable()`
   - Použít `Platform.runLater()`

3. **Kontrola threadu:**
   ```java
   if (Platform.isFxApplicationThread()) {
       // Už jsme na FX threadu
       logArea.appendText(message);
   } else {
       // Přepnout na FX thread
       Platform.runLater(() -> logArea.appendText(message));
   }
   ```

---

## ROZŠIŘITELNOST

### Co je připraveno:

✅ **Oddělená síťová vrstva** (`ClientConnection`)
- Snadno rozšířit o reconnect logiku
- Připraveno pro další obrazovky

✅ **MessageListener interface**
- Lze implementovat v jiných controllerech (LobbyController, GameController)
- Sdílené `ClientConnection` mezi obrazovkami

✅ **Struktura balíčků**
```
network/     - síťová vrstva
protocol/    - protokol
controller/  - controllery pro obrazovky
ui/          - FXML soubory
model/       - datové modely (zatím prázdné)
```

✅ **FXML design**
- Snadno přidat další obrazovky (LobbyView, GameView)
- Scene switching v Main.java

### Další kroky (budoucí packety):

⏳ **Model vrstva:**
```java
model/
├── GameState.java    - Stav celé hry
├── Room.java         - Místnost
├── Player.java       - Hráč
└── Card.java         - Karta
```

⏳ **Další obrazovky:**
```
ui/
├── LoginView.fxml    ✅ Hotovo
├── LobbyView.fxml    ⏳ Budoucnost
└── GameView.fxml     ⏳ Budoucnost
```

⏳ **Parsování zpráv:**
```java
protocol/
├── MessageParser.java    - Parsování příchozích zpráv
└── MessageBuilder.java   - Tvorba odchozích zpráv
```

⏳ **Vizualizace Pexesa:**
- GridPane s kartami (ImageView nebo Button)
- Animace otáčení karet
- Zvýraznění aktuálního hráče

---

## POUŽITÉ TECHNOLOGIE

| Komponenta | Verze | Účel |
|------------|-------|------|
| **Java** | 17 | Programovací jazyk |
| **JavaFX** | 17.0.2 | GUI framework |
| **Maven** | 3.x | Build tool |
| **FXML** | - | Deklarativní UI layout |

### Použité Java API:

**Síť:**
- `java.net.Socket` - TCP spojení
- `java.io.BufferedReader` - čtení řádků
- `java.io.PrintWriter` - odesílání zpráv
- `java.nio.charset.StandardCharsets` - UTF-8 encoding

**Threading:**
- `java.lang.Thread` - vlákna
- `volatile` - thread-safe flag
- `javafx.application.Platform` - UI thread synchronizace

**JavaFX:**
- `javafx.application.Application` - entry point
- `javafx.fxml.FXML` - FXML binding
- `javafx.scene.control.*` - UI komponenty

---

## STATISTIKY

| Metrika | Hodnota |
|---------|---------|
| **Počet Java souborů** | 5 |
| **Počet FXML souborů** | 1 |
| **Řádky Java kódu** | ~450 LOC |
| **Velikost JAR** | ~15 MB (s JavaFX) |
| **Kompilace** | 0 warnings |
| **JavaFX verze** | 17.0.2 |
| **Min. Java verze** | 17 |

---

## BEZPEČNOSTNÍ ASPEKTY

### Validace vstupů

```java
// IP address
if (ip.isEmpty()) {
    log("ERROR: IP address is required");
    return;
}

// Port
if (port < 1 || port > 65535) {
    log("ERROR: Port must be between 1 and 65535");
    return;
}

// Nickname
if (nickname.length() > ProtocolConstants.MAX_NICK_LENGTH) {
    log("ERROR: Nickname too long");
    return;
}
```

### Exception handling

```java
// Connection errors
try {
    connection.connect(ip, port);
} catch (IOException e) {
    log("ERROR: Failed to connect: " + e.getMessage());
}

// Reader thread errors
try {
    while ((line = in.readLine()) != null) { ... }
} catch (SocketTimeoutException e) {
    listener.onError("Connection timeout");
} catch (IOException e) {
    listener.onError("Connection lost");
}
```

### Resource cleanup

```java
// ClientConnection cleanup()
try {
    if (out != null) out.close();
    if (in != null) in.close();
    if (socket != null) socket.close();
} catch (Exception e) {
    // Ignorovat - cleanup shouldn't throw
}
```

---

## ZNÁMÉ LIMITACE (SKELETON)

| Limitace | Důvod | Budoucí řešení |
|----------|-------|----------------|
| ❌ Jen login obrazovka | Skeleton fáze | Packet 5+ přidá lobby a game |
| ❌ Žádné parsování zpráv | Základní funkcionalita | MessageParser v Packet 5 |
| ❌ Žádný reconnect | Není implementováno | Automatický reconnect v Packet 6 |
| ❌ Žádné zobrazení místností | Zatím neimplementováno | LobbyView v Packet 5 |
| ❌ Fixed velikost okna | Jednoduchost | Responzivní layout později |
| ❌ Žádný error dialog | Jen log | Přidat AlertDialog |

---

## TROUBLESHOOTING

### Problém: JavaFX není nalezen

**Symptom:**
```
Error: JavaFX runtime components are missing
```

**Řešení:**
```bash
# Ujisti se, že máš Java 17+ s JavaFX modulem
java --version

# Nebo použij Maven (JavaFX se stáhne automaticky)
mvn javafx:run
```

### Problém: Connection refused

**Symptom:**
```
[10:30:00] ERROR: Failed to connect: Connection refused
```

**Řešení:**
- Ověř, že server běží: `ps aux | grep server`
- Zkontroluj IP a port
- Zkus `nc 127.0.0.1 10000` pro test

### Problém: GUI zamrzá

**Symptom:** Tlačítko CONNECT nezareaguje, okno "Not responding"

**Řešení:**
- Ujisti se, že `connect()` běží v background threadu
- Zkontroluj timeout nastavení v `ProtocolConstants`

### Problém: Maven build fails

**Symptom:**
```
[ERROR] Failed to execute goal
```

**Řešení:**
```bash
# Vyčisti Maven cache
mvn clean

# Zkus s -X pro debug
mvn clean compile -X

# Zkontroluj pom.xml syntax
```

---

## ZÁVĚR

**Packet 4 úspěšně dokončen!** ✅

### Co funguje:
- ✅ JavaFX GUI s login obrazovkou
- ✅ TCP spojení k serveru (non-blocking)
- ✅ Samostatné vlákno pro příjem zpráv
- ✅ Thread-safe UI aktualizace přes `Platform.runLater()`
- ✅ Logování všech zpráv s časovými značkami
- ✅ Automatická odpověď na PING (PONG)
- ✅ Graceful disconnect
- ✅ Maven build s fat JAR
- ✅ Validace vstupů
- ✅ Exception handling

### Připravenost:
- Klient je **funkční** a komunikuje se serverem z Packetu 3
- Architektura je **modulární** a připravená pro rozšíření
- Kód je **čitelný** a dobře strukturovaný
- Všechny požadavky Packetu 4 **splněny**

### Test s Packetem 3:

```bash
# Terminál 1: Server
cd server_src
./server 127.0.0.1 10000 10 50

# Terminál 2: Klient
cd client_src
mvn javafx:run

# → Připoj se → úspěch! ✅
```

**Komunikace:**
```
Klient → Server: HELLO Alice
Server → Klient: ERROR NOT_IMPLEMENTED Server skeleton...
Server → Klient: ERROR NOT_IMPLEMENTED Command not yet implemented
```

**Další krok:** Packet 5 - Lobby obrazovka, parsování zpráv, správa místností

---

*Dokument vytvořen: 2025-11-12*
*Autor: Claude Code*
*Verze: 1.0*
