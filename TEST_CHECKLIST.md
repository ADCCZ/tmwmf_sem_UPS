# Test Checklist - KIV/UPS Semester Project

Tento dokument obsahuje kompletní seznam testů pro ověření všech požadavků ze zadání.

## Test 1: Základní funkčnost hry (2 hráči bez chyb)

### Cíl
Ověřit, že hra funguje od začátku do konce bez chyb.

### Kroky
1. Spusť server: `./server 0.0.0.0 10000 10 50`
2. Připoj 2 klienty
3. Vytvoř místnost (hráč 1)
4. Připoj se do místnosti (hráč 2)
5. Začni hru
6. Zahraj celou hru až do konce
7. Zkontroluj automatický návrat do lobby

### Očekávaný výsledek
- ✅ Žádné chyby v logu
- ✅ Žádné segfaults
- ✅ Hra končí korektně
- ✅ Oba klienti se vrátí do lobby

---

## Test 2: Síťový výpadek - iptables DROP

### Cíl
Ověřit reconnect logiku při síťovém výpadku.

### Kroky
1. Spusť server
2. Připoj 2 klienty a začni hru
3. Aplikuj iptables pravidlo:
   ```bash
   sudo iptables -A INPUT -p tcp --dport 10000 -j DROP
   ```
4. Počkej 10-15 sekund (klienti detekují disconnect)
5. Odstraň pravidlo:
   ```bash
   sudo iptables -D INPUT -p tcp --dport 10000 -j DROP
   ```
6. Zkontroluj reconnect

### Očekávaný výsledek
- ✅ Klienti detekují disconnect do 10s
- ✅ Automatický reconnect (5 pokusů po 5s)
- ✅ Server přijme RECONNECT a obnoví stav hry
- ✅ Hra pokračuje normálně
- ✅ Žádné duplicitní RECONNECT akceptovány

---

## Test 3: Síťový výpadek - iptables REJECT

### Cíl
Ověřit rychlé reconnect při explicitním odmítnutí.

### Kroky
1. Stejné jako Test 2, ale použij REJECT:
   ```bash
   sudo iptables -A INPUT -p tcp --dport 10000 -j REJECT
   sudo iptables -D INPUT -p tcp --dport 10000 -j REJECT
   ```

### Očekávaný výsledek
- ✅ Rychlejší reconnect než s DROP
- ✅ Chybová hláška "Connection refused"
- ✅ Úspěšný reconnect po odstranění pravidla

---

## Test 4: Long disconnect timeout (> 90s)

### Cíl
Ověřit forfeit logiku po timeoutu.

### Kroky
1. Spusť server
2. Připoj 2 klienty a začni hru
3. Aplikuj iptables DROP
4. Počkej 95 sekund (přes RECONNECT_TIMEOUT)
5. Zkontroluj server log

### Očekávaný výsledek
- ✅ Server loguje "waiting for reconnect: X/90 seconds"
- ✅ Po 90s: "GAME_END_FORFEIT" broadcast
- ✅ Hráč s vyšším skóre dostane bonus za zbývající páry
- ✅ Místnost a hra uvolněna
- ✅ Klient zobrazí "Game Over - Forfeit"

---

## Test 5: Nevalidní zprávy - random data

### Cíl
Ověřit zpracování nevalidních dat a odpojení po 3 chybách.

### Kroky
1. Spusť server
2. Pošli random data:
   ```bash
   cat /dev/urandom | head -c 1000 | nc 127.0.0.1 10000
   ```
3. Zkontroluj server log

### Očekávaný výsledek
- ✅ Server loguje "ERROR INVALID_COMMAND"
- ✅ Po 3 chybách: "Max error count reached, closing connection"
- ✅ Spojení uzavřeno
- ✅ Žádný crash

---

## Test 6: Nevalidní zprávy - formátově správné

### Cíl
Ověřit validaci dat v platných příkazech.

### Připojení jako legitimní klient:
```bash
nc 127.0.0.1 10000
```

### Test 6a: Karta mimo rozsah
```
HELLO TestPlayer
FLIP -1
```
**Očekáváno**: `ERROR INVALID_MOVE Card index out of bounds (0-15)`

### Test 6b: Příkaz ve špatném stavu
```
FLIP 5
```
**Očekáváno**: `ERROR NOT_IN_ROOM Not in a room`

### Test 6c: Nevalidní board_size
```
CREATE_ROOM TestRoom 2 7
```
**Očekáváno**: `ERROR INVALID_PARAMS Invalid board_size: 7 (must be 4, 6, or 8)`

---

## Test 7: Více souběžných místností

### Cíl
Ověřit paralelní běh her.

### Kroky
1. Spusť server: `./server 0.0.0.0 10000 10 50`
2. Připoj 6 klientů
3. Vytvoř 3 místnosti (2 hráči každá)
4. Začni všechny 3 hry současně
5. Hraj ve všech místnostech paralelně

### Očekávaný výsledek
- ✅ Všechny 3 hry běží nezávisle
- ✅ Tahy v jedné hře neovlivňují druhé
- ✅ Žádné race conditions
- ✅ Žádné crashes

---

## Test 8: Memory leaks - valgrind

### Cíl
Ověřit absenci memory leaků.

### Kroky
1. Spusť server s valgrind:
   ```bash
   valgrind --leak-check=full --show-leak-kinds=all ./server 0.0.0.0 10000 10 50
   ```
2. Připoj několik klientů
3. Vytvoř místnosti, hraj hry
4. Proveď reconnect test
5. Ukončí server (Ctrl+C)

### Očekávaný výsledek
```
LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
     possibly lost: 0 bytes in 0 blocks
```
- ✅ Žádné "Invalid read/write"
- ✅ Žádné "Use of uninitialized value"
- ✅ Žádné "Warning: invalid file descriptor"

---

## Test 9: UI - Non-blocking

### Cíl
Ověřit že UI nezamrzá během síťových operací.

### Kroky
1. Spusť klienta
2. Zadej IP serveru který neexistuje (např. 192.168.255.255)
3. Klikni "CONNECT"
4. Během timeoutu zkus pohybovat oknem

### Očekávaný výsledek
- ✅ Okno se pohybuje normálně
- ✅ Tlačítko "CONNECT" je disabled
- ✅ Po timeoutu: chybová hláška "Failed to connect"
- ✅ UI zůstává responzivní

---

## Test 10: UI - Informace o stavu

### Cíl
Ověřit že UI zobrazuje aktuální informace.

### Kroky
1. Připoj 2 klienty
2. Začni hru
3. Zkontroluj UI:
   - Seznam hráčů se jmény a skóre
   - Označení kdo je na tahu
   - Status label ("It's your turn!" / "Player2's turn")
4. Aplikuj iptables DROP
5. Zkontroluj UI během odpojení:
   - "Disconnected - reconnecting..." status
   - Warning dialog "Player X disconnected"

### Očekávaný výsledek
- ✅ Vše zobrazeno správně
- ✅ Real-time updaty
- ✅ Viditelné notifikace o disconnectu

---

## Test 11: Konfigurovatelné limity

### Cíl
Ověřit že server respektuje konfigurace.

### Test 11a: Limit místností
```bash
./server 0.0.0.0 10000 2 50  # Max 2 místnosti
```
- Vytvoř 2 místnosti ✅
- Pokus o 3. místnost → `ERROR No free room slots available` ✅

### Test 11b: Limit klientů
```bash
./server 0.0.0.0 10000 10 3  # Max 3 klienti
```
- Připoj 3 klienty ✅
- 4. klient → spojení odmítnuto ✅

---

## Test 12: Shutdown cleanup

### Cíl
Ověřit čisté ukončení serveru.

### Kroky
1. Spusť server s valgrind
2. Připoj klienty, začni hry
3. Aplikuj iptables DROP (simuluj disconnected klienty)
4. Ukončí server (Ctrl+C)
5. Zkontroluj valgrind output

### Očekávaný výsledek
- ✅ Server ukončí všechny thready
- ✅ Uvolní všechny resources (rooms, games, clients)
- ✅ Žádné memory leaky
- ✅ Žádné "Invalid write" během shutdownu
- ✅ Clean exit

---

## Test 13: Edge cases

### Test 13a: Reconnect když už je připojený
**Scénář**: Klient pošle RECONNECT i když je už připojený
**Očekáváno**: `ERROR Client is already connected`

### Test 13b: FLIP když není na tahu
**Scénář**: Hráč pošle FLIP když je na tahu druhý hráč
**Očekáváno**: `ERROR NOT_YOUR_TURN`

### Test 13c: FLIP už otočené karty
**Scénář**: Hráč pošle FLIP na kartu která je už matched
**Očekáváno**: `ERROR INVALID_CARD Cannot flip that card`

### Test 13d: Nickname s nevalidními znaky
**Scénář**: `HELLO Player<script>`
**Očekáváno**: Klient odmítne před odesláním (validace)

---

## Shrnutí - Co testovat před odevzdáním

### Musí projít:
1. ✅ Test 1 - Základní hra bez chyb
2. ✅ Test 2 - Reconnect s iptables DROP
3. ✅ Test 5 - Random data (3 chyby → disconnect)
4. ✅ Test 6 - Validace neplatných dat
5. ✅ Test 7 - Více souběžných her
6. ✅ Test 8 - Valgrind bez leaků
7. ✅ Test 11 - Konfigurovatelné limity

### Nice to have:
- Test 4 - Long timeout (forfeit)
- Test 9 - Non-blocking UI
- Test 10 - UI informace
- Test 12 - Clean shutdown
- Test 13 - Edge cases

---

## Testovací příkazy - Quick Reference

```bash
# Spuštění serveru
./server 0.0.0.0 10000 10 50

# Valgrind
valgrind --leak-check=full ./server 0.0.0.0 10000 10 50

# iptables DROP
sudo iptables -A INPUT -p tcp --dport 10000 -j DROP
sudo iptables -D INPUT -p tcp --dport 10000 -j DROP

# iptables REJECT
sudo iptables -A INPUT -p tcp --dport 10000 -j REJECT
sudo iptables -D INPUT -p tcp --dport 10000 -j REJECT

# Random data
cat /dev/urandom | head -c 1000 | nc 127.0.0.1 10000

# Manuální příkazy
nc 127.0.0.1 10000
HELLO TestPlayer
LIST_ROOMS
CREATE_ROOM TestRoom 2 4
```
