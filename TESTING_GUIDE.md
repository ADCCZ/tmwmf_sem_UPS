# KOMPLETNÍ TESTOVACÍ MANUÁL - Pexeso KIV/UPS

Tento dokument obsahuje kompletní testovací plán pokrývající **VŠECHNY požadavky z PozadavkyUPS.pdf**.

---

## OBSAH

1. [Příprava prostředí](#1-příprava-prostředí)
2. [Kompilace (požadavek: automatický překlad)](#2-kompilace)
3. [Spuštění s různými parametry](#3-spuštění-s-různými-parametry)
4. [Kompletní hra bez chyb](#4-kompletní-hra-bez-chyb)
5. [Nevalidní data (/dev/urandom)](#5-nevalidní-data)
6. [Neplatné příkazy a stavy](#6-neplatné-příkazy-a-stavy)
7. [Simulace výpadku sítě (iptables)](#7-simulace-výpadku-sítě)
8. [InTCPtor - zpoždění a fragmentace](#8-intcptor)
9. [Memory leaks (Valgrind)](#9-memory-leaks-valgrind)
10. [Paralelní místnosti](#10-paralelní-místnosti)
11. [Logging](#11-logging)
12. [GUI požadavky](#12-gui-požadavky)
13. [Finální checklist podle PozadavkyUPS.pdf](#13-finální-checklist)

---

## 1. PŘÍPRAVA PROSTŘEDÍ

### WSL (server)

Otevři WSL terminál:
```
wsl
```

Zkontroluj nástroje:
```
gcc --version
make --version
valgrind --version
nc -h
git --version
```

Pokud chybí:
```
sudo apt update
sudo apt install gcc make valgrind netcat-traditional git cmake build-essential
```

### Windows (klient)

Zkontroluj:
```
java -version
mvn -version
```

Potřebuješ Java 11+ a Maven 3.6+.

---

## 2. KOMPILACE

**POŽADAVEK:** Automatický překlad standardním nástrojem (make, maven)

### 2.1 Server (make)

Ve WSL:
```
cd /mnt/d/ZCU/UPS/tmwmf_sem_UPS/server_src
```

Vyčisti:
```
make clean
```

Zkompiluj:
```
make
```

**Očekávaný výstup:**
```
mkdir -p build
gcc -Wall -Wextra -pthread -g -std=c99 -c main.c -o build/main.o
...
Build successful: server
```

Zkontroluj binárku:
```
ls -la server
```

### 2.2 Klient (maven)

V CMD/PowerShell na Windows:
```
cd /d D:\ZCU\UPS\tmwmf_sem_UPS\client_src
```

Vyčisti a zkompiluj:
```
mvn clean compile
```

**Očekávaný výstup:**
```
BUILD SUCCESS
```

Spuštění:
```
mvn exec:java
```

---

## 3. SPUŠTĚNÍ S RŮZNÝMI PARAMETRY

**POŽADAVEK:** IP adresa a port nastavitelné (ne hardcoded)

### Test 3.1: Standardní spuštění

Ve WSL:
```
cd /mnt/d/ZCU/UPS/tmwmf_sem_UPS/server_src
./server 0.0.0.0 10000 10 50
```

**Parametry:** IP PORT MAX_ROOMS MAX_PLAYERS

### Test 3.2: Jiný port
```
./server 0.0.0.0 12345 5 20
```

### Test 3.3: Localhost
```
./server 127.0.0.1 10000 10 50
```

### Test 3.4: Limitovaný počet místností
```
./server 0.0.0.0 10000 2 10
```

**OVĚŘ:**
- Server naslouchá na zadané IP:PORT
  ```
  # V terminálu serveru uvidíš:
  # "Listening on 0.0.0.0:12345"
  # nebo zkontroluj:
  ss -tlnp | grep 12345
  # nebo:
  netstat -tlnp | grep 12345
  ```
- Limity se aplikují
  ```
  # V výstupu serveru uvidíš:
  # "Max rooms: 5, Max clients: 20"
  ```
- Log soubor se vytváří
  ```
  ls -la server.log
  cat server.log | head -10
  ```

---

## 4. KOMPLETNÍ HRA BEZ CHYB

**POŽADAVEK:** Odehrání jedné celé hry bez výpadků a nevalidních dat

### Příprava

**Terminál 1 (WSL) - Server:**
```
cd /mnt/d/ZCU/UPS/tmwmf_sem_UPS/server_src
./server 0.0.0.0 10000 10 50
```

Zjisti IP WSL:
```
hostname -I
```

**Terminál 2 (Windows CMD) - Klient 1:**
```
cd /d D:\ZCU\UPS\tmwmf_sem_UPS\client_src
mvn exec:java
```

**Terminál 3 (Windows CMD) - Klient 2:**
```
cd /d D:\ZCU\UPS\tmwmf_sem_UPS\client_src
mvn exec:java
```

### Postup testu

1. **Klient 1:** Přihlas jako "Alice", připoj na WSL_IP:10000
2. **Klient 1:** Create Room "TestGame", 2 hráči, 4x4 deska
3. **Klient 2:** Přihlas jako "Bob", připoj na WSL_IP:10000
4. **Klient 2:** Join Room "TestGame"
5. **Klient 1:** Start Game
6. **Oba:** Hrajte hru - otáčejte karty, hledejte páry
7. **OVĚŘ:**
   - Kdo je na tahu
     ```
     # V GUI klienta: Label "Your turn" nebo "Waiting for opponent"
     # Aktivní hráč má zvýrazněné jméno
     ```
   - Skóre se aktualizuje
     ```
     # V GUI: Panel s hráči ukazuje aktuální skóre
     # Po každém MATCH se číslo zvýší
     ```
   - Matchnuté karty zůstávají otočené
     ```
     # V GUI: Matchnuté karty mají zelenou barvu a zůstávají viditelné
     ```
   - Nematchnuté se vrací
     ```
     # V GUI: Po 2 sekundách se karty otočí zpět na "?"
     ```
8. Dohrajte až do konce (všech 8 párů)
9. **OVĚŘ:** GAME_END dialog, oba se vrátí do lobby
   ```
   # V GUI: Dialog s výsledky (Winner: Alice, Score: 5)
   # Poté oba klienti uvidí lobby obrazovku
   ```

**KRITICKÉ:**
- Žádné pady serveru
  ```
  # Terminál serveru stále běží bez chyb
  ps aux | grep "./server"
  ```
- Žádné chyby v konzoli
  ```
  # V Maven konzoli klienta: Žádné Exception stacktrace
  # V server.log: Žádné ERROR zprávy (kromě očekávaných testů)
  tail -50 server.log | grep -i error
  ```
- UI responzivní
  ```
  # Během hry zkus klikat, scrollovat - okno nesmí zamrznout
  ```
- Správná herní logika
  ```
  # Zkontroluj v server.log:
  grep -i "match\|mismatch\|flip" server.log | tail -20
  # Měl bys vidět správné sekvence FLIP a MATCH/MISMATCH
  ```

---

## 5. NEVALIDNÍ DATA

**POŽADAVEK:** Ošetření náhodných dat nevyhovujících protokolu

### Test 5.1: Náhodná data jako klient

Ve WSL (server běží):
```
cat /dev/urandom | head -c 1000 | nc localhost 10000
```

**OVĚŘ:**
- Server NEPADNE (žádný segfault)
  ```
  # V terminálu serveru - měl by stále běžet, žádná chyba typu "Segmentation fault"
  # Pokud spadl, uvidíš "Segmentation fault (core dumped)"
  ```
- Server pošle ERROR zprávy
  ```
  # V výstupu netcat uvidíš něco jako:
  # ERROR INVALID_COMMAND ...
  ```
- Server odpojí klienta po max 3 chybách
  ```
  # Zkontroluj log:
  tail -20 server.log | grep -i "error\|disconnect"
  # Měl bys vidět: "Error #1", "Error #2", "Error #3", "Disconnecting"
  ```
- Server běží dál
  ```
  # Zkontroluj proces:
  ps aux | grep "./server" | grep -v grep
  # Měl bys vidět běžící proces
  ```

### Test 5.2: Opakovaný útok

```
for i in 1 2 3 4 5; do cat /dev/urandom | head -c 100 | nc localhost 10000; sleep 1; done
```

**OVĚŘ:**
- Server stále běží
  ```
  ps aux | grep "./server" | grep -v grep
  ```
- Přijímá nová legitimní spojení
  ```
  # Zkus normální připojení:
  echo -e "HELLO TestAfterAttack\nLIST_ROOMS" | nc localhost 10000
  # Měl bys dostat: WELCOME X a ROOM_LIST
  ```

### Test 5.3: Prázdné zprávy

```
echo "" | nc localhost 10000
```

### Test 5.4: Velmi dlouhá zpráva

```
python3 -c "print('A' * 10000)" | nc localhost 10000
```

---

## 6. NEPLATNÉ PŘÍKAZY A STAVY

**POŽADAVEK:** Zprávy ve špatném stavu hry, neplatná data

### Test 6.1: Příkaz před autentizací

```
nc localhost 10000
```

Zadej:
```
LIST_ROOMS
```

**Očekáváno:** ERROR (nejsi autentizován)

### Test 6.2: Neexistující příkaz

```
nc localhost 10000
```

Zadej:
```
HELLO TestUser
NEEXISTUJICI_PRIKAZ
```

**Očekáváno:** ERROR INVALID_COMMAND

### Test 6.3: FLIP mimo hru

```
nc localhost 10000
```

Zadej:
```
HELLO TestUser
FLIP 0
```

**Očekáváno:** ERROR NOT_IN_GAME nebo podobné

### Test 6.4: START_GAME s 1 hráčem

```
nc localhost 10000
```

Zadej:
```
HELLO Owner
CREATE_ROOM TestRoom 2 4
START_GAME
```

**Očekáváno:** ERROR NEED_MORE_PLAYERS

### Test 6.5: FLIP s neplatným indexem

```
nc localhost 10000
```

Zadej (po vytvoření hry):
```
FLIP -1
FLIP 9999
FLIP abc
```

**Očekáváno:** ERROR pro každý

### Test 6.6: Počítadlo chyb (odpojení po 3)

```
nc localhost 10000
```

Zadej:
```
HELLO Hacker
BAD1
BAD2
BAD3
LIST_ROOMS
```

**Očekáváno:**
- 3x ERROR
- Spojení zavřeno PŘED LIST_ROOMS

---

## 7. SIMULACE VÝPADKU SÍTĚ

**POŽADAVEK:** Zotavení po krátkodobé/dlouhodobé síťové nedostupnosti

### Příprava

Spusť server a 2 klienty, začni hru.

### Test 7.1: Krátkodobý výpadek (DROP)

Ve WSL (jako root):
```
sudo iptables -A INPUT -p tcp --dport 10000 -j DROP
```

Počkej 10-20 sekund (méně než 60s).

Obnov:
```
sudo iptables -D INPUT -p tcp --dport 10000 -j DROP
```

**OVĚŘ:**
- Ostatní hráči vidí "Player disconnected SHORT"
  ```
  # V GUI druhého klienta (Bob):
  # Zpráva v log/status panelu: "Alice disconnected (short)"
  # Nebo v server.log:
  grep -i "disconnect.*short" server.log | tail -5
  ```
- Odpojený klient automaticky reconnectne
  ```
  # V Maven konzoli klienta Alice:
  # "Auto-reconnect triggered"
  # "Reconnect attempt 1/5"
  # "Reconnect successful!"
  ```
- Stav hry se obnoví
  ```
  # V GUI Alice: Vidí herní pole se všemi kartami v původním stavu
  # V server.log:
  grep -i "reconnect\|game_state" server.log | tail -10
  ```
- Hra pokračuje
  ```
  # Oba klienti mohou pokračovat v hraní
  # Tahy fungují normálně
  ```

### Test 7.2: Dlouhodobý výpadek

```
sudo iptables -A INPUT -p tcp --dport 10000 -j DROP
```

Počkej více než 60 sekund.

**OVĚŘ:**
- Hráč je odebrán ze hry
  ```
  # V server.log:
  grep -i "timeout\|removing.*player" server.log | tail -10
  # Měl bys vidět: "Client X timed out" nebo podobné
  ```
- Ostatní dostávají "Player disconnected LONG"
  ```
  # V GUI druhého klienta:
  # Zpráva: "Alice disconnected (long)" nebo dialog s informací
  # V server.log:
  grep -i "disconnect.*long" server.log | tail -5
  ```
- Hra končí nebo pokračuje bez hráče
  ```
  # V GUI: Dialog "Game ended" nebo návrat do lobby
  # V server.log:
  grep -i "game.*end\|return.*lobby" server.log | tail -10
  ```

Obnov pravidlo:
```
sudo iptables -D INPUT -p tcp --dport 10000 -j DROP
```

### Test 7.3: REJECT (rychlé odmítnutí)

```
sudo iptables -A INPUT -p tcp --dport 10000 -j REJECT
```

**OVĚŘ:**
- Klient okamžitě ví o chybě
  ```
  # V Maven konzoli:
  # "Connection refused" nebo "Connection reset"
  # V GUI: Error dialog se zobrazí ihned (ne po timeoutu)
  ```
- Nezasekne se čekáním
  ```
  # GUI reaguje okamžitě, ne po 30+ sekundách
  # Můžeš klikat na tlačítka
  ```

Obnov:
```
sudo iptables -D INPUT -p tcp --dport 10000 -j REJECT
```

### Kontrola pravidel

Zobraz aktivní pravidla:
```
sudo iptables -L -n
```

Smaž všechna pravidla (opatrně):
```
sudo iptables -F
```

---

## 8. InTCPtor

**ÚČEL:** Testování TCP fragmentace a zpoždění

### 8.1 Instalace InTCPtor

Ve WSL:
```
cd /tmp
git clone https://github.com/MartinUbl/InTCPtor.git
cd InTCPtor
mkdir build
cd build
cmake ..
cmake --build .
```

**Očekáváno:** Soubor `libintcptor-overrides.so` v build složce

Zkontroluj:
```
ls -la libintcptor-overrides.so
```

### 8.2 Konfigurace InTCPtor

Vytvoř konfigurační soubor:
```
cd /tmp/InTCPtor
nano intcptor_config.cfg
```

Obsah:
```
# Fragmentation probabilities (0.0 - 1.0)
send_prob_1B = 0.3
send_prob_2B = 0.2
send_prob_half = 0.25

# Delays (normal distribution, milliseconds)
delay_mean_ms = 100
delay_sigma_ms = 50

# Connection dropping
drop_enabled = false

# Logging
logging_enabled = true
```

Ulož: Ctrl+O, Enter, Ctrl+X

### 8.3 Spuštění serveru s InTCPtor

Ve WSL:
```
cd /mnt/d/ZCU/UPS/tmwmf_sem_UPS/server_src
LD_PRELOAD=/tmp/InTCPtor/build/libintcptor-overrides.so ./server 0.0.0.0 10000 10 50
```

**Očekáváno:** Server startuje normálně, InTCPtor loguje intercepty

### 8.4 Test s fragmentací

**Test A: Netcat**

V novém WSL terminálu:
```
nc localhost 10000
```

Zadej:
```
HELLO FragTest
LIST_ROOMS
CREATE_ROOM FragRoom 2 4
QUIT
```

**OVĚŘ:**
- Všechny příkazy fungují
  ```
  # Dostaneš správné odpovědi:
  # WELCOME X
  # ROOM_LIST 0
  # ROOM_CREATED Y FragRoom
  ```
- Odpovědi jsou kompletní
  ```
  # Žádné oříznuté zprávy, žádné divné znaky
  # Celá zpráva na jednom řádku
  ```
- Žádné chyby parsování
  ```
  # V server.log:
  grep -i "parse.*error\|incomplete" server.log
  # Výsledek by měl být prázdný
  ```

**Test B: Java klient**

Na Windows spusť klienta a hraj normální hru.

**OVĚŘ:**
- Hra funguje i se zpožděním
  ```
  # Tahy se provedou, i když trvají déle
  # Zpoždění 100-200ms je viditelné, ale hra funguje
  ```
- Zprávy se správně skládají
  ```
  # V Maven konzoli:
  # "Received: CARD_REVEAL 5 3 Alice" - kompletní zprávy
  # Žádné: "Received: CARD_REV" (oříznuté)
  ```
- Žádné ztracené zprávy
  ```
  # Všechny akce se projeví v GUI
  # Skóre odpovídá počtu matchů
  # V server.log vidíš všechny FLIP a MATCH/MISMATCH
  ```
- UI reaguje (možná pomaleji)
  ```
  # Klikání funguje, okno se nepřestane pohybovat
  # Tlačítka reagují na hover/click
  ```

### 8.5 Agresivní fragmentace

Uprav config:
```
send_prob_1B = 0.5
send_prob_2B = 0.3
send_prob_half = 0.4
delay_mean_ms = 200
```

Restartuj server s InTCPtor a testuj znovu.

**KRITICKÉ:**
- Server musí správně bufferovat příchozí data
- Hledá `\n` jako delimiter
- Skládá fragmenty do kompletních zpráv

---

## 9. MEMORY LEAKS (Valgrind)

**POŽADAVEK:** Vhodné (ne povinné) zkusit, zda neuniká paměť

### 9.1 Kompilace s debug flagy

```
cd /mnt/d/ZCU/UPS/tmwmf_sem_UPS/server_src
make clean
make
```

Makefile už obsahuje `-g` flag.

### 9.2 Spuštění pod Valgrind

```
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./server 127.0.0.1 10000 10 50
```

Server běží pomaleji, to je normální.

### 9.3 Testovací scénář

V jiném terminálu proveď sérii akcí:

```
echo -e "HELLO User1\nLIST_ROOMS\nQUIT" | nc localhost 10000
```

```
echo -e "HELLO User2\nCREATE_ROOM Room1 2 4\nQUIT" | nc localhost 10000
```

```
echo "NEPLATNY_PRIKAZ" | nc localhost 10000
```

```
cat /dev/urandom | head -c 100 | nc localhost 10000
```

### 9.4 Ukončení a analýza

V terminálu se serverem: Ctrl+C

**Valgrind výstup:**

**DOBRÉ:**
```
definitely lost: 0 bytes in 0 blocks
indirectly lost: 0 bytes in 0 blocks
ERROR SUMMARY: 0 errors
```

**PŘIJATELNÉ:**
```
still reachable: X bytes in Y blocks
```
(pthread internals, globální struktury)

**ŠPATNÉ:**
```
definitely lost: 1234 bytes
```
= Memory leak!

---

## 10. PARALELNÍ MÍSTNOSTI

**POŽADAVEK:** Server je schopen paralelně obsluhovat několik herních místností

### Test: 3 hry současně

Spusť server:
```
./server 0.0.0.0 10000 10 50
```

Spusť 6 klientů (2 pro každou hru):
- Klienti 1+2 → Room "Game1"
- Klienti 3+4 → Room "Game2"
- Klienti 5+6 → Room "Game3"

Hrajte všechny 3 hry SOUČASNĚ.

**OVĚŘ:**
- Žádné míchání dat mezi hrami
  ```
  # V každé hře mají hráči své vlastní karty
  # MATCH v Game1 neovlivní skóre v Game2
  # Zkontroluj v server.log:
  grep "Room" server.log | tail -20
  # Měl bys vidět akce z různých místností (Room 1, Room 2, Room 3)
  ```
- Každá hra má vlastní stav
  ```
  # V GUI: Každý klient vidí pouze svou hru
  # Skóre odpovídá pouze tahům v dané místnosti
  ```
- Žádný deadlock
  ```
  # Server odpovídá na všechny požadavky
  # Žádná hra se nezasekne "čekáním"
  # V server.log nejsou dlouhé mezery mezi akcemi
  ```
- Server nepadne
  ```
  ps aux | grep "./server" | grep -v grep
  # Server stále běží
  # Žádný "Segmentation fault" nebo "Deadlock detected"
  ```

---

## 11. LOGGING

**POŽADAVEK:** Obě aplikace mají formu záznamu

### Server log

Sleduj v reálném čase:
```
tail -f /mnt/d/ZCU/UPS/tmwmf_sem_UPS/server_src/server.log
```

**Musí obsahovat:**
- Časové razítko
  ```
  # Každý řádek začíná: [2025-11-13 21:30:00]
  head -5 server.log
  ```
- Připojení/odpojení klientů
  ```
  grep -i "connect\|disconnect" server.log | tail -10
  # Měl bys vidět: "Client X connected", "Client X disconnected"
  ```
- Autentizace (HELLO/WELCOME)
  ```
  grep -i "authenticated\|hello\|welcome" server.log | tail -10
  # Měl bys vidět: "Client X authenticated as Alice"
  ```
- Vytvoření/připojení místností
  ```
  grep -i "room\|created\|joined" server.log | tail -10
  # Měl bys vidět: "Created room TestRoom", "Client X joined room Y"
  ```
- Herní akce (FLIP, MATCH, MISMATCH)
  ```
  grep -i "flip\|match\|mismatch" server.log | tail -20
  # Měl bys vidět: "FLIP 5", "MATCH", "MISMATCH"
  ```
- Chybové stavy
  ```
  grep -i "error\|warning" server.log | tail -10
  # Měl bys vidět: "ERROR INVALID_COMMAND" apod.
  ```
- PING/PONG aktivity
  ```
  grep -i "ping\|pong" server.log | tail -10
  # Měl bys vidět: "PING sent", "PONG received"
  ```

### Klient log

Výstup v Maven konzoli (stdout).

**OVĚŘ:**
```
# V Maven konzoli uvidíš:
# "Connected to server"
# "Received: WELCOME 1"
# "Sent: LIST_ROOMS"
# "Received: ROOM_LIST ..."
# atd.

# Hledej případné chyby:
# "Exception", "Error", "Failed"
```

---

## 12. GUI POŽADAVKY

**POŽADAVEK:** JavaFX GUI, neblokující, zobrazuje stav hry

### Test 12.1: Responzivita UI

Během připojování/hraní:
- Klikni na různá tlačítka
- Zkus scrollovat
- Zkus změnit velikost okna

**OVĚŘ:** UI se nezasekne
```
# Jak ověřit:
# 1. Během připojování zkus kliknout na "Cancel" - mělo by reagovat
# 2. Během hry zkus změnit velikost okna - překreslí se správně
# 3. Zkus scrollovat v room listu - plynulé scrollování
# 4. Klikni na kartu - okamžitá vizuální odezva (change color/border)
# 5. V Task Manageru: CPU usage klienta by mělo být nízké (< 10%)
```

### Test 12.2: Zobrazení stavu hry

Během hry:

**OVĚŘ:**
- Vidíš hrací pole (karty)
  ```
  # Grid 4x4 (nebo 6x6, 8x8) s tlačítky/kartami
  # Každá karta zobrazuje "?" nebo hodnotu
  ```
- Vidíš přezdívky hráčů
  ```
  # Panel na pravé straně s: "Alice", "Bob"
  ```
- Vidíš skóre
  ```
  # U každého hráče číslo: "Alice: 3"
  ```
- Vidíš kdo je na tahu
  ```
  # Label: "Your turn" nebo "Waiting for Alice"
  # Nebo zvýraznění aktivního hráče (bold, jiná barva)
  ```
- Vidíš status připojení
  ```
  # Status bar: "Connected" nebo "Reconnecting..."
  # Nebo zelená/červená ikona
  ```

### Test 12.3: Informování o nedostupnosti

**A) Při startu:** Zadej špatnou IP/port (např. 1.2.3.4:99999)
```
# Očekáváno v GUI:
# Error dialog: "Connection failed" nebo "Cannot connect to server"
# Klient se nezasekne, můžeš zadat jinou adresu
```

**B) V lobby:** Zastav server (Ctrl+C v terminálu serveru)
```
# Očekáváno v GUI:
# Dialog nebo status: "Connection lost" nebo "Server disconnected"
# Klient zkusí reconnect nebo se vrátí na login screen
```

**C) Ve hře:** Druhý hráč ukončí klienta (Alt+F4)
```
# Očekáváno v GUI prvního hráče:
# Zpráva: "Bob disconnected" nebo podobné
# Hra se pozastaví nebo skončí
```

### Test 12.4: Validace vstupů

Test v GUI klienta:

- Prázdná přezdívka → Error
  ```
  # Nech pole prázdné, klikni Connect
  # Očekáváno: "Nickname is required" nebo tlačítko disabled
  ```
- Přezdívka s mezerou → Error
  ```
  # Zadej "Test User", klikni Connect
  # Očekáváno: "Invalid nickname" nebo validační chyba
  ```
- Neplatný port → Error
  ```
  # Zadej port "abc" nebo "99999"
  # Očekáváno: "Invalid port" nebo pole se zvýrazní červeně
  ```
- Prázdný název místnosti → Error
  ```
  # Při vytváření místnosti nech název prázdný
  # Očekáváno: "Room name required" nebo tlačítko disabled
  ```

---

## 13. FINÁLNÍ CHECKLIST PODLE PozadavkyUPS.pdf

### KRITICKÉ (červené) - vrácení práce

- [ ] Server: C/C++ (BSD sockety)
- [ ] Klient: Java (standardní knihovna)
- [ ] Překlad: make (server), maven (klient)
- [ ] Bez externích síťových knihoven
- [ ] Modulární kód (moduly v C, třídy v Java)
- [ ] Stabilita: bez segfaultů, ošetřené výjimky
- [ ] Hra pro 2+ hráčů (testovatelná ve 2)
- [ ] Lobby s místnostmi
- [ ] Zotavení po výpadku (krátkodobém/dlouhodobém)
- [ ] Hráči se vrací do lobby po hře
- [ ] Aplikace běží bez restartu
- [ ] Ošetření nevalidních zpráv (odpojení při chybě)
- [ ] Logging (server i klient)
- [ ] Server: paralelní obsluha místností
- [ ] Server: konfigurovatelný limit místností
- [ ] Server: konfigurovatelný limit hráčů
- [ ] Server: konfigurovatelná IP a port
- [ ] Klient: JavaFX GUI (ne konzole)
- [ ] Klient: zadání IP/portu
- [ ] Klient: neblokující UI
- [ ] Klient: identifikace přezdívkou
- [ ] Klient: ošetření vstupů
- [ ] Klient: zobrazení aktuálního stavu hry
- [ ] Klient: informování o nedostupnosti serveru
- [ ] Klient: informování o nedostupnosti protihráče

### DŮLEŽITÉ (oranžové) - může vést k vrácení

- [ ] Počítadlo nevalidních zpráv (odpojení po např. 3)

### DOPORUČENÉ

- [ ] Valgrind test (bez memory leaků)
- [ ] InTCPtor test (fragmentace TCP)
- [ ] Stress test (20+ souběžných připojení)
- [ ] Test s /dev/urandom (náhodná data)

---

## PRŮBĚH ODEVZDÁNÍ (podle PDF)

Co bude při odevzdání:

1. **Překlad klienta a serveru** - ukázat make a mvn
2. **Spuštění s různými parametry** - změnit IP, port, limity
3. **Odehrání jedné celé hry bez výpadků** - kompletní hra
4. **Schopnost reagovat na výpadky** - iptables DROP/REJECT
5. **Schopnost vyrovnat se s nevalidními daty** - /dev/urandom test
6. **Ověření náročnosti na systémové prostředky** - Valgrind, htop

---

## PROSTŘEDÍ PRO ODEVZDÁNÍ

- Server: GNU/Linux (WSL nebo nativní)
- Klient 1: GNU/Linux
- Klient 2: MS Windows
- Laboratoř: UC-326

Před odevzdáním:
1. Ověř kompilaci na obou prostředích
2. Ověř spuštění a propojení
3. Připrav všechny testovací scénáře

---

## ZÁVĚR

Po úspěšném dokončení všech testů splňuješ:

- Všechny kritické požadavky z PozadavkyUPS.pdf
- Server odolný vůči nevalidním datům
- Klient s plnohodnotným GUI
- Robustní síťová komunikace
- Zotavení po výpadcích
- Kompletní logging
- Paralelní obsluha více her

**Minimální bodové hodnocení: 15-30 bodů**
