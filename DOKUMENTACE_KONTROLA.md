# KONTROLA DOKUMENTACE - Splnění požadavků ze zadání

## ✅ VŠECHNY POŽADAVKY JSOU SPLNĚNY

Porovnání požadavků z **PozadavkyUPS.pdf strana 3** proti obsahu **dokumentace.tex**:

---

## 1. ✅ Základní zkrácený popis hry

**Požadavek:** "základní zkrácený popis hry, ve variantě, ve které jste se rozhodli ji implementovat"

**Splněno v dokumentaci:**
- **Sekce 1.1:** Popis hry (strana 2)
  - Pexeso pro 2-4 hráče
  - Pravidla (tahová hra, otáčení karet, shoda/neshoda, skóre)
  - Konec hry, určení vítěze

**Obsah:**
```
\subsection{Popis hry}
Pexeso je tahová paměťová hra pro 2--4 hráče...
\textbf{Pravidla:}
- Hráči se střídají v tazích
- V každém tahu hráč otočí postupně 2 karty
...
```

---

## 2. ✅ Popis protokolu dostatečný pro implementaci alternativního klienta/serveru

### 2a. ✅ Formát zpráv

**Požadavek:** "formát zpráv"

**Splněno v dokumentaci:**
- **Sekce 2.1:** Základní charakteristika (strana 3)
  - Tabulka s parametry (typ protokolu, transport, kódování, ukončení, formát, oddělování)
  - Formát: `COMMAND [PARAM1] [PARAM2] ...\n`

**Obsah:**
```
\begin{table}[H]
\textbf{Parametr} & \textbf{Hodnota}
Ukončení zprávy & \texttt{\textbackslash n} (newline)
Formát zprávy & \texttt{COMMAND [PARAM1] [PARAM2] ...}
Oddělování parametrů & Jedna mezera (ASCII 0x20)
```

- **Sekce 2.3:** Příklady zpráv (strana 4)
  - Konkrétní příklady klient→server a server→klient

---

### 2b. ✅ Přenášené struktury, datové typy

**Požadavek:** "přenášené struktury, datové typy"

**Splněno v dokumentaci:**
- **Sekce 2.2:** Datové typy a omezení (strana 3)
  - Tabulka s 9 datovými typy (nickname, room_id, card_id, ...)
  - Pro každý typ: popis, formát, omezení

**Obsah:**
```
\begin{table}[H]
\textbf{Typ} & \textbf{Popis} & \textbf{Formát} & \textbf{Omezení}
nickname & Přezdívka hráče & String bez mezer & 1--16 znaků
card_id & Index karty & Integer & 0--(board_size-1)
...
```

- **Sekce 3.2:** Datové struktury serveru (strana 6-7)
  - `client_t`, `room_t`, `game_t` s komentovaným C kódem

---

### 2c. ✅ Význam přenášených dat a kódů

**Požadavek:** "význam přenášených dat a kódů"

**Splněno v dokumentaci:**
- **Sekce 2.3:** Příklady zpráv (strana 4)
  - Konkrétní příklady s vysvětlením každé zprávy

- **Sekce 2.6:** Validace a chybové stavy (strana 8)
  - Tabulka error kódů s popisem významu
  - INVALID_COMMAND, ROOM_NOT_FOUND, NOT_YOUR_TURN, ...

**Obsah:**
```
\begin{table}[H]
\textbf{Error kód} & \textbf{Popis}
INVALID_COMMAND & Neznámý příkaz
NOT_YOUR_TURN & Není na tahu tento hráč
```

---

### 2d. ✅ Omezení vstupních hodnot a validaci dat

**Požadavek:** "omezení vstupních hodnot a validaci dat (omezení na hodnotu, apod.)"

**Splněno v dokumentaci:**
- **Sekce 2.2:** Datové typy a omezení (strana 3)
  - Sloupec "Omezení" pro každý typ
  - Např. nickname: 1-16 znaků, a-zA-Z0-9_-
  - board_size: pouze 16, 24, 32, 36

- **Sekce 2.6:** Validace a chybové stavy (strana 8)
  - Server validuje zprávy a odpojí po 3 chybách
  - Typy chyb: neznámý příkaz, špatné parametry, příkazy ve špatném stavu, neplatné herní tahy

**Obsah:**
```
Server validuje všechny příchozí zprávy:
- Náhodná data (nevalidní formát)
- Neplatné parametry
- Příkazy ve špatném stavu
- Nevalidní herní pravidla
Server odpojí klienta po 3 nevalidních zprávách
```

---

### 2e. ✅ Návaznost zpráv, např. formou stavového diagramu

**Požadavek:** "návaznost zpráv, např. formou stavového diagramu"

**Splněno v dokumentaci:**
- **Sekce 2.4:** Stavový diagram klienta (strana 5)
  - **OBRAZKOVÝ TikZ diagram** se stavy a přechody
  - Stavy: DISCONNECTED → CONNECTED → AUTHENTICATED → IN_LOBBY → IN_ROOM → IN_GAME
  - Přechody s příkazy: HELLO, WELCOME, JOIN_ROOM, GAME_START, ...
  - Zobrazuje i reconnect mechanismus (SHORT_DISCONNECT → RECONNECT → IN_GAME)

- **Sekce 2.7 (nově přidaná):** Návaznost zpráv (sekvenční diagram)
  - **OBRAZKOVÝ TikZ sekvenční diagram**
  - Komunikace Alice ↔ Server ↔ Bob
  - Kompletní flow: připojení, vytvoření místnosti, join, start hry, herní tah

**Obsah:**
```latex
\begin{figure}[H]
\begin{tikzpicture}
% Stavový diagram s uzly a hranami
\node[initial] (disc) {DISCONNECTED};
\node[state, right of=disc] (conn) {CONNECTED};
\draw[->] (disc) -- node[above] {\tiny connect()} (conn);
...
\end{tikzpicture}
\caption{Stavový diagram klienta}
\end{figure}
```

---

### 2f. ✅ Chybové stavy a jejich hlášení (kdy, co znamenají)

**Požadavek:** "chybové stavy a jejich hlášení (kdy, co znamenají)"

**Splněno v dokumentaci:**
- **Sekce 2.6:** Validace a chybové stavy (strana 8)
  - Tabulka všech error kódů s popisem
  - Kdy jsou posílány
  - Co znamenají
  - Politika odpojení (po 3 chybách)

**Obsah:**
```
\begin{table}[H]
\textbf{Error kód} & \textbf{Popis}
INVALID_COMMAND & Neznámý příkaz
INVALID_PARAMS & Špatný počet/formát parametrů
NOT_AUTHENTICATED & Příkaz vyžaduje autentizaci
ROOM_NOT_FOUND & Místnost neexistuje
NOT_YOUR_TURN & Není na tahu tento hráč
...
\end{table}

Server odpojí klienta po 3 nevalidních zprávách (konfigurovatelné).
```

---

## 3. ✅ Popis implementace klienta a serveru (programátorská dokumentace)

### 3a. ✅ Dekompozice do modulů/tříd

**Požadavek:** "dekompozice do modulů/tříd"

**Splněno v dokumentaci:**

**Server:**
- **Sekce 3.1:** Architektura a moduly (strana 6)
  - Tabulka 7 modulů: main.c, server.c, client_handler.c, client_list.c, room.c, game.c, logger.c, protocol.h
  - Pro každý modul: odpovědnost a popis

**Klient:**
- **Sekce 4.1:** Architektura MVC (strana 9)
  - Tabulka balíčků/tříd: Main, network/ClientConnection, protocol, model, controller, util/Logger
  - MVC architektura

**Obsah:**
```
\begin{table}[H]
\textbf{Modul} & \textbf{Odpovědnost}
main.c & Vstupní bod, parsování argumentů
server.c & Listening socket, accept loop
client_handler.c & Obsluha klientů, zpracování zpráv
...
\end{table}
```

---

### 3b. ✅ Rozvrstvení aplikace

**Požadavek:** "rozvrstvení aplikace"

**Splněno v dokumentaci:**

**Server:**
- **Sekce 3.1:** Vrstvy: main → server → client_handler → room/game → logger

**Klient:**
- **Sekce 4.1:** MVC architektura
  - Model (Room)
  - View (FXML layouts)
  - Controller (LoginController, LobbyController, GameController)
  - Network (ClientConnection)
  - Protocol (ProtocolConstants)

**Obsah:**
```
Klient je implementován v Javě s využitím JavaFX a architekturou Model-View-Controller:

\begin{tabular}
controller/ & Ovládání GUI
model/ & Model herní místnosti
network/ & TCP spojení, auto-reconnect
\end{tabular}
```

---

### 3c. ✅ Použité knihovny, verze prostředí (Java), apod.

**Požadavek:** "použité knihovny, verze prostředí (Java), apod."

**Splněno v dokumentaci:**

**Server:**
- **Sekce 3.5:** Použité knihovny a API (strana 7)
  - BSD Sockets: `<sys/socket.h>`, `<netinet/in.h>`, `<arpa/inet.h>`
  - POSIX Threads: `<pthread.h>`
  - Standardní C knihovna
  - **Důležité:** Všechny jsou POSIX standard, žádné externí knihovny

**Klient:**
- **Sekce 4.3:** Použité knihovny a API (strana 10)
  - Java SE: `java.net.Socket`, `java.io.*`
  - JavaFX: `javafx.application.*`, `javafx.scene.*`
  - **Důležité:** Pouze Java SE, žádné externí knihovny

- **Sekce 5.1:** Požadavky na prostředí (strana 11)
  - Server: GCC 7.5+, GNU Make 4.1+
  - Klient: **Java SE 17+**, Maven 3.6+, JavaFX 17+

**Obsah:**
```
\textbf{Server (C):}
- GNU/Linux
- GCC 7.5+ nebo Clang 6.0+
- GNU Make 4.1+
- POSIX threads (libpthread)

\textbf{Klient (Java):}
- Java SE 17+ (OpenJDK nebo Oracle JDK)
- Apache Maven 3.6+
- JavaFX 17+
```

---

### 3d. ✅ Metoda paralelizace (select, vlákna, procesy)

**Požadavek:** "metoda paralelizace (select, vlákna, procesy)"

**Splněno v dokumentaci:**
- **Sekce 3.3:** Paralelizace -- POSIX threads (strana 6-7)
  - **Metoda:** POSIX threads, thread-per-client model
  - Hlavní thread: accept loop
  - Pro každého klienta: nový thread
  - Detach: automatický cleanup
  - Mutex: logger, room broadcast
  - **Proč vlákna:** Jednoduchost, blokující I/O přijatelné, izolace, vhodné pro desítky klientů

**Obsah:**
```
\subsection{Paralelizace -- POSIX threads}

Server používá thread-per-client model:
- Hlavní thread běží v server_run() -- nekonečná accept loop
- Pro každého nového klienta se vytvoří vlastní thread (pthread_create)
- Thread se detachuje (pthread_detach) → automatický cleanup

\textbf{Výhody thread-per-client:}
- Jednoduchá implementace
- Blokující I/O neovlivňuje ostatní klienty
- Izolace mezi klienty
- Vhodné pro desítky klientů (max 50)
```

---

## 4. ✅ Požadavky na překlad, spuštění a běh aplikace

**Požadavek:** "požadavky na překlad, spuštění a běh aplikace (verze Javy, gcc, ...)"

**Splněno v dokumentaci:**
- **Sekce 5.1:** Požadavky na prostředí (strana 11)
  - Server: GNU/Linux, GCC 7.5+, Make 4.1+, POSIX threads
  - Klient: Java SE 17+, Maven 3.6+, JavaFX 17+

**Obsah:**
```
\subsection{Požadavky na prostředí}

\textbf{Server (C):}
- GNU/Linux
- GCC 7.5+ nebo Clang 6.0+
- GNU Make 4.1+
- POSIX threads (libpthread)

\textbf{Klient (Java):}
- Java SE 17+ (OpenJDK nebo Oracle JDK)
- Apache Maven 3.6+
- JavaFX 17+
```

---

## 5. ✅ Postup překladu

**Požadavek:** "postup překladu"

**Splněno v dokumentaci:**

- **Sekce 5.2:** Překlad serveru (strana 11)
  - Přesné příkazy: `make clean`, `make`
  - Kompletní Makefile

- **Sekce 5.3:** Překlad klienta (strana 11)
  - Přesné příkazy: `mvn clean package`
  - Výsledný JAR

- **Sekce 5.4:** Spuštění (strana 11)
  - Příkazy pro server: `./server <IP> <PORT> <MAX_ROOMS> <MAX_CLIENTS>`
  - Příkazy pro klienta: `java -jar ...` nebo `mvn javafx:run`

**Obsah:**
```
\subsection{Překlad serveru}

\textbf{Pomocí Makefile:}
cd server_src
make clean    # Vycistit stare buildy
make          # Zkompilovat server

\subsection{Překlad klienta}

\textbf{Pomocí Maven:}
cd client_src
mvn clean package

Maven vytvoří spustitelný JAR:
target/pexeso-client-1.0-SNAPSHOT.jar
```

---

## 6. ✅ Závěr, zhodnocení dosažených výsledků

**Požadavek:** "závěr, zhodnocení dosažených výsledků"

**Splněno v dokumentaci:**
- **Sekce 7:** Závěr (strana 13-14)
  - Dosažené výsledky (všechny požadavky splněny)
  - Použité technologie (C, Java, BSD sockets, POSIX threads, JavaFX)
  - Možná rozšíření (chat, statistiky, žebříček, ...)
  - Zhodnocení (robustní protokol, stabilní běh, úspěšné testování)
  - Klíčové poznatky

**Obsah:**
```
\section{Závěr}

\subsection{Dosažené výsledky}

Projekt splňuje všechny požadavky zadání:
- Kompletní síťová hra (2-4 hráči, lobby, místnosti)
- Textový protokol (ASCII přes TCP)
- Robustní implementace (validace, error handling, logování)
- Reconnect mechanismus
- Paralelní běh místností
- Stabilita (bez memory leaks, bez segfaultů)
...

\subsection{Zhodnocení}

Implementace prokázala schopnost navrhnout a realizovat kompletní síťovou aplikaci...
Klíčové poznatky:
- Důležitost správného buffering při fragmentaci zpráv
- Nutnost thread-safe operací
...
```

---

## 📊 SOUHRN - VŠECHNY POŽADAVKY SPLNĚNY

| # | Požadavek | Sekce v dokumentaci | Status |
|---|-----------|---------------------|--------|
| 1 | Popis hry | 1.1 | ✅ |
| 2a | Formát zpráv | 2.1, 2.3 | ✅ |
| 2b | Datové typy | 2.2, 3.2 | ✅ |
| 2c | Význam dat/kódů | 2.3, 2.6 | ✅ |
| 2d | Validace dat | 2.2, 2.6 | ✅ |
| 2e | **Stavový diagram** | 2.4 **(OBRAZKOVÝ)**, 2.7 **(SEKVENČNÍ)** | ✅ |
| 2f | Chybové stavy | 2.6 | ✅ |
| 3a | Dekompozice modulů | 3.1, 4.1 | ✅ |
| 3b | Rozvrstvení | 3.1, 4.1 | ✅ |
| 3c | Knihovny, verze | 3.5, 4.3, 5.1 | ✅ |
| 3d | Paralelizace | 3.3 | ✅ |
| 4 | Požadavky na překlad | 5.1 | ✅ |
| 5 | Postup překladu | 5.2, 5.3, 5.4 | ✅ |
| 6 | Závěr | 7 | ✅ |

---

## 🎨 NOVĚ PŘIDANÉ OBRAZKOVÉ DIAGRAMY

### 1. Stavový diagram klienta (TikZ)
- **Sekce:** 2.4
- **Typ:** Stavový automat
- **Obsah:**
  - Stavy: DISCONNECTED, CONNECTED, AUTHENTICATED, IN_LOBBY, IN_ROOM, IN_GAME, SHORT_DISCONNECT, LONG_DISCONNECT
  - Přechody s příkazy (HELLO, WELCOME, JOIN_ROOM, GAME_START, ...)
  - Reconnect mechanismus
  - Barevné rozlišení (initial=červená, final=zelená)

### 2. Sekvenční diagram komunikace (TikZ)
- **Sekce:** 2.7 (nově přidáno)
- **Typ:** Sekvenční diagram
- **Obsah:**
  - 3 účastníci: Alice, Server, Bob
  - Kompletní flow: připojení → vytvoření místnosti → join → start hry → herní tah
  - Zobrazuje návaznost zpráv klient↔server

---

## 📄 VELIKOST DOKUMENTACE

- **Očekávaný počet stran:** 12-15 stran
- **Formát:** A4, 12pt, standardní margin 2.5cm
- **Kompilace:** 3× pdflatex (pro odkazy a obsah)

---

## ✅ ZÁVĚR

**Dokumentace SPLŇUJE VŠECHNY požadavky ze zadání (PozadavkyUPS.pdf strana 3).**

Všechny požadované části jsou v dokumentaci přítomny a řádně popsány:
- ✅ Popis hry
- ✅ Kompletní popis protokolu (formát, typy, validace, **OBRAZKOVÉ diagramy**)
- ✅ Implementace (moduly, vrstvy, knihovny, paralelizace)
- ✅ Překlad a spuštění
- ✅ Závěr a zhodnocení

---

**Datum kontroly:** 2026-01-11
**Dokument:** dokumentace.tex
**Zadání:** PozadavkyUPS.pdf strana 3
