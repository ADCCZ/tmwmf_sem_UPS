# Manuální testování serveru s Java klientem

Tento dokument obsahuje kompletní návod na testování Pexeso serveru pomocí **skutečného Java klienta** a InTCPtoru.
Všechny testy jsou **manuální** - bez použití bash skriptů, pouze otevírání terminálů/oken a klikání v GUI.

---

## Příprava prostředí

### 1. Zkompilování serveru (Linux/WSL)
```bash
cd server_src
make clean
make
```

### 2. Zkompilování Java klienta (Windows/Linux)

**Windows:**
```cmd
cd client_src
mvn clean package
```

**Linux/WSL:**
```bash
cd client_src
mvn clean package
```

Zkontrolujte, že vznikl soubor: `client_src/target/pexeso-client-1.0-SNAPSHOT.jar`

### 3. Příprava InTCPtoru (Linux/WSL)

InTCPtor je **LD_PRELOAD knihovna**, která hooká BSD socket funkce a přidává zpoždění, fragmentaci a dropování spojení.

**Build InTCPtoru:**
```bash
cd /cesta/k/intcptor
./build_and_env.sh
```

nebo manuálně:
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

**Ověření:**
Měli byste mít soubory:
- `intcptor-run` (runner executable)
- `libintcptor-overrides.so` (shared library)

### 4. Kontrola konfiguračních souborů InTCPtoru

InTCPtor vytvoří defaultní konfiguraci `intcptor_config.cfg` při prvním spuštění.

Pro testy vytvořte dva konfigurační soubory:

**intcptor_config.cfg** - základní fragmentace:
```ini
Send__1B_Sends = 0.1
Send__2B_Sends = 0.1
Send__2_Separate_Sends = 0.3
Send__2B_Sends_And_Second_Send = 0.2
Recv__1B_Less = 0.1
Recv__2B_Less = 0.1
Recv__Half = 0.3
Recv__2B = 0.2
Send_Delay_Ms_Mean = 100
Send_Delay_Ms_Sigma = 10
Drop_Connections = 0
Log_Enabled = 1
```

**intcptor_drop_config.cfg** - fragmentace + dropování spojení:
```ini
Send__1B_Sends = 0.1
Send__2B_Sends = 0.1
Send__2_Separate_Sends = 0.3
Send__2B_Sends_And_Second_Send = 0.2
Recv__1B_Less = 0.1
Recv__2B_Less = 0.1
Recv__Half = 0.3
Recv__2B = 0.2
Send_Delay_Ms_Mean = 100
Send_Delay_Ms_Sigma = 10
Drop_Connections = 1
Drop_Connection_Delay_Ms_Min = 5000
Drop_Connection_Delay_Ms_Max = 15000
Log_Enabled = 1
```

---

## Jak funguje InTCPtor

**DŮLEŽITÉ:** InTCPtor **NENÍ proxy server** na jiném portu!

InTCPtor je **LD_PRELOAD wrapper**, který se aplikuje přímo na spouštěný program:

```bash
# Server BEZ InTCPtoru
./pexeso_server 127.0.0.1 10000 10 50

# Server S InTCPtorem
./intcptor-run ./pexeso_server 127.0.0.1 10000 10 50
```

**Co InTCPtor dělá:**
- Hooká funkce: `socket`, `accept`, `recv`, `send`, `read`, `write`, `close`
- Fragmentuje příchozí zprávy (`recv`) - server obdrží zprávy po částech
- Fragmentuje odchozí zprávy (`send`) - klienti obdrží odpovědi po částech
- Přidává náhodná zpoždění (průměr 100ms, sigma 10ms)
- Může náhodně dropnout spojení (pokud `Drop_Connections = 1`)

**Klienti se připojují NORMÁLNĚ** na port 10000 - InTCPtor pracuje "uvnitř" serveru.

---

## Jak spustit více instancí Java klienta

### Windows:
Otevřete **více příkazových řádků** (CMD nebo PowerShell):

```cmd
cd client_src
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

Každé okno spustí novou instanci klienta.

### Linux/WSL:
```bash
cd client_src
java -jar target/pexeso-client-1.0-SNAPSHOT.jar &
java -jar target/pexeso-client-1.0-SNAPSHOT.jar &
```

Nebo otevřete více terminálů a v každém spusťte:
```bash
cd client_src
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

---

## TEST 1: Základní funkčnost serveru BEZ InTCPtoru

**Cíl:** Ověřit, že server funguje správně v ideálních síťových podmínkách s Java klientem.

### Krok 1: Spusťte server

**Terminál 1 (Linux/WSL) - Server:**
```bash
cd server_src
./pexeso_server 127.0.0.1 10000 10 50
```

**Očekávaný výstup:**
```
[INFO] Server initialized: 127.0.0.1:10000 (max_rooms=10, max_clients=50)
[INFO] PING thread started
[INFO] Timeout checker thread started
[INFO] Server started, waiting for connections...
```

### Krok 2: Spusťte první klienta

**Okno 2 (Windows CMD):**
```cmd
cd client_src
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**Zobrazí se Login obrazovka.**

**V GUI proveďte:**
1. Server IP: `127.0.0.1` (už vyplněno)
2. Server Port: `10000` (už vyplněno)
3. Nickname: Změňte na `Alice`
4. Klikněte **CONNECT**

**Očekávaný výsledek:**
- V logu uvidíte: `Connected to server`
- Objeví se **Lobby obrazovka** s nápisem "Welcome, Alice!"
- Seznam místností bude prázdný: `No rooms available`

**V Server logu:**
```
[INFO] Client 1: Handler thread started (fd=4)
[INFO] Client 1: Received message: 'HELLO Alice'
[INFO] Client 1 authenticated as 'Alice'
```

### Krok 3: Alice vytvoří místnost

**V Lobby klienta Alice:**
1. Klikněte tlačítko **Create Room**
2. V dialogu zadejte:
   - Room name: `TestRoom`
   - Klikněte OK
3. Vyberte **Max Players: 2**
4. Vyberte **Board Size: 4x4**

**Očekávaný výsledek:**
- Přepne se na **Game obrazovku**
- Nahoře uvidíte: `Room: TestRoom`
- Tlačítko **Start Game** (zelené) - aktivní
- Tlačítko **Leave Room** (červené)
- Vpravo: Player List s "Alice (0 points)"
- Status: `Waiting for players...`

**Server log:**
```
[INFO] Client 1 (Alice) created room 1
```

### Krok 4: Spusťte druhého klienta (Bob)

**Okno 3 (Windows CMD):**
```cmd
cd client_src
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V GUI druhého klienta:**
1. Nickname: `Bob`
2. Klikněte **CONNECT**

**V Lobby Boba:**
- Uvidíte seznam místností:
  ```
  Room #1: TestRoom (1/2) - 4x4 - WAITING
  ```
3. Klikněte na místnost v seznamu (zvýrazní se)
4. Klikněte **Join Room**

**Očekávaný výsledek:**
- Bob přejde na Game obrazovku
- V Player List: "Alice (0 points)" a "Bob (0 points)"

**V Alice GUI:**
- V Player List se objeví "Bob (0 points)"
- V logu: `Player Bob joined the room`

### Krok 5: Alice spustí hru

**V klientu Alice:**
1. Klikněte **Start Game** (zelené tlačítko)
2. Karta grid se vytvoří (4x4 = 16 karet)
3. Objeví se tlačítko **Ready** (modré)
4. Klikněte **Ready**

**V klientu Bob:**
1. Karta grid se vytvoří
2. Objeví se tlačítko **Ready**
3. Klikněte **Ready**

**Očekávaný výsledek po obou READY:**
- Oba klienti: Status `Game started!`
- Jeden z nich dostane: `YOUR TURN`
- Karty se stanou klikatelné pro hráče na tahu
- Pro druhého hráče: `Waiting for Alice's turn...`

### Krok 6: Zahrajte kompletní hru

**Hráč na tahu:**
1. Klikněte na libovolnou kartu → otočí se (ukáže číslo)
2. Klikněte na druhou kartu → otočí se

**Pokud jsou shodné:**
- Status: `MATCH! Alice scored!`
- Skóre Alice se zvýší
- Alice má další tah

**Pokud nejsou shodné:**
- Status: `No match. Next player's turn.`
- Karty se otočí zpět (po chvíli)
- Turn přejde na Boba

**Pokračujte, dokud se nenajdou všechny páry.**

**Očekávaný výsledek:**
- Objeví se dialog: `Game Over! Winner: Alice with 5 pairs`
- Oba klienti se vrátí do Lobby
- Místnost `TestRoom` zmizí ze seznamu

**Server log:**
```
[INFO] Room 1: Game finished, 1 winner(s)
[INFO] Client 1 (Alice) returned to lobby after game end
[INFO] Client 2 (Bob) returned to lobby after game end
```

### Ukončení testu
- Zavřete oba Java klienty
- V terminálu se serverem stiskněte `Ctrl+C`

✅ **Výsledek:** Kompletní herní cyklus funguje bez chyb.

---

## TEST 2: Server s InTCPtorem - fragmentace zpráv

**Cíl:** Otestovat, zda komunikace funguje i přes fragmentované TCP pakety.

### Krok 1: Spusťte server S InTCPtorem

**Terminál 1 (Linux/WSL):**

**Varianta A - Pomocí runneru (doporučeno):**
```bash
cd /cesta/k/intcptor/build
cp intcptor_config.cfg ./
./intcptor-run /cesta/k/server_src/pexeso_server 127.0.0.1 10000 10 50
```

**Varianta B - Pomocí LD_PRELOAD:**
```bash
cd server_src
LD_PRELOAD=/cesta/k/intcptor/build/libintcptor-overrides.so ./pexeso_server 127.0.0.1 10000 10 50
```

**Očekávaný výstup:**
```
[InTCPtor] Configuration loaded from intcptor_config.cfg
[InTCPtor] Overrides initialized
[INFO] Server initialized: 127.0.0.1:10000 (max_rooms=10, max_clients=50)
[INFO] Server started, waiting for connections...
```

**Co to dělá:**
- Server běží **normálně na portu 10000**
- InTCPtor hooká všechny socket operace serveru
- Zprávy od klientů jsou fragmentovány při `recv()` na straně serveru
- Odpovědi serveru jsou fragmentovány při `send()` před odesláním klientům

### Krok 2: Připojte Java klienty NORMÁLNĚ

**Okno 2 + Okno 3 (2x Java klient):**
```cmd
cd client_src
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V obou klientech:**
1. Server IP: `127.0.0.1`
2. Server Port: `10000` ← **STEJNÝ PORT** jako vždy!
3. Nickname: `Charlie` (první klient), `Diana` (druhý klient)
4. Klikněte **CONNECT**

### Krok 3: Vytvořte místnost a zahrajte hru

**Postup stejný jako v TEST 1:**
- Charlie vytvoří místnost "FragmentTest"
- Diana se připojí
- Charlie spustí hru
- Oba kliknou Ready
- Zahrajte několik tahů (FLIP karty)

**Co sledovat:**
- GUI by mělo fungovat **normálně** i přes fragmentaci
- Zprávy jsou správně doručeny (i když fragmentované)
- Status zprávy se zobrazují korektně
- Karty se otáčí správně

**V Server logu uvidíte InTCPtor logy:**
```
[InTCPtor] recv() split to 2B chunks
[InTCPtor] send() split to half
[INFO] Client 1: Received message: 'HELLO Charlie'
[INFO] Client 2: Received message: 'HELLO Diana'
[INFO] Client 1 (Charlie) created room 1
[InTCPtor] send() with 100ms delay
[INFO] Room 1: Player Diana flipped card 0 (value=3)
```

**Důležité pozorování:**
- Server logy ukazují **kompletní zprávy** (`HELLO Charlie`), ne fragmenty
- To znamená, že server správně skládá fragmentované `recv()` volání
- InTCPtor logy ukazují, jak byly zprávy fragmentovány

✅ **Výsledek:** Komunikace funguje správně i s fragmentací. Server správně skládá fragmenty.

### Ukončení testu
- Zavřete Java klienty
- Stiskněte `Ctrl+C` v terminálu se serverem

---

## TEST 3: PING/PONG mechanismus

**Cíl:** Ověřit, že server detekuje neodpovídající klienty a Java klient automaticky odpovídá na PING.

### Krok 1: Spusťte server (bez InTCPtoru)

**Terminál 1:**
```bash
cd server_src
./pexeso_server 127.0.0.1 10000 10 50
```

### Test A: Java klient - automatický PONG

**Okno 2 (Java klient):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**Připojte se jako `TestUser`.**

**Počkejte ~5 sekund.**

**V Server logu uvidíte:**
```
[INFO] PING sent to client 1 (TestUser)
[INFO] Client 1: PONG received
```

**V Java klientu:**
- PING/PONG probíhá **automaticky na pozadí**
- V logu klienta neuvidíte nic (PONG je tichý)
- Spojení zůstane aktivní

**Každých ~5 sekund se opakuje PING → PONG.**

### Test B: Simulace mrtvého spojení (netcat)

**Nyní použijeme netcat pro test timeoutu:**

**Okno 3:**
```bash
nc 127.0.0.1 10000
```
Zadejte:
```
HELLO DeadClient
```

**Počkejte na PING (přijde zpráva `PING`).**

**NEODPOVÍDEJTE** (nezadávejte `PONG`).

**Po 5 sekundách:**
```
[WARNING] Client 2 (DeadClient) didn't respond to PING within 5 seconds
[INFO] Client 2: Connection closed
```

Netcat se odpojí.

✅ **Výsledek:**
- Java klient automaticky odpovídá na PING
- Server detekuje mrtvé spojení do 10s (5s čekání + 5s timeout)

---

## TEST 4: Reconnect - Krátké odpojení (SHORT) protokol

**Cíl:** Ověřit, že reconnect protokol funguje správně.

**POZNÁMKA:** Java klient automaticky reconnectuje pouze po **skutečné ztrátě spojení** (network timeout).
Pro test samotného protokolu použijeme netcat. Pro realistický test viz TEST 6 (InTCPtor DROP).

### Fáze 1: Založení hry

**Terminál 1 (Server):**
```bash
cd server_src
./pexeso_server 127.0.0.1 10000 10 50
```

**Okno 2 (Alice - Java klient):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**Alice:**
1. Nickname: `Alice`
2. Connect
3. Create Room: `ReconnectTest`, 2 players, 4x4

**V Server logu poznamenejte si client_id Alice:**
```
[INFO] Client 1 authenticated as 'Alice'  ← client_id = 1
```

**Okno 3 (Bob - Java klient):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**Bob:**
1. Nickname: `Bob`
2. Connect
3. Join Room #1

**Alice:**
1. Start Game
2. Ready

**Bob:**
1. Ready

**Hra se spustí.** Jeden z hráčů dostane turn.

### Fáze 2: Simulace odpojení Alice

**V Okně 2 (Alice):**
- **Zavřete okno klienta** (klikněte na X)

**V Okně 3 (Bob) se zobrazí:**
- Status: `Player Alice disconnected (SHORT)`
- V logu: `Player Alice disconnected. Waiting for reconnection...`

**V Server logu:**
```
[INFO] Client 1 (Alice): Disconnected in room 1 - waiting for reconnect
[INFO] Client 1 (Alice): Will be removed after 120 seconds if not reconnected
```

**V Okně 3 (Bob):**
- Hra je **pozastavená**
- Karty jsou **neaktivní**
- Bob čeká na reconnect Alice

### Fáze 3: Manuální RECONNECT přes netcat

**Okno 4 (Terminál):**
```bash
nc 127.0.0.1 10000
```

Zadejte:
```
RECONNECT 1
```

(Kde `1` je client_id Alice z Fáze 1)

**Očekávaná odpověď:**
```
WELCOME 1 Reconnected successfully
GAME_STATE 2 0 Alice 0 Bob 0
```

(A případně `YOUR_TURN`, pokud byl turn Alice)

**V Okně 3 (Bob) se zobrazí:**
- Status: `Player Alice reconnected`
- Hra pokračuje

**Server log:**
```
[INFO] Client 3: Reconnecting as client 1 (Alice), disconnect duration: 15 seconds
[INFO] Client 1: Reconnection successful
[INFO] Client 1: Sent GAME_STATE for reconnection
```

✅ **Částečný výsledek:** Reconnect protokol funguje (ověřeno přes netcat).

**Pro realistický test automatického reconnectu Java klienta viz TEST 6 (InTCPtor DROP).**

---

## TEST 5: Reconnect timeout - Dlouhé odpojení (LONG)

**Cíl:** Ověřit, že server po 60 sekundách vyhodí hráče a ukončí hru.

### Postup

**Opakujte TEST 4 - Fázi 1 a 2** (založení hry a odpojení Alice).

**Nyní NEPŘIPOJUJTE Alice zpět.**

**Čekejte 1 minutu (60 sekund).**

### Co se stane po 60 sekundách

**V Okně 3 (Bob) se zobrazí:**
```
Player Alice disconnected (LONG)
Game ended due to disconnect
```

Bob se vrátí do Lobby.

**Server log:**
```
[WARNING] Client 1 (Alice) reconnect timeout expired after 60 seconds
[INFO] Room 1: Ending game due to player Alice disconnect timeout
[INFO] Client 2 (Bob) returned to lobby after game end (disconnect)
```

**Pokus o pozdní reconnect:**

**Okno 4 (Terminál):**
```bash
nc 127.0.0.1 10000
RECONNECT 1
```

**Odpověď:**
```
ERROR Session expired (timeout > 60s)
```

✅ **Výsledek:** Server správně vymaže klienta po 60 sekundách a ukončí hru.

---

## TEST 6: InTCPtor DROP - Automatický reconnect Java klienta

**Cíl:** Realistický test automatického reconnectu při náhodném dropování spojení.

**Toto je HLAVNÍ reconnect test!**

### Krok 1: Příprava konfigurace

Zkontrolujte, že máte soubor `intcptor_drop_config.cfg`:
```ini
Drop_Connections = 1
Drop_Connection_Delay_Ms_Min = 5000
Drop_Connection_Delay_Ms_Max = 15000
```

### Krok 2: Spusťte server S InTCPtorem DROP

**Terminál 1:**
```bash
cd /cesta/k/intcptor/build
cp intcptor_drop_config.cfg ./
./intcptor-run -c intcptor_drop_config.cfg /cesta/k/server_src/pexeso_server 127.0.0.1 10000 10 50
```

Nebo:
```bash
cd server_src
LD_PRELOAD=/cesta/k/intcptor/build/libintcptor-overrides.so ./pexeso_server 127.0.0.1 10000 10 50
```
(Ujistěte se, že `intcptor_drop_config.cfg` je v aktuálním adresáři)

**Očekávaný výstup:**
```
[InTCPtor] Configuration loaded from intcptor_drop_config.cfg
[InTCPtor] Drop_Connections enabled (5000-15000ms)
[INFO] Server initialized: 127.0.0.1:10000
```

**Co to dělá:**
- InTCPtor náhodně dropne **socket spojení** po 5-15 sekundách
- Simuluje náhlou ztrátu sítě

### Krok 3: Připojte 2 Java klienty

**Okno 2 + Okno 3 (2x Java klient):**
```cmd
cd client_src
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V obou klientech:**
- Server Port: `10000` (normálně!)
- Nickname: `Eve` a `Frank`
- Connect

### Krok 4: Založte hru

**Eve:**
1. Create Room: `DropTest`, 2 players, 4x4
2. Start Game
3. Ready

**Frank:**
1. Join Room #1
2. Ready

**Hra se spustí.**

### Krok 5: Sledujte automatický reconnect

**InTCPtor dropne spojení během 5-15 sekund.**

**Co se stane v GUI klienta, jehož spojení bylo dropnuto (např. Eve):**

1. **Okamžitě po DROP:**
   - V logu: `Connection lost. Attempting to reconnect...`
   - Status: `Disconnected from server`

2. **Po ~10 sekundách (první pokus):**
   - V logu: `Reconnecting... (attempt 1/5)`
   - Klient automaticky pošle: `RECONNECT <client_id>`

3. **Po úspěšném reconnectu:**
   - V logu: `Reconnected successfully!`
   - Status: `Connected`
   - Obdrží: `GAME_STATE` → board se obnoví
   - Pokud byl na tahu: `YOUR TURN` → může pokračovat

**V GUI druhého klienta (Frank):**
- V logu: `Player Eve disconnected (SHORT)`
- Po chvíli: `Player Eve reconnected`
- Hra pokračuje normálně

**Server log:**
```
[InTCPtor] Dropping connection (fd=4) after 8234ms
[INFO] Client 1 (Eve): Disconnected in room 1 - waiting for reconnect
[INFO] Client 1 (Eve): Connection closed
[INFO] Client 3: Handler thread started (fd=5)  ← nové spojení
[INFO] Client 3: Received message: 'RECONNECT 1'
[INFO] Client 3: Reconnecting as client 1 (Eve), disconnect duration: 12 seconds
[INFO] Client 1: Reconnection successful
[INFO] Client 1: Sent GAME_STATE for reconnection
```

### Krok 6: Pokračujte ve hře

**Po reconnectu:**
- Eve může normálně klikat na karty
- Hra funguje stejně jako před odpojením

**InTCPtor může dropnout spojení znovu** během dalších 5-15s - proces se opakuje (až 5 pokusů auto-reconnectu).

### Varianta: Opakované dropování

Pokud InTCPtor dropne spojení vícekrát:
- Klient se automaticky snaží reconnectovat až 5x
- Interval mezi pokusy: 10 sekund
- Po 5 neúspěšných pokusech: návrat do Login obrazovky

✅ **Výsledek:** Java klient automaticky reconnectuje po ztrátě spojení, hra plynule pokračuje.

---

## TEST 7: Nevalidní zprávy a error counter

**Cíl:** Ověřit, že server odpojí klienta po 3 chybných zprávách.

**Pro tento test použijeme netcat**, protože Java klient posílá pouze validní zprávy.

### Terminál 1 (Server)
```bash
cd server_src
./pexeso_server 127.0.0.1 10000 10 50
```

### Terminál 2 (Netcat)
```bash
nc 127.0.0.1 10000
```

Zadejte postupně:
```
HELLO TestUser
INVALID_COMMAND
ANOTHER_BAD_COMMAND
FLIP 999
```

**Očekávané odpovědi:**
```
WELCOME 1
ERROR INVALID_COMMAND INVALID_COMMAND
ERROR INVALID_COMMAND ANOTHER_BAD_COMMAND
ERROR INVALID_MOVE Card index out of bounds (0-15)
```

Po **3. chybě** server uzavře spojení. Netcat se odpojí.

**Server log:**
```
[WARNING] Client 1 (TestUser): Error sent - INVALID_COMMAND (count: 1/3)
[WARNING] Client 1 (TestUser): Error sent - INVALID_COMMAND (count: 2/3)
[WARNING] Client 1 (TestUser): Error sent - INVALID_MOVE (count: 3/3)
[ERROR] Client 1: Max error count reached, closing connection
```

✅ **Výsledek:** Server chrání před floodováním nevalidními příkazy.

---

## TEST 8: Random data z /dev/urandom (Linux)

**Cíl:** Otestovat odolnost proti náhodnému binárnímu vstupu.

### Terminál 1 (Server)
```bash
cd server_src
./pexeso_server 127.0.0.1 10000 10 50
```

### Terminál 2 (Random data)
```bash
cat /dev/urandom | head -c 1000 | nc 127.0.0.1 10000
```

**Co se stane:**
- Server obdrží 1000 náhodných bytů
- Server je zpracuje jako zprávy (hledá `\n` delimitery)
- Po několika chybách odpojí spojení

**Server log:**
```
[WARNING] Client 1: Message too long, truncating
[WARNING] Client 1 (unauthenticated): Error sent - INVALID_COMMAND (count: 1/3)
[WARNING] Client 1 (unauthenticated): Error sent - INVALID_COMMAND (count: 2/3)
[WARNING] Client 1 (unauthenticated): Error sent - INVALID_COMMAND (count: 3/3)
[ERROR] Client 1: Max error count reached, closing connection
```

✅ **Výsledek:** Server nepadá, pouze ukončí spojení. Žádný segfault.

---

## TEST 9: Více místností paralelně

**Cíl:** Ověřit, že server zvládá více her současně bez interference.

### Příprava: Spusťte server + 4 Java klienty

**Terminál 1 (Server):**
```bash
cd server_src
./pexeso_server 127.0.0.1 10000 10 50
```

**Okno 2, 3, 4, 5 (4x Java klient):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

### Místnost 1: Alice + Bob

**Okno 2 (Alice):**
1. Nickname: `Alice`
2. Connect
3. Create Room: `Room1`, 2 players, 4x4
4. Start Game
5. Ready

**Okno 3 (Bob):**
1. Nickname: `Bob`
2. Connect
3. Refresh Rooms → uvidíte "Room #1: Room1 (1/2)..."
4. Join Room #1
5. Ready

### Místnost 2: Charlie + Diana

**Okno 4 (Charlie):**
1. Nickname: `Charlie`
2. Connect
3. Create Room: `Room2`, 2 players, 4x4
4. Start Game
5. Ready

**Okno 5 (Diana):**
1. Nickname: `Diana`
2. Connect
3. Refresh Rooms → uvidíte oba Room1 a Room2
4. Join Room #2
5. Ready

### Test interference

**Obě hry nyní běží paralelně.**

**Zahrajte tahy v obou hrách střídavě:**

**Okno 2 (Alice v Room1):**
- Klikněte na kartu 0
- Klikněte na kartu 1

**Okno 4 (Charlie v Room2):**
- Klikněte na kartu 0
- Klikněte na kartu 1

**Okno 3 (Bob v Room1):**
- Klikněte na kartu 2
- Klikněte na kartu 3

**Ověření:**
- **Alice/Bob** vidí pouze karty z Room1
- **Charlie/Diana** vidí pouze karty z Room2
- Skóre jsou nezávislá
- Status zprávy se nemíchají
- Žádné zprávy z Room1 se neobjeví v Room2 a naopak

✅ **Výsledek:** Server správně izoluje místnosti, nedochází k race conditions.

---

## TEST 10: Memory leaky (Valgrind)

**Cíl:** Detekovat úniky paměti při připojení/odpojení klientů.

### Terminál 1 (Server s Valgrind)
```bash
cd server_src
valgrind --leak-check=full --show-leak-kinds=all ./pexeso_server 127.0.0.1 10000 10 50
```

**Poznámka:** Server poběží **výrazně pomaleji**.

### Okno 2, 3, 4 (Java klienti)

**Proveďte několik cyklů připojení a odpojení:**

**Cyklus 1:**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```
1. Connect jako `User1`
2. Create Room
3. Klikněte **Disconnect**
4. Zavřete okno

**Cyklus 2:**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```
1. Connect jako `User2`
2. Create Room
3. Zavřete okno (bez Disconnect)

**Cyklus 3:**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```
1. Connect jako `User3`
2. List Rooms
3. Klikněte **Disconnect**
4. Zavřete okno

**Opakujte 5-10x různé varianty.**

### Ukončení serveru a analýza

**V Terminálu 1 stiskněte `Ctrl+C`.**

**Valgrind vypíše report:**
```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 0 bytes in 0 blocks
==12345==   total heap usage: 1,234 allocs, 1,234 frees, 567,890 bytes allocated
==12345==
==12345== All heap blocks were freed -- no leaks are possible
```

**Výsledek:**
- ✅ **"All heap blocks were freed"** - žádné memory leaky
- ❌ **"definitely lost: X bytes"** - leak, nutno opravit

---

## TEST 11: Kompletní hra od začátku do konce

**Cíl:** Projít celý herní cyklus bez chyb.

### Terminál 1 (Server)
```bash
cd server_src
./pexeso_server 127.0.0.1 10000 10 50
```

### Okno 2 (Player1) + Okno 3 (Player2)
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

### Krok za krokem

**Player1:**
1. Connect jako `Player1`
2. Create Room: `FinalTest`, 2 players, 4x4
3. Start Game
4. Ready

**Player2:**
1. Connect jako `Player2`
2. Refresh → Join Room #1
3. Ready

### Hra

**Hrajte dokud není hra ukončena:**
- Klikejte na karty
- Hledejte páry
- Sledujte skóre

**Pokračujte, dokud neuvidíte dialog:**
```
Game Over!
Winner: Player1 with 5 pairs

Final Scores:
Player1: 5
Player2: 3
```

**Kontrola:**
- Oba klienti dostali `Game Over` dialog
- Součet skóre = počet párů (pro 4x4 = 8 párů)
- Po zavření dialogu: oba klienti v Lobby
- Místnost `FinalTest` zmizela ze seznamu
- Server nepadl, nepsal chyby do logu

✅ **Výsledek:** Kompletní herní cyklus funguje bez chyb.

---

## TEST 12: Validace vstupů v GUI

**Cíl:** Ověřit, že klient neposílá nevalidní data na server.

### Okno 1 (Java klient)
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

### Test A: Nevalidní IP adresa

**V Login obrazovce:**
1. Server IP: `999.999.999.999`
2. Klikněte **CONNECT**

**Očekávaný výsledek:**
- Zobrazí se chyba: `Invalid IP address format`
- CONNECT tlačítko nic neudělá
- Žádná zpráva není odeslána na server

### Test B: Nevalidní port

1. Server Port: `99999` (mimo rozsah 1-65535)
2. Klikněte **CONNECT**

**Očekávaný výsledek:**
- Chyba: `Port must be between 1 and 65535`
- Spojení se nepokusí navázat

### Test C: Nevalidní nickname

1. Nickname: `A` (příliš krátké, min 3 znaky)
2. Klikněte **CONNECT**

**Očekávaný výsledek:**
- Chyba: `Nickname must be 3-32 characters long`

1. Nickname: `User With Spaces` (obsahuje mezery)
2. Klikněte **CONNECT**

**Očekávaný výsledek:**
- Chyba: `Nickname can only contain letters, numbers, underscores and hyphens`

### Test D: Nevalidní room name

1. Připojte se validně
2. Create Room
3. Room name: `Room With Many Spaces And Long Name That Exceeds The Maximum Length`
4. Klikněte OK

**Očekávaný výsledek:**
- Chyba: `Room name too long (max 64 characters)`
- NEBO name je automaticky zkrácené

✅ **Výsledek:** Klient validuje vstupy před odesláním, server neobdrží nevalidní data.

---

## TEST 13: Owner Disconnect - Ownership Transfer (Lobby fáze)

**Cíl:** Ověřit, že pokud owner opustí místnost v lobby (před hrou), ownership se přenese na jiného hráče.

### Krok 1: Spusťte server

**Terminál 1:**
```bash
cd server_src
./pexeso_server 127.0.0.1 10000 10 50
```

### Krok 2: Alice vytvoří místnost

**Okno 2 (Java klient - Alice):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V GUI:**
1. Nickname: `Alice`
2. Connect
3. Create Room: `OwnerTest`, 4 players, 4x4

**Alice je nyní v místnosti jako owner.**

### Krok 3: Bob se připojí

**Okno 3 (Java klient - Bob):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V GUI:**
1. Nickname: `Bob`
2. Connect
3. Join Room #1

**V místnosti jsou nyní Alice (owner) a Bob.**

### Krok 4: Alice se odpojí (zavře okno)

**V Okně 2 (Alice):**
- Zavřete okno klienta (klikněte na X) nebo klikněte **Leave Room**

**V Okně 3 (Bob) se zobrazí:**
- V logu: `Player Alice left the room`
- Status: `You are now the room owner`
- Tlačítko **Start Game** se stane aktivní (zelené)

**Server log:**
```
[INFO] Client 1 (Alice) left room 1 (was owner)
[INFO] Room 1: Ownership transferred to Bob
[INFO] Room 1: Notifying players about ownership change
```

**Bob je nyní owner místnosti.**

### Krok 5: Ověření, že místnost stále existuje

**Okno 4 (Java klient - Charlie):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V GUI:**
1. Nickname: `Charlie`
2. Connect
3. Refresh Rooms

**Uvidíte:**
```
Room #1: OwnerTest (1/4) - 4x4 - WAITING
```

Místnost stále existuje s Bobem jako ownerem.

**Charlie může normálně Join Room #1.**

✅ **Výsledek:** Ownership se úspěšně přenesl na Boba, místnost nebyla zničena.

---

## TEST 14: Owner Disconnect - Room Destruction (Ready fáze, long timeout)

**Cíl:** Ověřit, že pokud owner odpojí během ready fáze a jeho timeout vyprší (60s), všichni hráči jsou vyhozeni a místnost je zničena.

### Krok 1: Spusťte server

**Terminál 1:**
```bash
cd server_src
./pexeso_server 127.0.0.1 10000 10 50
```

### Krok 2: Alice vytvoří místnost a Bob se připojí

**Okno 2 (Java klient - Alice):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V GUI:**
1. Nickname: `Alice`
2. Connect
3. Create Room: `TimeoutTest`, 2 players, 4x4

**Okno 3 (Java klient - Bob):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V GUI:**
1. Nickname: `Bob`
2. Connect
3. Join Room #1

### Krok 3: Alice spustí hru (přejde do Ready fáze)

**V Okně 2 (Alice):**
1. Klikněte **Start Game**
2. Objeví se karta grid a tlačítko **Ready**
3. **NEKLIKEJTE Ready ještě** (necháme místnost v "ready fázi")

**V Okně 3 (Bob):**
- Také vidíte karta grid a tlačítko **Ready**
- **NEKLIKEJTE Ready**

**Stav:** Místnost je po START_GAME, ale hra ještě nezačala (hráči neklikli Ready).

### Krok 4: Alice zavře okno (simulace odpojení)

**V Okně 2 (Alice):**
- Zavřete okno klienta (klikněte na X)

**V Okně 3 (Bob) se zobrazí:**
```
Player Alice disconnected (SHORT)
Waiting for reconnection...
```

**Server log:**
```
[INFO] Client 1 (Alice): Disconnected in room 1 - waiting for reconnect
[INFO] Client 1 (Alice): Will be removed after 60 seconds if not reconnected
```

### Krok 5: Čekejte 60 sekund (LONG timeout)

**Nereconnectujte Alice. Čekejte 1 minutu.**

### Co se stane po 60 sekundách

**V Okně 3 (Bob) se zobrazí:**
```
Player Alice disconnected (LONG)
Player Alice left the room
```

**Server log:**
```
[WARNING] Client 1 (Alice) reconnect timeout expired after 60 seconds
[INFO] Room 1: Player Alice disconnect timeout (no active game)
[INFO] Client 1 (Alice) left room 1 (was owner)
[INFO] Room 1: Ownership transferred to Bob
```

**Výsledek:**
- Bob zůstává v místnosti jako nový owner
- Místnost není zničena, protože Bob je stále připojen
- Bob může pokračovat (čekat na další hráče nebo opustit místnost)

**POZNÁMKA:** Pokud by Bob byl také odpojený (všichni hráči odpojeni), místnost by byla zničena.

✅ **Výsledek:** Timeout správně přenese ownership nebo zničí místnost, pokud nejsou žádní aktivní hráči.

---

## TEST 15: Reconnect Během Aktivní Hry - Soupeř Pokračuje

**Cíl:** Ověřit, že pokud se hráč odpojí během hry, může se znovu připojit i když soupeř mezitím odehraje své tahy.

**Tento test používá iptables DROP pro realistickou simulaci.**

### Příprava: Poznamenejte si WSL IP adresu

**Na WSL (Linux):**
```bash
ip addr show eth0 | grep inet
```

Poznamenejte si IP (např. `172.28.160.1`). Tuto IP budete používat v iptables pravidlech.

### Krok 1: Spusťte server

**Terminál 1 (WSL/Linux):**
```bash
cd server_src
./pexeso_server 0.0.0.0 10000 10 50
```

### Krok 2: Alice a Bob založí hru

**Okno 2 (Windows - Alice):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V GUI:**
1. Server IP: `<WSL_IP>` (např. 172.28.160.1)
2. Nickname: `Alice`
3. Connect
4. Create Room: `ActiveGame`, 2 players, 4x4

**Okno 3 (Windows - Bob):**
```cmd
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

**V GUI:**
1. Server IP: `<WSL_IP>`
2. Nickname: `Bob`
3. Connect
4. Join Room #1

**Alice:**
1. Start Game
2. Ready

**Bob:**
1. Ready

**Hra se spustí.** Předpokládejme, že Alice dostane první turn.

### Krok 3: Alice odehraje jeden tah

**V Okně 2 (Alice):**
1. Klikněte na kartu 0 (ukáže např. číslo 5)
2. Klikněte na kartu 1 (ukáže např. číslo 3)
3. Neshodují se → MISMATCH
4. Turn přejde na Boba

**V Okně 3 (Bob):**
- Status: `YOUR TURN`
- Karty jsou aktivní

### Krok 4: Odpojte Alice přes iptables DROP

**Terminál 2 (WSL/Linux, sudo):**
```bash
# Najděte Alice IP adresu (z Windows ipconfig)
# Předpokládejme Alice má Windows IP 172.28.144.1

# Zablokujte komunikaci od Alice na port 10000
sudo iptables -A INPUT -p tcp -s 172.28.144.1 --dport 10000 -j DROP

# Zablokujte i odchozí komunikaci k Alice
sudo iptables -A OUTPUT -p tcp -d 172.28.144.1 --sport 10000 -j DROP
```

**V Okně 2 (Alice):**
- Po ~10 sekundách: `Connection lost. Attempting to reconnect...`
- Status: `Disconnected from server`

**V Okně 3 (Bob):**
- Status: `Player Alice disconnected (SHORT)`
- V logu: `Player Alice disconnected. Waiting for reconnection...`
- **Karty zůstávají aktivní** (Bob může pokračovat)

### Krok 5: Bob odehraje své tahy během Alice odpojení

**V Okně 3 (Bob):**
1. Klikněte na kartu 2 (ukáže např. číslo 7)
2. Klikněte na kartu 3 (ukáže např. číslo 7)
3. MATCH! Bob získá bod
4. Bob má další turn
5. Klikněte na kartu 4 (ukáže např. číslo 2)
6. Klikněte na kartu 5 (ukáže např. číslo 8)
7. MISMATCH → Turn přejde na Alice

**Bob nyní čeká na Alice.**

**Server log:**
```
[INFO] Room 1: Player Bob flipped card 2 (value=7)
[INFO] Room 1: Player Bob flipped card 3 (value=7)
[INFO] Room 1: MATCH! Bob scored
[INFO] Room 1: Player Bob flipped card 4 (value=2)
[INFO] Room 1: Player Bob flipped card 5 (value=8)
[INFO] Room 1: MISMATCH, next turn: Alice
```

### Krok 6: Obnovte Alice připojení (odstraňte iptables pravidla)

**Terminál 2 (WSL/Linux, sudo):**
```bash
# Odstraňte DROP pravidla
sudo iptables -D INPUT -p tcp -s 172.28.144.1 --dport 10000 -j DROP
sudo iptables -D OUTPUT -p tcp -d 172.28.144.1 --sport 10000 -j DROP

# Ověření, že pravidla jsou pryč
sudo iptables -L -n | grep 10000
```

### Krok 7: Alice se automaticky reconnectuje

**V Okně 2 (Alice):**
- Po ~10 sekundách: `Reconnecting... (attempt 1/5)`
- Klient automaticky pošle: `RECONNECT 1`

**Server log:**
```
[INFO] Client 3: Handler thread started (fd=5)
[INFO] Client 3: Received message: 'RECONNECT 1'
[INFO] Client 3: Reconnecting as client 1 (Alice), disconnect duration: 35 seconds
[INFO] Client 1: Reconnection successful
[INFO] Client 1: Sent GAME_STATE for reconnection
```

**V Okně 2 (Alice) po reconnectu:**
- Status: `Reconnected successfully!`
- Status: `YOUR TURN` (Alice je na tahu)
- **Karta grid se obnoví se správným stavem:**
  - Karty 2 a 3 (Bob's match) jsou **odstraněné** (matched pairs)
  - Všechny ostatní karty jsou **lícem dolů**
- **Skóre je aktualizované:**
  - Alice: 0 párů
  - Bob: 1 pár

**V Okně 3 (Bob):**
- Status: `Player Alice reconnected`
- V logu: `Alice reconnected successfully`

### Krok 8: Pokračujte ve hře

**V Okně 2 (Alice):**
- Alice může normálně pokračovat a klikat na karty
- Hra funguje stejně jako před odpojením

✅ **Výsledek:**
- Alice se úspěšně reconnectovala během aktivní hry
- Obdržela správný GAME_STATE se všemi tahy, které Bob odehrál během jejího odpojení
- Hra plynule pokračuje bez ztráty dat

---

## Užitečné Příkazy a Tipy

### Simulace síťových výpadků (Linux/WSL)

#### iptables DROP - Simulace ztráty spojení (doporučeno)

**Blokovat příchozí spojení na port:**
```bash
sudo iptables -A INPUT -p tcp --dport 10000 -j DROP
```

**Odstranit pravidlo:**
```bash
sudo iptables -D INPUT -p tcp --dport 10000 -j DROP
```

**Blokovat spojení od konkrétního klienta:**
```bash
# Najděte IP klienta (např. z Windows: ipconfig)
sudo iptables -A INPUT -p tcp -s 192.168.1.100 --dport 10000 -j DROP
sudo iptables -A OUTPUT -p tcp -d 192.168.1.100 --sport 10000 -j DROP
```

**Odstranit pravidla pro konkrétního klienta:**
```bash
sudo iptables -D INPUT -p tcp -s 192.168.1.100 --dport 10000 -j DROP
sudo iptables -D OUTPUT -p tcp -d 192.168.1.100 --sport 10000 -j DROP
```

#### iptables REJECT - Simulace odmítnutí spojení

**Rozdíl:**
- `DROP` = pakety jsou zahozeny, spojení timeout po několika sekundách
- `REJECT` = server okamžitě odmítne spojení s RST paketem

```bash
sudo iptables -A INPUT -p tcp --dport 10000 -j REJECT
sudo iptables -D INPUT -p tcp --dport 10000 -j REJECT  # Odstranit
```

#### Zobrazit všechna aktivní iptables pravidla

```bash
sudo iptables -L -n --line-numbers
```

#### Odstranit konkrétní pravidlo podle čísla

```bash
# Ukáže čísla řádků
sudo iptables -L INPUT -n --line-numbers

# Odstraní pravidlo číslo 3 z INPUT chain
sudo iptables -D INPUT 3
```

#### Flush všechna pravidla (reset)

```bash
sudo iptables -F
```

**VAROVÁNÍ:** Použijte opatrně - vymaže všechna iptables pravidla na systému!

### Valgrind - Detekce memory leaks

#### Základní použití

```bash
# Kompilace s debug symboly
cd server_src
make clean
gcc -g -Wall -Wextra -pthread *.c -o pexeso_server

# Spuštění s Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./pexeso_server 127.0.0.1 10000 10 50
```

#### Valgrind s podrobným logováním

```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --verbose \
         --log-file=valgrind-out.txt \
         ./pexeso_server 127.0.0.1 10000 10 50
```

#### Interpretace Valgrind výstupu

**Žádné leaky:**
```
==12345== HEAP SUMMARY:
==12345==     in use at exit: 0 bytes in 0 blocks
==12345==   total heap usage: 1,234 allocs, 1,234 frees
==12345==
==12345== All heap blocks were freed -- no leaks are possible
```

**Memory leak detekován:**
```
==12345== 256 bytes in 1 blocks are definitely lost
==12345==    at malloc (vg_replace_malloc.c:309)
==12345==    by room_create (room.c:93)
==12345==    by handle_create_room (client_handler.c:456)
```

**"Still reachable" (lze ignorovat):**
```
==12345== 72 bytes in 3 blocks are still reachable
```
To jsou pthread/glibc interní struktury - není to problém.

### Netcat - Testování protokolu

#### Připojení k serveru

```bash
nc 127.0.0.1 10000
```

**Zadejte zprávy ručně:**
```
HELLO TestUser
LIST_ROOMS
CREATE_ROOM TestRoom 2 4
```

#### Posílání náhodných dat

```bash
# Pošle 1000 náhodných bytů
cat /dev/urandom | head -c 1000 | nc 127.0.0.1 10000

# Nekonečný stream (testování max error count)
cat /dev/urandom | nc 127.0.0.1 10000
```

#### Testování PING/PONG timeoutu

```bash
nc 127.0.0.1 10000
HELLO DeadClient
# Čekejte na PING zprávu (server pošle za ~5s)
# NEODPOVÍDEJTE - server odpojí za dalších 5s
```

### Debugging - GDB a core dumps

#### Povolení core dumps

```bash
# Zjistit aktuální limit
ulimit -c

# Povolit neomezené core dumps
ulimit -c unlimited
```

#### Analýza core dump po pádu

```bash
# Server spadl → vytvoří se soubor "core" nebo "core.12345"
gdb ./pexeso_server core

# V GDB:
(gdb) bt              # Backtrace - ukáže stack trace
(gdb) frame 3         # Přepne na frame 3
(gdb) print variable  # Vypíše hodnotu proměnné
(gdb) info locals     # Zobrazí všechny lokální proměnné
(gdb) quit
```

#### Spuštění serveru přímo v GDB

```bash
gdb --args ./pexeso_server 127.0.0.1 10000 10 50

# V GDB:
(gdb) run             # Spustí program
# Program běží...
# Při pádu: automaticky breakpoint
(gdb) bt              # Stack trace
(gdb) info threads    # Zobrazí všechna vlákna
(gdb) thread 5        # Přepne na thread 5
(gdb) bt              # Stack trace threadu 5
```

### Monitoring - netstat, lsof, ps

#### Kontrola, zda server běží na portu

```bash
# Zobrazí všechny TCP spojení na portu 10000
netstat -an | grep 10000

# Nebo:
ss -tuln | grep 10000
```

**Očekávaný výstup (server běží):**
```
tcp   0   0 127.0.0.1:10000   0.0.0.0:*   LISTEN
```

#### Zjištění PID procesu na portu

```bash
lsof -i :10000
```

**Výstup:**
```
COMMAND     PID USER   FD   TYPE DEVICE SIZE/OFF NODE NAME
pexeso_se  1234 user    3u  IPv4  12345      0t0  TCP 127.0.0.1:10000 (LISTEN)
```

#### Zabití procesu, který blokuje port

```bash
# Zjistit PID
lsof -i :10000 | grep LISTEN

# Zabít proces
kill -9 1234
```

#### Zobrazení všech běžících pexeso_server procesů

```bash
ps aux | grep pexeso_server
```

### Make - Build příkazy

#### Kompilace s debug symboly

```bash
cd server_src

# Clean build
make clean
make

# Build s extra debug info
gcc -g -O0 -Wall -Wextra -pthread *.c -o pexeso_server
```

#### Kompilace s adresními sanitizery (detekce use-after-free)

```bash
gcc -g -fsanitize=address -fsanitize=undefined -Wall -Wextra -pthread *.c -o pexeso_server

# Spuštění
./pexeso_server 127.0.0.1 10000 10 50

# ASAN automaticky detekuje:
# - Use after free
# - Buffer overflows
# - Memory leaks
```

### Java klient - Build a spuštění

#### Build (Maven)

```bash
cd client_src
mvn clean package
```

**Výstup:** `target/pexeso-client-1.0-SNAPSHOT.jar`

#### Spuštění

```bash
# Windows CMD:
java -jar target/pexeso-client-1.0-SNAPSHOT.jar

# Linux/WSL:
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

#### Spuštění více instancí paralelně (Linux)

```bash
java -jar target/pexeso-client-1.0-SNAPSHOT.jar &
java -jar target/pexeso-client-1.0-SNAPSHOT.jar &
java -jar target/pexeso-client-1.0-SNAPSHOT.jar &
```

#### JavaFX problémy - alternativní spuštění

```bash
# Pokud "java -jar" nefunguje (chybějící JavaFX):
mvn javafx:run
```

### Logování a debugging

#### Server logy v reálném čase

```bash
# Spusťte server a sledujte výstup
./pexeso_server 127.0.0.1 10000 10 50 | tee server.log
```

**Výhoda:** Logy jsou viditelné v terminálu a zároveň ukládány do `server.log`.

#### Grep filtrování logů

```bash
# Zobrazit pouze ERROR zprávy
./pexeso_server 127.0.0.1 10000 10 50 | grep ERROR

# Zobrazit zprávy pro konkrétního klienta
./pexeso_server 127.0.0.1 10000 10 50 | grep "Client 1"

# Zobrazit reconnect události
./pexeso_server 127.0.0.1 10000 10 50 | grep -i reconnect
```

### Rychlé příkazy pro testování

#### 1-řádkový test: Connect + Disconnect

```bash
echo "HELLO TestUser" | nc 127.0.0.1 10000
```

#### Test PING timeout (automatický)

```bash
(echo "HELLO TimeoutUser"; sleep 20) | nc 127.0.0.1 10000
```

#### Test nevalidních zpráv

```bash
(echo "HELLO TestUser"; echo "INVALID_COMMAND"; echo "ANOTHER_BAD"; echo "THIRD_BAD") | nc 127.0.0.1 10000
```

#### Test max message length

```bash
python3 -c "print('HELLO ' + 'A'*2000)" | nc 127.0.0.1 10000
```

### WSL specifické příkazy

#### Zjištění WSL IP adresy

```bash
# Linux WSL IP (pro server bind)
ip addr show eth0 | grep "inet "

# Windows host IP (pro klienta)
ip route | grep default | awk '{print $3}'
```

#### Restart WSL síťování (pokud problémy s připojením)

```powershell
# V PowerShell (Windows, as Administrator):
wsl --shutdown
wsl
```

---

## Shrnutí testovacích scénářů

| Test | Testovaná funkcionalita | Nástroj | Očekávaný výsledek |
|------|-------------------------|---------|-------------------|
| **1** | Základní server bez InTCPtoru | Java klient | Kompletní hra funguje |
| **2** | InTCPtor fragmentace | Server+InTCPtor + Java klient | Fragmentace nezpůsobí problémy |
| **3** | PING/PONG mechanismus | Java klient + netcat | Auto PONG funguje, mrtvá spojení detekována |
| **4** | SHORT reconnect (<60s) | Netcat (manuální RECONNECT) | Protokol reconnectu funguje |
| **5** | LONG timeout (>60s) | Java klient | Hra ukončena, hráč vyhozen |
| **6** | InTCPtor DROP - Auto reconnect | Server+InTCPtor DROP + Java klient | Automatický reconnect funguje |
| **7** | Nevalidní zprávy | Netcat | Odpojení po 3 chybách |
| **8** | Random data | Netcat + /dev/urandom | Server nepadá |
| **9** | Paralelní místnosti | 4x Java klient | Žádná interference |
| **10** | Valgrind | Java klient + Valgrind | Žádné memory leaky |
| **11** | Kompletní hra | Java klient | Hra funguje od začátku do konce |
| **12** | Validace GUI vstupů | Java klient | Nevalidní data nejsou odeslána |
| **13** | Owner disconnect - ownership transfer | Java klient | Ownership přechází na jiného hráče |
| **14** | Owner disconnect - long timeout | Java klient | Timeout přenese ownership nebo zničí room |
| **15** | Reconnect během aktivní hry | Java klient + iptables | Hráč se reconnectuje a obdrží GAME_STATE |

---

## Kontrolní checklist před odevzdáním

### Server
- [ ] Server běží stabilně alespoň 10 minut s aktivními klienty
- [ ] Reconnect funguje ve všech případech (<120s)
- [ ] PING/PONG detekuje mrtvá spojení
- [ ] Fragmentace zpráv (InTCPtor) nepůsobí problémy
- [ ] Nevalidní zprávy jsou ošetřeny (max 3 errory)
- [ ] Valgrind nehlásí memory leaky
- [ ] Paralelní hry běží bez interference
- [ ] Random data (/dev/urandom) nezpůsobí pád serveru
- [ ] Logy obsahují všechny důležité události

### Java klient
- [ ] GUI se spouští bez chyb na Windows i Linux
- [ ] Validace vstupů funguje (IP, port, nickname, room name)
- [ ] Připojení k serveru funguje
- [ ] Lobby zobrazuje seznam místností
- [ ] Vytvoření a připojení do místnosti funguje
- [ ] Game board se vykresluje správně (4x4, 6x6, 8x8)
- [ ] Klikání na karty funguje
- [ ] MATCH/MISMATCH logika správná
- [ ] Skóre se aktualizuje
- [ ] Game Over dialog se zobrazí
- [ ] Automatický PONG funguje
- [ ] Automatický reconnect funguje (InTCPtor DROP test)
- [ ] Disconnect a návrat do Login funguje

---

## Časté problémy a řešení

### Server hned odmítne připojení
- **Příčina:** Port už používá jiný proces
- **Řešení:** `lsof -i :10000` a `kill <PID>` nebo změňte port

### Java klient: "Could not find or load main class"
- **Příčina:** Chybně zbuildovaný JAR
- **Řešení:** `mvn clean package` a zkuste znovu

### Java klient: "JavaFX runtime components are missing"
- **Příčina:** Chybějící JavaFX v JDK
- **Řešení:** Použijte `mvn javafx:run` místo `java -jar`

### InTCPtor: "Configuration file not found"
- **Příčina:** `intcptor_config.cfg` není v aktuálním adresáři
- **Řešení:** Zkopírujte config do adresáře, odkud spouštíte intcptor-run

### InTCPtor: Žádné fragmentace se neděje
- **Příčina:** Server spuštěn BEZ InTCPtoru
- **Řešení:** Zkontrolujte, že vidíte `[InTCPtor]` logy při startu serveru

### Java klient: Connection timeout
- **Příčina:** Server neběží nebo špatný port
- **Řešení:** Zkontrolujte, že server běží: `netstat -an | grep 10000`

### Reconnect vrací "Client not found"
- **Příčina:** Uplynulo >120s nebo server byl restartován
- **Řešení:** Reconnectněte rychleji nebo se přihlaste znovu

### Valgrind hlásí "still reachable"
- **Příčina:** Pthread struktury nejsou uvolněny (normální při Ctrl+C)
- **Řešení:** "Still reachable" není critical leak, ignorujte

### GUI zamrzlo po FLIP
- **Příčina:** Blocking operace na GUI vlákně
- **Řešení:** Bug v klientu - síťové operace musí být na separátním vlákně

### Server spadne při FLIP
- **Příčina:** Race condition v game logice
- **Řešení:** Zajistěte správné mutexové zamykání v `game.c`

---

## Dodatečné poznámky

### Jak InTCPtor funguje

**InTCPtor NENÍ proxy server!**

Je to **LD_PRELOAD knihovna**, která hooká BSD socket funkce:

```
Normální server:        Server s InTCPtorem:
┌──────────┐           ┌──────────┐
│  Server  │           │  Server  │
│   recv() ├──┐        │   recv() ├──┐
│   send() │  │        │   send() │  │
└──────────┘  │        └──────────┘  │
              │                       │
              ▼                       ▼
         ┌─────────┐          ┌──────────────┐
         │  libc   │          │  InTCPtor    │
         │ socket  │          │  overrides   │
         └─────────┘          └──────────────┘
              │                       │
              ▼                       ▼
         ┌─────────┐          ┌──────────────┐
         │ Kernel  │          │  libc socket │
         └─────────┘          └──────────────┘
                                      │
                                      ▼
                              ┌──────────────┐
                              │    Kernel    │
                              └──────────────┘
```

**Co InTCPtor interceptuje:**
- `recv()` - fragmentuje příchozí data (klient→server)
- `send()` - fragmentuje odchozí data (server→klient)
- `accept()` - může dropnout spojení po N sekundách
- `close()`, `socket()` - trackování socketů

**Výhody:**
- ✅ Aplikuje se přímo na server proces
- ✅ Testuje skutečnou fragmentaci TCP streamu
- ✅ Lze použít na jakýkoliv C/C++ program

**Nevýhody:**
- ❌ Funguje pouze na Linux (LD_PRELOAD)
- ❌ Nelze použít na Java aplikace (JVM má vlastní sockety)
- ❌ Vyžaduje překlad do binárky (ne skripty)

### Rozdíl mezi Java klientem a netcat

**Java klient:**
- ✅ Realistické testování
- ✅ GUI validace vstupů
- ✅ Automatický PONG
- ✅ Automatický reconnect
- ❌ Nelze poslat nevalidní zprávy (pro edge case testování)
- ❌ Nelze použít s InTCPtorem (JVM izolace)

**Netcat:**
- ✅ Manuální kontrola nad každou zprávou
- ✅ Testování edge cases (nevalidní zprávy, chybějící PONG)
- ✅ Rychlé prototypování
- ❌ Nerealistické (uživatelé nepoužívají netcat)

**Doporučení:**
- Používejte **Java klienta pro všechny realistické testy**
- Používejte **InTCPtor na serveru** pro testování fragmentace a dropování
- Používejte **netcat pouze pro edge cases** (nevalidní zprávy, PONG timeout)

---

**Konec návodu**

Tento dokument poskytuje kompletní testování s **reálným Java klientem** a **správným použitím InTCPtoru**.
Všechny testy lze provést otevřením několika oken a klikáním v GUI + základní příkazy v terminálu.
