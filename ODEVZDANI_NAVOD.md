# Návod pro odevzdání - KIV/UPS Semestrální práce

**Tento dokument obsahuje přesný postup jak otestovat VŠECHNY požadavky ze zadání před odevzdáním.**

Červené body (●) = kritické, jejich nesplnění = vrácení práce!

---

## 🚀 Příprava před odevzdáním

### Krok 1: Ověř prostředí v UC-326
```bash
# Přihlaš se do UC-326
# Otevři terminál a ověř verze:
gcc --version        # Mělo by být gcc 11+
java --version       # Mělo by být Java 11+
javac --version
maven --version
```

### Krok 2: Přenes projekt do UC-326
```bash
# Z Windowsu do UC-326 (přes WinSCP nebo git)
# Umísti projekt např. do /tmp/ups_projekt
cd /tmp
git clone <tvuj-repozitar>  # nebo zkopíruj
cd ups_projekt
```

---

## ✅ ČÁST 1: PŘEKLAD APLIKACÍ (KRITICKÉ ●)

### Test 1.1: Překlad serveru
**Požadavek**: ● Aplikace přeloženy standardním nástrojem (make, ne bash skript, ne ručně gcc)

```bash
cd server_src
make clean
make
```

**Očekávaný výsledek**:
```
✅ Úspěšný překlad bez chyb
✅ Vznikne binárka: ./server
✅ Žádné warnings (nebo minimální)
```

**Co cvičící kontroluje**:
- Je přítomen Makefile
- Překlad proběhne jedním příkazem `make`
- Nevyužívá se žádná externí networking knihovna

---

### Test 1.2: Překlad klienta (Linux)
**Požadavek**: ● Aplikace přeloženy standardním nástrojem (maven)

```bash
cd ../client_src
mvn clean package
```

**Očekávaný výsledek**:
```
✅ [INFO] BUILD SUCCESS
✅ Vznikne JAR: target/pexeso-client-1.0-SNAPSHOT.jar
✅ Žádné compilation errors
```

---

## ✅ ČÁST 2: SPUŠTĚNÍ S PARAMETRY (KRITICKÉ ●)

### Test 2.1: Server s parametry
**Požadavek**: ● IP adresa a port nastavitelné při spuštění (ne hardcoded)
**Požadavek**: ● Limit místností nastavitelný
**Požadavek**: ● Limit hráčů nastavitelný

```bash
# Zpět na Linux PC
cd server_src

# Test 1: Minimální konfigurace
./server 127.0.0.1 10000 5 10

# Test 2: Jiný port
./server 0.0.0.0 20000 10 50

# Test 3: Velká kapacita
./server 0.0.0.0 10000 20 100
```

**Co ukázat cvičícímu**:
```
✅ Server přijímá 4 argumenty: IP PORT MAX_ROOMS MAX_CLIENTS
✅ Server vypisuje konfiguraci při startu:
   [INFO] Configuration: IP=0.0.0.0, Port=10000, MaxRooms=10, MaxClients=50
✅ Server naslouchá na zadané IP a portu
✅ Limity se projevují při běhu
```

---

### Test 2.2: Klient - zadání adresy a portu
**Požadavek**: ● Klient umožní zadání IP a portu pro připojení

**Spusť klienta**:
```bash
cd client_src
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**Co ukázat cvičícímu**:
```
✅ GUI okno s poli pro:
   - IP address (např. 127.0.0.1)
   - Port (např. 10000)
   - Nickname
✅ Tlačítko "CONNECT"
✅ Možnost zadat různé adresy (localhost, 127.0.0.1, IP stroje)
```

---

## ✅ ČÁST 3: JEDNA CELÁ HRA BEZ VÝPADKŮ (KRITICKÉ ●)

### Test 3.1: Kompletní hra 2 hráčů
**Požadavek**: ● Vždy by měla jít dohrát ve 2 lidech
**Požadavek**: ● Hráči po skončení hry přesunuti zpět do lobby

**Scénář pro cvičícího**:

1. **Spuštění serveru**:
```bash
# Linux PC
cd server_src
./server 0.0.0.0 10000 10 50
```

2. **Klient 1 (Linux)**:
```bash
# Druhý terminál na Linux PC
cd client_src
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```
- Zadej: IP=127.0.0.1, Port=10000, Nick=Player1
- Connect
- Create Room: "TestRoom", 2 players, 4x4 board

3. **Klient 2 (Windows)**:
```cmd
# Windows PC v UC-326
cd client_src
run-client.bat
```
- Zadej: IP=<Linux-IP>, Port=10000, Nick=Player2
- Refresh rooms
- Join "TestRoom"

4. **Hra**:
- Player1: START GAME
- Oba: READY
- Hraj celou hru (16 karet = 8 párů)
- Dohrání do konce

**Co ukázat cvičícímu**:
```
✅ Lobby se seznamem místností
✅ Vytvoření místnosti
✅ Připojení druhého hráče
✅ Začátek hry (všichni ready)
✅ Tahy střídavě
✅ Správné počítání skóre
✅ Konec hry s výsledky
✅ Automatický návrat do lobby (BEZ potvrzovacího dialogu)
✅ Možnost hrát další hru
```

**Server log musí obsahovat**:
```
[INFO] Client 1 authenticated as 'Player1'
[INFO] Room created: id=1, name='TestRoom'
[INFO] Client 2 (Player2) joined room 1
[INFO] Game started, first player: Player1
[INFO] Room 1: Player Player1 flipped card 0 (value=3)
[INFO] MATCH! Player Player1 matched cards...
[INFO] Game finished! All pairs matched
[INFO] Client 1 (Player1) returned to lobby after game end
[INFO] Client 2 (Player2) returned to lobby after game end
```

---

## ✅ ČÁST 4: VÝPADKY A RECONNECT (KRITICKÉ ●)

### Test 4.1: Krátkodobý výpadek - iptables DROP
**Požadavek**: ● Hra umožňuje zotavení po krátkodobé síťové nedostupnosti
**Požadavek**: ● Krátkodobá nedostupnost nesmí nutit hráče k manuálnímu připojení (automatické)
**Požadavek**: ● Všichni hráči musí vědět o výpadku protihráče

**Scénář pro cvičícího**:

1. **Rozehrání hry** (stejně jako Test 3.1)

2. **Simulace výpadku**:
```bash
# Na Linux serveru (v třetím terminálu):
sudo iptables -A INPUT -p tcp --dport 10000 -j DROP
```

3. **Co se stane (10-15 sekund)**:
```
✅ Oba klienti zobrazí: "Disconnected from server - attempting to reconnect..."
✅ Server log: "Client X (PlayerY) marked as disconnected due to PONG timeout"
✅ Server čeká 90s na reconnect
```

4. **Obnovení spojení**:
```bash
# Odebrání pravidla:
sudo iptables -D INPUT -p tcp --dport 10000 -j DROP
```

5. **Co se stane (5-25 sekund)**:
```
✅ Klienti automaticky reconnectují (5 pokusů po 5s)
✅ Server log: "Client X: Reconnecting as client Y, disconnect duration: 15 seconds"
✅ Oba klienti obdrží GAME_STATE a pokračují ve hře
✅ Klient na tahu vidí "It's your turn!"
✅ Dialog: "Player X reconnected"
✅ Hra pokračuje normálně
```

**Co ukázat cvičícímu**:
```
✅ Automatický reconnect (BEZ manuálního klikání)
✅ Obnovení stavu hry (skóre, otočené karty)
✅ Notifikace o reconnectu protihráče
✅ Hra pokračuje kde skončila
✅ Server log obsahuje "RECONNECT", "WELCOME", "GAME_STATE"
```

---

### Test 4.2: Krátkodobý výpadek - iptables REJECT
**Stejný test jako 4.1, ale s REJECT místo DROP**

```bash
# Simulace:
sudo iptables -A INPUT -p tcp --dport 10000 -j REJECT

# Obnovení:
sudo iptables -D INPUT -p tcp --dport 10000 -j REJECT
```

**Rozdíl**:
```
✅ Rychlejší reconnect (Connection refused místo timeout)
✅ Klient ihned ví, že server není dostupný
✅ Stejný výsledek - úspěšný reconnect
```

---

### Test 4.3: Dlouhodobý výpadek (> 90s)
**Požadavek**: ● Dlouhodobá nedostupnost vyžaduje manuální akci
**Požadavek**: ● Hráč nedostupný dlouhodobě je odebrán ze hry
**Požadavek**: ● Aktivní hráč vrácen zpět do lobby

**Scénář pro cvičícího**:

1. Rozehraj hru

2. Aplikuj iptables DROP

3. **POČKEJ 95 sekund** (přes RECONNECT_TIMEOUT)

4. **Co se stane**:
```
✅ Server log každých 10s: "Client X waiting for reconnect: 10/90 seconds"
✅ Po 90s: "GAME_END_FORFEIT"
✅ Server log: "Game finished, 1 winner(s)"
✅ Hráč s vyšším skóre dostane bonus za zbývající páry
✅ Připojený klient vidí: "Game Over - Forfeit! A player disconnected."
✅ Připojený klient automaticky vrácen do lobby
✅ Místnost a hra uvolněna
```

5. Odstraň iptables pravidlo

6. **Odpojený klient**:
```
✅ Po 5 pokusech (25s): "Auto-reconnect failed after 5 attempts"
✅ Klient zobrazí: "Your session has expired. Please login again manually."
✅ Vrácen na login obrazovku
```

---

### Test 4.4: Informace o nedostupnosti protihráče
**Požadavek**: ● Klient viditelně informuje o nedostupnosti protihráče

**Během Test 4.1 zkontroluj**:

**SHORT disconnect (čeká se)**:
```
✅ Warning dialog: "Player X temporarily disconnected"
✅ Text: "Waiting for reconnect (up to 90 seconds)..."
✅ Status label: "Player1 disconnected - waiting for reconnect (90s)"
```

**LONG disconnect (nebude se vracet)**:
```
✅ Error dialog: "Player X disconnected permanently"
✅ Text: "Player did not reconnect within timeout. Game may be ended."
✅ Game Over screen s forfeit informací
```

---

## ✅ ČÁST 5: NEVALIDNÍ DATA (KRITICKÉ ●)

### Test 5.1: Náhodná data z /dev/urandom
**Požadavek**: ● Obě aplikace ošetřují náhodná data nevyhovující protokolu

```bash
# Spuštění serveru:
./server 0.0.0.0 10000 10 50

# V druhém terminálu - pošli random data:
cat /dev/urandom | head -c 1000 | nc 127.0.0.1 10000
```

**Co ukázat cvičícímu**:
```
Server log:
[WARN] Client X: Error sent - INVALID_COMMAND (count: 1/3)
[WARN] Client X: Error sent - INVALID_COMMAND (count: 2/3)
[WARN] Client X: Error sent - INVALID_COMMAND (count: 3/3)
[ERROR] Client X: Max error count reached, closing connection
[INFO] Client X: Connection closed

✅ Server NEpadl
✅ Po 3 chybách odpojení
✅ Server pokračuje v běhu
✅ Jiní klienti neovlivněni
```

---

### Test 5.2: Formátově správné, ale neplatná data
**Požadavek**: ● Zprávy s očividně neplatnými daty (tah na pole -1)

```bash
# Připoj se netcatem:
nc 127.0.0.1 10000

# Zadej postupně:
HELLO TestPlayer
FLIP -1
```

**Očekávaný výsledek**:
```
Server response:
WELCOME 1 TestPlayer
ERROR NOT_IN_ROOM Not in a room

Server log:
[WARN] Client 1 (TestPlayer): Error sent - NOT_IN_ROOM (count: 1/3)

✅ Validace rozsahu hodnot
✅ Odpojení po 3 chybách
```

---

### Test 5.3: Příkazy ve špatném stavu
**Požadavek**: ● Zprávy ve špatném stavu hry (tah když není ve hře)

```bash
nc 127.0.0.1 10000

# Zadej:
HELLO TestPlayer
FLIP 5
START_GAME
READY
```

**Očekávaný výsledek**:
```
ERROR NOT_IN_ROOM Not in a room
ERROR NOT_IN_ROOM Not in a room
ERROR NOT_IN_ROOM Not in a room

✅ Server kontroluje stav klienta před každým příkazem
✅ Po 3 chybách → disconnect
```

---

### Test 5.4: Nevalidní game rules
**Požadavek**: ● Zprávy s nevalidními vstupy dle pravidel hry

**Připoj legitimního klienta, vstup do hry:**

```bash
# Po vstupu do aktivní hry:
FLIP 100    # Karta mimo rozsah
```

**Očekávaný výsledek**:
```
ERROR INVALID_MOVE Card index out of bounds (0-15)

Server log:
[WARN] Client X: Error sent - INVALID_MOVE (count: 1/3)
```

**Další testy v GUI klientovi**:
- Klikni na už otočenou kartu → Server odmítne
- Flipni kartu když nejsi na tahu → ERROR NOT_YOUR_TURN
- Flipni 3 karty v jednom tahu → Server odmítne třetí

---

### Test 5.5: Počítadlo chyb
**Požadavek**: ● Aplikace obsahují počítadlo, odpojí po např. třech

```bash
nc 127.0.0.1 10000

# Pošli postupně:
INVALID1
INVALID2
INVALID3
# <spojení ukončeno serverem>
```

**Co ukázat cvičícímu**:
```
Server log:
[WARN] Client X: Error sent - INVALID_COMMAND (count: 1/3)
[WARN] Client X: Error sent - INVALID_COMMAND (count: 2/3)
[WARN] Client X: Error sent - INVALID_COMMAND (count: 3/3)
[ERROR] Client X: Max error count reached, closing connection

✅ Přesně 3 chyby tolerance
✅ 4. chyba = disconnect
✅ Konfigurovatelné (MAX_ERROR_COUNT = 3 v protocol.h)
```

---

## ✅ ČÁST 6: PARALELNÍ MÍSTNOSTI (KRITICKÉ ●)

### Test 6.1: Více souběžných her
**Požadavek**: ● Server je schopen paralelně obsluhovat několik místností bez vzájemného ovlivnění

**Scénář pro cvičícího**:

1. **Spusť server**:
```bash
./server 0.0.0.0 10000 10 50
```

2. **Připoj 6 klientů** (3 Linux terminály + 3 Windows)

3. **Vytvoř 3 místnosti**:
   - Room1: Player1 + Player2
   - Room2: Player3 + Player4
   - Room3: Player5 + Player6

4. **Začni všechny 3 hry současně**

5. **Hraj ve všech místnostech paralelně**:
   - Room1: Tah
   - Room2: Tah
   - Room3: Tah
   - Room1: Tah
   - ...

**Co ukázat cvičícímu**:
```
✅ Všechny 3 hry běží nezávisle
✅ Tah v Room1 neovlivní Room2 nebo Room3
✅ Různý stav ve všech hrách (skóre, otočené karty)
✅ Žádné race conditions
✅ Žádné chyby v logu

Server log:
[INFO] Room 1: Player Player1 flipped card 5
[INFO] Room 2: Player Player3 flipped card 2
[INFO] Room 3: Player Player5 flipped card 8
[INFO] Room 1: MATCH! Player Player1...
```

---

### Test 6.2: Limit místností
**Požadavek**: ● Počet místností (limit) je nastavitelný

```bash
# Spusť server s limitem 2 místností:
./server 0.0.0.0 10000 2 50

# Připoj 6 klientů
# Vytvoř 2 místnosti → úspěch
# Pokus o 3. místnost → ERROR
```

**Očekávaný výsledek**:
```
Klient 3 při pokusu CREATE_ROOM:
ERROR No free room slots available

✅ Server respektuje MAX_ROOMS
✅ Elegantní odmítnutí (ne crash)
```

---

### Test 6.3: Limit klientů
**Požadavek**: ● Celkový limit hráčů omezen

```bash
# Spusť server s limitem 3 klienti:
./server 0.0.0.0 10000 10 3

# Připoj 3 klienty → úspěch
# Připoj 4. klienta → odmítnuto
```

**Očekávaný výsledek**:
```
Server log:
[WARN] Connection limit reached, rejecting connection from X.X.X.X

Klient 4:
ERROR: Failed to connect: Connection refused

✅ Server odmítne nové spojení po dosažení limitu
✅ Existující klienti neovlivněni
```

---

## ✅ ČÁST 7: APLIKACE BĚŽÍ BEZ RESTARTU (KRITICKÉ ●)

### Test 7.1: Několik her za sebou
**Požadavek**: ● Obě aplikace musí běžet bez nutnosti restartu

**Scénář**:

1. Spusť server a 2 klienty

2. **Hra 1**: Celá hra 4x4 board → konec → lobby

3. **Hra 2**: Vytvoř novou místnost 4x4 → celá hra → konec → lobby

4. **Hra 3**: Jiná velikost desky 6x6 → celá hra → konec → lobby

5. **Během**: Testuj reconnect, simuluj výpadky

**Co ukázat cvičícímu**:
```
✅ 3+ her za sebou bez restartu serveru
✅ 3+ her za sebou bez restartu klientů
✅ Různé velikosti desek (4x4, 6x6)
✅ Různé počty hráčů (2, 3, 4)
✅ Server log rostoucí (ne resetující se)
✅ Žádné memory leaky
✅ Žádné zpomalování
```

---

## ✅ ČÁST 8: NON-BLOCKING UI (KRITICKÉ ●)

### Test 8.1: UI nezamrzá při připojení
**Požadavek**: ● UI není závislé na odezvě protistrany

**Scénář**:

1. Spusť klienta

2. Zadej **neexistující IP** (např. 192.168.255.255)

3. Klikni CONNECT

4. **Během timeoutu** (10s):
   - Pohni oknem
   - Zkus kliknout na jiné prvky
   - Zkontroluj že okno reaguje

**Co ukázat cvičícímu**:
```
✅ Okno se pohybuje normálně během připojení
✅ Tlačítko CONNECT disabled (aby šlo vidět že probíhá akce)
✅ Žádné "Not responding"
✅ Po timeoutu: "ERROR: Failed to connect: Connection timed out"
✅ UI zůstává responzivní
✅ Lze zkusit připojit znovu
```

---

### Test 8.2: Informace o nedostupnosti serveru
**Požadavek**: ● Klient viditelně informuje o nedostupnosti serveru

**Při startu hry**:
```
✅ Login screen: "ERROR: Failed to connect: <důvod>"
✅ Červený text v log konzoli
```

**V lobby**:
```
✅ Při odpojení: "Disconnected from server - attempting to reconnect..."
✅ Status label změní barvu (červená)
```

**Ve hře**:
```
✅ Dialog: "Connection to server lost"
✅ Status: "Reconnecting... (attempt X/5)"
✅ Po neúspěchu: "Connection failed. Returning to login."
```

---

## ✅ ČÁST 9: AKTUÁLNÍ STAV HRY (KRITICKÉ ●)

### Test 9.1: Zobrazení stavu
**Požadavek**: ● Klient vždy ukazuje aktuální stav hry

**Co musí být vidět v GUI**:

**Lobby**:
```
✅ Seznam místností: název, počet hráčů (2/4), velikost desky
✅ Tlačítka: Refresh, Create Room, Join Room
```

**Room (před hrou)**:
```
✅ Název místnosti
✅ Seznam hráčů v místnosti
✅ Ready stavy jednotlivých hráčů
✅ Tlačítko START GAME (jen pro ownera)
✅ Tlačítko READY
```

**Hra**:
```
✅ Hrací pole se všemi kartami
✅ Otočené karty zobrazují hodnotu
✅ Matched karty viditelně odlišené (zelená, skryté)
✅ Seznam hráčů se skóre:
   Player1: 3 🏆
   Player2: 2
✅ Status label:
   "It's your turn!" (zelená) nebo
   "Player2's turn" (žlutá)
✅ Connection status: "Connected" (zelená)
✅ Kdo je nedostupný (pokud někdo odpojený)
```

---

## ✅ ČÁST 10: LOGGING (KRITICKÉ ●)

### Test 10.1: Server log
**Požadavek**: ● Obě aplikace mají nějakou formu záznamu

**Zkontroluj server.log**:

```bash
cat server_src/server.log
```

**Musí obsahovat**:
```
✅ [INFO] Logger initialized
✅ [INFO] Server started, waiting for connections...
✅ [INFO] New connection from 127.0.0.1:xxxxx (fd=5)
✅ [INFO] Client 1 authenticated as 'Player1'
✅ [INFO] Room created: id=1, name='MyRoom'
✅ [INFO] Client 2 (Player2) joined room 1
✅ [INFO] Game started, first player: Player1
✅ [INFO] Room 1: Player Player1 flipped card 5 (value=3)
✅ [INFO] MATCH! Player Player1 matched cards...
✅ [INFO] Game finished! All pairs matched
✅ [WARN] Client X: Error sent - INVALID_COMMAND (count: 1/3)
✅ [ERROR] Client X: Max error count reached
✅ [INFO] DEBUG: Client X disconnected...
✅ [INFO] Client X: Reconnection successful
```

---

### Test 10.2: Klient console output
**Požadavek**: Logging na klientovi

**Spusť klienta z terminálu a sleduj output**:
```bash
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**Musí obsahovat**:
```
✅ Sent: HELLO Player1
✅ Received: WELCOME 1 Player1
✅ Received: ROOM_LIST ...
✅ Game received: GAME_START ...
✅ Game received: YOUR_TURN
✅ Sent: FLIP 5
✅ Game received: CARD_REVEAL 5 3 Player1
✅ DEBUG: Auto-reconnect triggered at 17:58:09
✅ Reconnect successful!
✅ Connection error: <popis>
```

---

## ✅ ČÁST 11: VALIDACE UŽIVATELSKÝCH VSTUPŮ (KRITICKÉ ●)

### Test 11.1: Klient-side validace
**Požadavek**: ● Všechny uživatelské vstupy ošetřeny na nevalidní hodnoty

**Login screen - zkus zadat**:

1. **Prázdný nickname** → "ERROR: Please enter nickname"
2. **Nickname se spacemi** → "ERROR: Nickname can only contain..."
3. **Nickname s < >** → "ERROR: Nickname can only contain..."
4. **Nevalidní IP** → "ERROR: Invalid IP address format"
5. **Port < 1** → "ERROR: Port must be between 1 and 65535"
6. **Port > 65535** → "ERROR: Port must be between 1 and 65535"
7. **Port = "abc"** → "ERROR: Invalid port number"

**Create Room - zkus**:

1. **Prázdný název** → Tlačítko disabled nebo chyba
2. **Max players < 2** → Error nebo výběr omezen
3. **Max players > 4** → Error nebo výběr omezen
4. **Board size 3** → Jen 4, 6, 8 dostupné

**Co ukázat cvičícímu**:
```
✅ Všechny nevalidní vstupy odmítnuty JIŽ NA KLIENTOVI
✅ Nic nevalidního se nedostane na server
✅ Červená chybová hlášení viditelná
✅ Input fields mění barvu při chybě
```

---

## ✅ ČÁST 12: STRUKTUROVANÝ KÓD A DOKUMENTACE (KRITICKÉ ●)

### Test 12.1: Ukázka struktury kódu
**Požadavek**: ● Kód vhodně strukturovaný do modulů/tříd
**Požadavek**: ● Kód dostatečně dokumentovaný komentáři

**Server (C moduly)**:
```bash
ls -la server_src/

✅ main.c           - Entry point, argument parsing
✅ server.c/.h      - Server loop, PING/timeout threads
✅ client_handler.c/.h  - Command handling
✅ client_list.c/.h - Client management
✅ room.c/.h        - Room management
✅ game.c/.h        - Game logic
✅ logger.c/.h      - Logging
✅ protocol.h       - Protocol constants
✅ Makefile         - Build system
```

**Klient (Java třídy)**:
```bash
ls -la client_src/src/main/java/cz/zcu/kiv/ups/pexeso/

✅ Main.java
✅ controller/
   - LoginController.java
   - LobbyController.java
   - GameController.java
✅ network/
   - ClientConnection.java
   - MessageListener.java
✅ protocol/
   - ProtocolConstants.java
✅ model/
   - Room.java
✅ pom.xml          - Maven build
```

**Ukázka dokumentace v kódu**:
```c
/**
 * Handles RECONNECT command from client
 * Validates old client exists, transfers state to new client
 * Updates room/game pointers to prevent use-after-free
 *
 * @param new_client - Newly connected client attempting reconnect
 * @param params - "old_client_id" to reconnect as
 */
static void handle_reconnect(client_t *new_client, const char *params) {
    ...
}
```

---

## ✅ ČÁST 13: MEMORY LEAKS (DOPORUČENO)

### Test 13.1: Valgrind na serveru
**Požadavek**: Je vhodné zkusit, zda neuniká paměť

```bash
cd server_src

# Kompiluj s debug symboly:
make clean
gcc -Wall -Wextra -pthread -g main.c server.c client_handler.c client_list.c logger.c room.c game.c -o server

# Spusť s valgrind:
valgrind --leak-check=full --show-leak-kinds=all ./server 0.0.0.0 10000 10 50
```

**Během běhu**:
1. Připoj několik klientů
2. Vytvoř místnosti
3. Zahraj pár her
4. Testuj reconnect
5. Ukončí server (Ctrl+C)

**Očekávaný výsledek**:
```
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
  total heap usage: 150 allocs, 150 frees, 10,240 bytes allocated

LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
     possibly lost: 0 bytes in 0 blocks
   still reachable: 0 bytes in 0 blocks
         suppressed: 0 bytes in 0 blocks

All heap blocks were freed -- no leaks are possible

ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```

**Co ukázat cvičícímu**:
```
✅ Žádné "definitely lost" memory leaky
✅ Žádné "Invalid read/write"
✅ Žádné "Use of uninitialized value"
✅ Clean exit
```

---

## 📋 KONTROLNÍ SEZNAM PRO ODEVZDÁNÍ

Před příchodem na odevzdání PROJDI TENTO CHECKLIST:

### Příprava (den předem):
- [ ] Přelož server i klienta na Linux i Windows v UC-326
- [ ] Spusť alespoň 1 celou hru na obou systémech
- [ ] Zkontroluj že máš všechny soubory
- [ ] Ověř že dokumentace je kompletní
- [ ] Přečti si znovu PozadavkyUPS.pdf

### Build systém:
- [ ] `make` přeloží server jedním příkazem
- [ ] `mvn clean package` přeloží klienta jedním příkazem
- [ ] Žádné external networking libs (jen BSD sockets, java.net.Socket)

### Server:
- [ ] Přijímá 4 argumenty: IP PORT MAX_ROOMS MAX_CLIENTS
- [ ] Běží na zadaném IP a portu (ne hardcoded)
- [ ] Limity místností a klientů fungují
- [ ] Paralelní místnosti bez ovlivnění
- [ ] Loguje do server.log (nový při každém spuštění)
- [ ] Odpojí po 3 nevalidních zprávách
- [ ] Zvládá random data z /dev/urandom
- [ ] Validuje neplatná data (FLIP -1)
- [ ] Validuje stav (FLIP když není ve hře)
- [ ] Validuje game rules (karta už otočená)
- [ ] PING/PONG keepalive (5s interval)
- [ ] Reconnect timeout 90s
- [ ] Forfeit po long timeout

### Klient:
- [ ] JavaFX GUI (ne konzole)
- [ ] Zadání IP a portu při připojení
- [ ] Non-blocking UI (okno se pohybuje během připojení)
- [ ] Validace nickname (jen [a-zA-Z0-9_-])
- [ ] Validace IP formátu
- [ ] Validace portu (1-65535)
- [ ] Zobrazuje lobby se seznamem místností
- [ ] Zobrazuje stav hry (pole, skóre, tah)
- [ ] Automatický reconnect (5× po 5s)
- [ ] Dialog při SHORT disconnect protihráče
- [ ] Dialog při LONG disconnect protihráče
- [ ] Chybová hláška při nedostupnosti serveru
- [ ] Automatický návrat do lobby po hře (bez dialogu)
- [ ] Console output s přijatými/odeslanými zprávami

### Reconnect:
- [ ] Krátkodobý výpadek (iptables DROP) → automatický reconnect
- [ ] Krátkodobý výpadek (iptables REJECT) → automatický reconnect
- [ ] Dlouhodobý výpadek > 90s → forfeit + návrat do lobby
- [ ] Oba hráči vidí notifikace o disconnectu
- [ ] Server čeká 90s před forfeit
- [ ] Klient 5 pokusů po 5s = 25s total
- [ ] GAME_STATE obnovuje stav po reconnectu

### Testy:
- [ ] Celá hra 2 hráčů bez výpadků
- [ ] Celá hra s iptables DROP výpadkem
- [ ] Celá hra s iptables REJECT výpadkem
- [ ] Long timeout > 90s → forfeit
- [ ] Random data z /dev/urandom
- [ ] Nevalidní data (FLIP -1)
- [ ] Příkazy ve špatném stavu
- [ ] 3 souběžné místnosti
- [ ] 3+ her za sebou bez restartu
- [ ] Valgrind bez memory leaků

---

## 🎬 SCÉNÁŘ ODEVZDÁNÍ (30 minut)

**Co bude cvičící chtít vidět (v tomto pořadí)**:

### 1. Překlad (5 min)
```bash
# Server
cd server_src && make clean && make

# Klient Linux
cd client_src && mvn clean package

# Klient Windows
cd client_src && build-client.bat
```

### 2. Základní hra (5 min)
- Spuštění serveru s parametry
- 2 klienti (Linux + Windows)
- Celá hra od začátku do konce
- Automatický návrat do lobby

### 3. Reconnect (10 min)
- Rozehraná hra
- iptables DROP
- Automatický reconnect obou klientů
- Hra pokračuje

### 4. Nevalidní data (5 min)
- Random data z /dev/urandom → 3 chyby → disconnect
- FLIP -1 → ERROR INVALID_MOVE
- Příkaz ve špatném stavu → ERROR

### 5. Paralelní místnosti (5 min)
- 3 místnosti současně
- Tahy ve všech nezávisle
- Žádné ovlivnění

### 6. Otázky a dokumentace (5 min)
- Prohlédnutí kódu
- Kontrola dokumentace
- Struktura projektu
- Použité technologie

---

## ⚠️ ČASTÉ CHYBY A JAK SE JIM VYHNOUT

1. **Server padá při odpojení klienta během hry**
   - Řešení: Ošetřit is_disconnected flag, nepoužívat dangling pointers

2. **Klient zamrzá při připojení**
   - Řešení: Síťové operace v separátním threadu

3. **Memory leaky**
   - Řešení: Valgrind před odevzdáním, free všech malloc

4. **Reconnect nefunguje**
   - Řešení: Server čeká 90s, klient 5× po 5s

5. **Hra neskončí po odpojení**
   - Řešení: Forfeit logika po RECONNECT_TIMEOUT

6. **Nevalidní data crashnou server**
   - Řešení: Validace VŠECH vstupů, error counter

7. **Žádný log**
   - Řešení: Logger do souboru + stdout

8. **Hardcoded IP/port**
   - Řešení: Command-line argumenty

---

## 📝 FINÁLNÍ RADA

**DEN PŘED ODEVZDÁNÍM**:
1. Přijď do UC-326
2. Přelož server a klienta
3. Zahraj si celou hru
4. Zkus reconnect
5. Zkontroluj že vše funguje

**Pokud něco nefunguje**:
- NEPANIKAŘ
- Zkontroluj IP adresy (127.0.0.1 vs. IP stroje)
- Zkontroluj firewall (`sudo ufw status`)
- Zkontroluj že server běží (`ps aux | grep server`)
- Zkontroluj port (`netstat -tuln | grep 10000`)

**Na odevzdání si přines**:
- Vytištěnou dokumentaci
- USB s projektem (backup)
- Papír a tužku na poznámky

Hodně štěstí! 🍀
