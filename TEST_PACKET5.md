# Testovací Scénář pro Packet 5

## Příprava

### 1. Kompilace serveru
```bash
cd server_src
make clean
make
```

Očekávaný výstup:
```
gcc -Wall -Wextra -pthread -g -std=c99 -c main.c -o main.o
gcc -Wall -Wextra -pthread -g -std=c99 -c server.c -o server.o
gcc -Wall -Wextra -pthread -g -std=c99 -c client_handler.c -o client_handler.o
gcc -Wall -Wextra -pthread -g -std=c99 -c logger.c -o logger.o
gcc -Wall -Wextra -pthread -g -std=c99 -c room.c -o room.o
gcc main.o server.o client_handler.o logger.o room.o -o server -pthread
Build successful: server
```

### 2. Spuštění serveru
```bash
./server 127.0.0.1 10000 10 50
```

Parametry:
- IP: 127.0.0.1
- Port: 10000
- Max místností: 10
- Max klientů: 50

Server by měl vypsat:
```
[INFO] Logger initialized
[INFO] Server initialized: 127.0.0.1:10000 (max_rooms=10, max_clients=50)
[INFO] Server started, waiting for connections...
```

## Základní Testy

### Test 1: Připojení a autentizace

**Otevři nový terminál:**
```bash
nc 127.0.0.1 10000
```

**Pošli:**
```
HELLO Alice
```

**Očekávaný výstup:**
```
WELCOME 1
```

**Server log měl vypsat:**
```
[INFO] New connection from 127.0.0.1:xxxxx (fd=x)
[INFO] Client 1: Thread created successfully
[INFO] Client 1: Handler thread started (fd=x)
[INFO] Client 1: Received message: 'HELLO Alice'
[INFO] Client 1 authenticated as 'Alice'
```

### Test 2: Seznam místností (prázdný)

**V terminálu Alice pošli:**
```
LIST_ROOMS
```

**Očekávaný výstup:**
```
ROOM_LIST 0
```

### Test 3: Vytvoření místnosti

**Alice vytvoří místnost:**
```
CREATE_ROOM MyRoom 4
```

**Očekávaný výstup:**
```
ROOM_CREATED 1 MyRoom
```

**Server log:**
```
[INFO] Client 1: Received message: 'CREATE_ROOM MyRoom 4'
[INFO] Client 1 (Alice) created room 1
```

### Test 4: Seznam místností (s jednou místností)

**Alice pošle:**
```
LIST_ROOMS
```

**Očekávaný výstup:**
```
ROOM_LIST 1 1 MyRoom 1 4 WAITING
```

Formát: `ROOM_LIST <count> <id> <name> <current_players> <max_players> <state>`

## Testy s Více Klienty

### Test 5: Druhý klient se připojí

**Otevři nový terminál (Bob):**
```bash
nc 127.0.0.1 10000
```

**Bob se autentizuje:**
```
HELLO Bob
```

**Očekávaný výstup:**
```
WELCOME 2
```

### Test 6: Bob vidí Alici místnost

**Bob pošle:**
```
LIST_ROOMS
```

**Očekávaný výstup:**
```
ROOM_LIST 1 1 MyRoom 1 4 WAITING
```

### Test 7: Bob se připojí k místnosti

**Bob pošle:**
```
JOIN_ROOM 1
```

**Očekávaný výstup Bobu:**
```
ROOM_JOINED 1 MyRoom
```

**Alice by měla dostat broadcast:**
```
PLAYER_JOINED Bob
```

**Server log:**
```
[INFO] Client 2: Received message: 'JOIN_ROOM 1'
[INFO] Client 2 (Bob) joined room 1
```

### Test 8: Seznam místností nyní ukazuje 2 hráče

**V novém terminálu připoj třetího klienta (Charlie):**
```bash
nc 127.0.0.1 10000
HELLO Charlie
LIST_ROOMS
```

**Charlie uvidí:**
```
WELCOME 3
ROOM_LIST 1 1 MyRoom 2 4 WAITING
```

### Test 9: Bob opustí místnost

**Bob pošle:**
```
LEAVE_ROOM
```

**Očekávaný výstup Bobu:**
```
LEFT_ROOM
```

**Alice dostane:**
```
PLAYER_LEFT Bob
```

## Testy Error Handling

### Test 10: Připojení do plné místnosti

**Vytvoř místnost s max 2 hráči:**
```
# Jako Alice:
CREATE_ROOM SmallRoom 2

# Jako Bob:
JOIN_ROOM 2

# Jako Charlie:
JOIN_ROOM 2
```

**Charlie dostane:**
```
ERROR ROOM_FULL Room is full
```

### Test 11: Neexistující místnost

**Jako Charlie:**
```
JOIN_ROOM 999
```

**Očekávaný výstup:**
```
ERROR ROOM_NOT_FOUND Room not found
```

### Test 12: Vytvoření místnosti když už v místnosti jsi

**Jako Alice (která je v MyRoom):**
```
CREATE_ROOM AnotherRoom 4
```

**Očekávaný výstup:**
```
ERROR ALREADY_IN_ROOM Already in a room
```

### Test 13: Opuštění místnosti když nejsi v žádné

**Jako Charlie (který není v místnosti):**
```
LEAVE_ROOM
```

**Očekávaný výstup:**
```
ERROR NOT_IN_ROOM Not in a room
```

### Test 14: Příkazy bez autentizace

**Otevři nový terminál:**
```bash
nc 127.0.0.1 10000
```

**Bez HELLO pošli:**
```
LIST_ROOMS
```

**Očekávaný výstup:**
```
ERROR NOT_AUTHENTICATED Not authenticated
```

### Test 15: Dvojí autentizace

**Jako Alice:**
```
HELLO Alice
HELLO AliceAgain
```

**Druhé HELLO vrátí:**
```
ERROR ALREADY_AUTHENTICATED Already authenticated
```

### Test 16: Neplatné parametry

**Jako Charlie:**
```
CREATE_ROOM TestRoom 1
```

**Očekávaný výstup:**
```
ERROR INVALID_PARAMS Max players must be 2-4
```

**Zkus také:**
```
CREATE_ROOM TestRoom 10
```

**Očekávaný výstup:**
```
ERROR INVALID_PARAMS Max players must be 2-4
```

## Test Odpojení

### Test 17: Odpojení klienta v místnosti

**Jako Bob (v místnosti):**
- Stiskni Ctrl+C nebo zavři terminál

**Alice by měla dostat:**
```
PLAYER_DISCONNECTED Bob LONG
```

**Server log:**
```
[INFO] Client 2: Connection closed by client
[INFO] Client 2: Removing from room 1
[INFO] Client 2: Closing connection
```

### Test 18: Automatické smazání prázdné místnosti

**Jako Alice (poslední v místnosti):**
```
LEAVE_ROOM
```

**Jako Charlie:**
```
LIST_ROOMS
```

**Charlie uvidí:**
```
ROOM_LIST 0
```

Místnost byla automaticky smazána, protože byla prázdná.

## Test s InTCPtor

### Test 19: Fragmentace zpráv

**Spusť InTCPtor:**
```bash
# Nastavení:
# Server: 127.0.0.1:10000
# Fragmentace: 2 bajty
# Zpoždění: 100ms
```

**Připoj se přes InTCPtor a pošli:**
```
HELLO FragmentedUser
LIST_ROOMS
CREATE_ROOM FragTest 3
```

Server by měl správně zpracovat všechny zprávy i přes fragmentaci.

## Test Více Místností

### Test 20: Vytvoř více místností současně

**Připoj 4 klienty:**

**Client 1:**
```
HELLO Alice
CREATE_ROOM Room1 2
```

**Client 2:**
```
HELLO Bob
CREATE_ROOM Room2 3
```

**Client 3:**
```
HELLO Charlie
CREATE_ROOM Room3 4
```

**Client 4:**
```
HELLO Dave
LIST_ROOMS
```

**Dave uvidí:**
```
ROOM_LIST 3 1 Room1 1 2 WAITING 2 Room2 1 3 WAITING 3 Room3 1 4 WAITING
```

### Test 21: Paralelní připojování

**Client 4 (Dave) se připojí k Room2:**
```
JOIN_ROOM 2
```

**Client 5 (nový):**
```bash
nc 127.0.0.1 10000
HELLO Eve
JOIN_ROOM 2
```

**Bob dostane:**
```
PLAYER_JOINED Dave
PLAYER_JOINED Eve
```

## Kontrola Logů

### Test 22: Zkontroluj server.log

```bash
cat server.log
```

Měl bys vidět:
- Všechny připojení (New connection)
- Všechny autentizace (authenticated as)
- Všechny vytvoření místností (created room)
- Všechny připojení k místnostem (joined room)
- Všechny odpojení (Connection closed)

## Test Paměťových Úniků

### Test 23: Valgrind

**Zavři server (Ctrl+C) a spusť s Valgrind:**
```bash
valgrind --leak-check=full --show-leak-kinds=all ./server 127.0.0.1 10000 10 50
```

**Proveď několik operací:**
1. Připoj se, autentizuj, vytvoř místnost, odpoj se
2. Opakuj 5x

**Ukonči server (Ctrl+C)**

**Zkontroluj výstup Valgrind:**
```
HEAP SUMMARY:
    in use at exit: 0 bytes in 0 blocks
  total heap usage: X allocs, X frees, Y bytes allocated

All heap blocks were freed -- no leaks are possible
```

## Rychlý Test Skript

Můžeš vytvořit jednoduchý test skript:

### test_basic.sh
```bash
#!/bin/bash

echo "=== Test 1: Alice creates room ==="
(
    echo "HELLO Alice"
    sleep 0.5
    echo "CREATE_ROOM TestRoom 4"
    sleep 0.5
    echo "LIST_ROOMS"
    sleep 0.5
) | nc 127.0.0.1 10000 &

sleep 2

echo "=== Test 2: Bob joins room ==="
(
    echo "HELLO Bob"
    sleep 0.5
    echo "LIST_ROOMS"
    sleep 0.5
    echo "JOIN_ROOM 1"
    sleep 2
) | nc 127.0.0.1 10000 &

wait
echo "=== Tests complete, check server.log ==="
```

## Checklist

Po dokončení všech testů by měly být splněny následující podmínky:

- [ ] Server se spustí bez chyb
- [ ] Klienti se mohou připojit a autentizovat
- [ ] LIST_ROOMS vrací správný formát
- [ ] CREATE_ROOM vytváří místnosti správně
- [ ] JOIN_ROOM funguje a broadcastuje ostatním
- [ ] LEAVE_ROOM funguje a broadcastuje
- [ ] Prázdné místnosti se automaticky mažou
- [ ] Error handling funguje pro všechny chybové stavy
- [ ] Odpojení klienta v místnosti je správně ošetřeno
- [ ] Více místností funguje paralelně
- [ ] Žádné memory leaky (valgrind)
- [ ] Žádné segfaults
- [ ] Server běží stabilně
- [ ] Logy jsou správné a kompletní

## Známé Problémy

Pokud narazíš na problémy:

1. **Server se nespustí:** Zkontroluj, jestli port 10000 není už obsazený: `netstat -tuln | grep 10000`
2. **Nemůžeš se připojit:** Zkontroluj firewall
3. **Server crashuje:** Spusť s valgrind nebo gdb pro debug
4. **Zprávy nejsou správně zpracovány:** Zkontroluj, že končí `\n`
