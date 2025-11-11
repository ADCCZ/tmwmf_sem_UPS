  ---
  SPECIFIKACE KOMUNIKAČNÍHO PROTOKOLU PRO HRU PEXESO

  1. ZÁKLADNÍ CHARAKTERISTIKA

  Typ protokolu: Textový, na aplikační vrstvěTransportní 
  protokol: TCPKódování: ASCII (bez diakritiky)Ukončení 
  zprávy: Každá zpráva je ukončena znakem nového řádku
  (\n)Formát zprávy: COMMAND [PARAM1] [PARAM2] ...
  \nOddělování parametrů: Jedna mezera (ASCII 0x20)Reakce
   na požadavky: Na každý požadavek přijde odpověď (OK,
  ERROR nebo specifická odpověď)

  ---
  2. PRAVIDLA HRY PEXESO (implementovaná varianta)

  - Hra pro 2 až 4 hráče
  - Herní deska obsahuje N párů karet (typicky 8, 12, 16
  nebo 18 párů = 16, 24, 32, 36 karet)
  - Každá karta má hodnotu (číslo symbolu), každá hodnota
   se vyskytuje přesně 2×
  - Hráči se střídají v tazích
  - V každém tahu hráč otočí 2 karty:
    - Pokud se hodnoty shodují: Hráč získává 1 bod a
  hraje znovu
    - Pokud se neshodují: Karty se vrátí lícem dolů a
  přichází na řadu další hráč
  - Hra končí, když jsou všechny páry nalezeny
  - Vyhrává hráč s nejvyšším skóre

  ---
  3. DATOVÉ TYPY A OMEZENÍ

  | Typ             | Popis                         |
  Formát                    | Omezení                 |
  |-----------------|-------------------------------|----
  -----------------------|-------------------------|
  | nickname        | Přezdívka hráče               |
  String bez mezer          | 1-16 znaků, a-zA-Z0-9_- |
  | room_id         | Identifikátor místnosti       |
  Celé číslo                | 1-9999                  |
  | room_name       | Název místnosti               |
  String bez mezer          | 1-20 znaků, a-zA-Z0-9_- |
  | client_id       | ID klienta přidělené serverem |
  Celé číslo                | 1-9999                  |
  | card_id         | Index karty na desce          |
  Celé číslo                | 0 až (board_size-1)     |
  | card_value      | Hodnota karty (symbol)        |
  Celé číslo                | 0 až (board_size/2 - 1) |
  | board_size      | Počet karet celkem            |
  Celé číslo                | 16, 24, 32, 36 (sudý)   |
  | max_players     | Max. počet hráčů v místnosti  |
  Celé číslo                | 2-4                     |
  | score           | Skóre hráče                   |
  Celé číslo                | >= 0                    |
  | error_code      | Kód chyby                     |
  Celé číslo                | 100-599                 |
  | message         | Textová zpráva                |
  String (může obsahovat _) | Max. 100 znaků          |
  | status          | Stav místnosti                |
  String                    | WAITING, PLAYING        |
  | disconnect_type | Typ výpadku                   |
  String                    | SHORT, LONG             |

  ---
  4. ZPRÁVY OD KLIENTA K SERVERU

  4.1 HELLO

  Účel: První zpráva po navázání TCP spojení,
  identifikace hráčeFormát: HELLO <nickname>Parametry:
  - nickname – přezdívka hráče (unique, bez mezer)

  Kdy se posílá: Ihned po úspěšném TCP připojeníOčekávaná
   odpověď: WELCOME nebo ERRORPříklad: HELLO Petr123

  ---
  4.2 LIST_ROOMS

  Účel: Požadavek na výpis všech existujících
  místnostíFormát: LIST_ROOMSParametry: žádnéKdy se 
  posílá: Kdykoli v lobbyOčekávaná odpověď:
  ROOM_LISTPříklad: LIST_ROOMS

  ---
  4.3 CREATE_ROOM

  Účel: Vytvoření nové herní místnostiFormát: CREATE_ROOM
   <room_name> <max_players> <board_size>Parametry:
  - room_name – název místnosti (bez mezer)
  - max_players – maximální počet hráčů (2-4)
  - board_size – počet karet na desce (16, 24, 32, 36)

  Kdy se posílá: V lobby, když hráč chce založit
  místnostOčekávaná odpověď: ROOM_CREATED nebo
  ERRORPříklad: CREATE_ROOM MyRoom 3 24

  ---
  4.4 JOIN_ROOM

  Účel: Vstup do existující místnostiFormát: JOIN_ROOM
  <room_id>Parametry:
  - room_id – ID místnosti (ze seznamu)

  Kdy se posílá: V lobby, když hráč chce vstoupit do
  místnostiOčekávaná odpověď: ROOM_JOINED nebo
  ERRORPříklad: JOIN_ROOM 42

  ---
  4.5 LEAVE_ROOM

  Účel: Opuštění místnosti (návrat do lobby)Formát:
  LEAVE_ROOMParametry: žádnéKdy se posílá: Když je hráč v
   místnosti (před začátkem hry) a chce ji
  opustitOčekávaná odpověď: ROOM_LEFT nebo ERRORPříklad:
  LEAVE_ROOM

  ---
  4.6 START_GAME

  Účel: Zahájení hry (pouze tvůrce místnosti)Formát:
  START_GAMEParametry: žádnéKdy se posílá: Když je v
  místnosti alespoň 2 hráči a tvůrce chce hru
  spustitOčekávaná odpověď: GAME_START (broadcast všem v
  místnosti) nebo ERRORPříklad: START_GAME

  ---
  4.7 FLIP

  Účel: Otočení karty (herní tah)Formát: FLIP
  <card_id>Parametry:
  - card_id – index karty na herní desce (0 až
  board_size-1)

  Kdy se posílá: Když je hráč na tahu a chce otočit
  kartuOčekávaná odpověď: CARD_REVEAL, následně MATCH
  nebo MISMATCHPříklad: FLIP 15

  ---
  4.8 PONG

  Účel: Odpověď na PING od serveru (udržování
  spojení)Formát: PONGParametry: žádnéKdy se posílá: Jako
   reakce na příchozí PING od serveruOčekávaná odpověď:
  žádná (server jen zaznamenává aktivitu)Příklad: PONG

  ---
  4.9 DISCONNECT

  Účel: Korektní odpojení klientaFormát:
  DISCONNECTParametry: žádnéKdy se posílá: Když klient
  chce ukončit aplikaci/spojeníOčekávaná odpověď: Server
  uzavře spojeníPříklad: DISCONNECT

  ---
  4.10 RECONNECT

  Účel: Znovupřipojení po krátkodobém výpadkuFormát:
  RECONNECT <nickname>Parametry:
  - nickname – původní přezdívka hráče

  Kdy se posílá: Po obnovení TCP spojení, pokud klient
  detekuje, že byl odpojenOčekávaná odpověď: WELCOME +
  stav hry, nebo ERROR (pokud už je timeout)Příklad:
  RECONNECT Petr123

  ---
  5. ZPRÁVY OD SERVERU K KLIENTOVI

  5.1 WELCOME

  Účel: Potvrzení úspěšného připojení a
  identifikaceFormát: WELCOME <client_id>Parametry:
  - client_id – unikátní ID přidělené klientovi

  Kdy se posílá: Jako odpověď na HELLO nebo úspěšné
  RECONNECTPříklad: WELCOME 1234

  ---
  5.2 ERROR

  Účel: Oznámení chybyFormát: ERROR <error_code>
  <message>Parametry:
  - error_code – číselný kód chyby
  - message – textový popis chyby (podtržítka místo
  mezer)

  Chybové kódy:
  - 100 – Invalid_command (neznámý příkaz)
  - 101 – Invalid_parameters (špatné parametry)
  - 102 – Invalid_state (příkaz nelze provést v aktuálním
   stavu)
  - 200 – Nickname_already_in_use
  - 201 – Nickname_too_long
  - 202 – Nickname_invalid_characters
  - 300 – Room_not_found
  - 301 – Room_full
  - 302 – Room_already_playing
  - 303 – Not_room_owner (nelze spustit hru)
  - 304 – Not_enough_players (min. 2 hráči)
  - 305 – Room_limit_reached (server má plno místností)
  - 400 – Not_your_turn
  - 401 – Card_already_revealed
  - 402 – Card_out_of_bounds
  - 403 – Two_cards_must_be_flipped (očekává se druhá
  karta)
  - 500 – Server_error (interní chyba serveru)

  Kdy se posílá: Jako odpověď na neplatný příkaz nebo
  operaciPříklad: ERROR 300 Room_not_found

  ---
  5.3 ROOM_LIST

  Účel: Seznam všech místnostíFormát: ROOM_LIST <count>
  [<room_id> <name> <players> <max_players> <status>]
  ...Parametry:
  - count – počet místností
  - Pro každou místnost:
    - room_id – ID místnosti
    - name – název místnosti
    - players – aktuální počet hráčů
    - max_players – maximální počet hráčů
    - status – stav (WAITING, PLAYING)

  Kdy se posílá: Jako odpověď na LIST_ROOMSPříklad:
  ROOM_LIST 2 1 Room1 2 3 WAITING 5 Room2 3 4
  PLAYINGPříklad (prázdný seznam): ROOM_LIST 0

  ---
  5.4 ROOM_CREATED

  Účel: Potvrzení vytvoření místnostiFormát: ROOM_CREATED
   <room_id>Parametry:
  - room_id – ID nově vytvořené místnosti

  Kdy se posílá: Jako odpověď na CREATE_ROOMPoznámka:
  Klient je automaticky přidán do místnostiPříklad:
  ROOM_CREATED 7

  ---
  5.5 ROOM_JOINED

  Účel: Potvrzení vstupu do místnostiFormát: ROOM_JOINED
  <room_id> <player_count> [<nickname>] ...Parametry:
  - room_id – ID místnosti
  - player_count – počet hráčů v místnosti
  - seznam nickname – přezdívky všech hráčů v místnosti

  Kdy se posílá: Jako odpověď na JOIN_ROOMPříklad:
  ROOM_JOINED 7 3 Alice Bob Charlie

  ---
  5.6 ROOM_LEFT

  Účel: Potvrzení opuštění místnostiFormát:
  ROOM_LEFTParametry: žádnéKdy se posílá: Jako odpověď na
   LEAVE_ROOMPříklad: ROOM_LEFT

  ---
  5.7 PLAYER_JOINED

  Účel: Informace o novém hráči v místnosti
  (broadcast)Formát: PLAYER_JOINED <nickname>Parametry:
  - nickname – přezdívka hráče, který vstoupil

  Kdy se posílá: Server posílá všem hráčům v místnosti,
  když někdo nový vstoupíPříklad: PLAYER_JOINED David

  ---
  5.8 PLAYER_LEFT

  Účel: Informace o odchodu hráče (broadcast)Formát:
  PLAYER_LEFT <nickname>Parametry:
  - nickname – přezdívka hráče, který odešel

  Kdy se posílá: Server posílá všem v místnosti, když
  někdo opustí místnostPříklad: PLAYER_LEFT David

  ---
  5.9 GAME_START

  Účel: Zahájení hry (broadcast všem v místnosti)Formát:
  GAME_START <board_size> <player_count> [<nickname>]
  ...Parametry:
  - board_size – počet karet (16, 24, 32, 36)
  - player_count – počet hráčů ve hře
  - seznam nickname – přezdívky hráčů v pořadí, jak budou
   hrát

  Kdy se posílá: Když tvůrce místnosti pošle START_GAME a
   podmínky jsou splněnyPříklad: GAME_START 24 3 Alice
  Bob Charlie

  ---
  5.10 YOUR_TURN

  Účel: Informace, že je hráč na tahuFormát:
  YOUR_TURNParametry: žádnéKdy se posílá: Server posílá
  hráči, který je aktuálně na tahuPříklad: YOUR_TURN

  ---
  5.11 TURN

  Účel: Informace o tom, kdo je na tahu (broadcast
  ostatním)Formát: TURN <nickname>Parametry:
  - nickname – přezdívka hráče, který je na tahu

  Kdy se posílá: Všem hráčům kromě toho, kdo je na tahu
  (ten dostane YOUR_TURN)Příklad: TURN Alice

  ---
  5.12 CARD_REVEAL

  Účel: Odhalení hodnoty karty (broadcast všem ve
  hře)Formát: CARD_REVEAL <card_id> <card_value>
  <nickname>Parametry:
  - card_id – index karty
  - card_value – hodnota (symbol) na kartě
  - nickname – hráč, který kartu otočil

  Kdy se posílá: Po příkazu FLIP od hráče na tahuPříklad:
   CARD_REVEAL 15 7 Alice

  ---
  5.13 MATCH

  Účel: Oznámení o nalezení páru (broadcast)Formát: MATCH
   <nickname> <score>Parametry:
  - nickname – hráč, který našel pár
  - score – nové skóre tohoto hráče

  Kdy se posílá: Když hráč otočí dvě karty se stejnou
  hodnotouPoznámka: Hráč zůstává na tahuPříklad: MATCH
  Alice 5

  ---
  5.14 MISMATCH

  Účel: Oznámení, že karty se neshodují
  (broadcast)Formát: MISMATCHParametry: žádnéKdy se 
  posílá: Když hráč otočí dvě karty s různou
  hodnotouPoznámka: Karty se vracejí lícem dolů, přichází
   na řadu další hráčPříklad: MISMATCH

  ---
  5.15 GAME_END

  Účel: Konec hry s výsledky (broadcast)Formát: GAME_END
  <winner> <player_count> [<nickname> <score>]
  ...Parametry:
  - winner – přezdívka vítěze (nebo "DRAW" při remíze)
  - player_count – počet hráčů
  - Pro každého hráče: nickname a jeho konečné score

  Kdy se posílá: Když jsou nalezeny všechny páry
  karetPoznámka: Hráči jsou automaticky vráceni do
  lobbyPříklad: GAME_END Alice 3 Alice 8 Bob 5 Charlie 3

  ---
  5.16 PING

  Účel: Kontrola spojení (keepalive)Formát:
  PINGParametry: žádnéKdy se posílá: Server periodicky
  (např. každých 15 sekund), když nedochází k jiné
  komunikaciOčekávaná odpověď: PONG do 5 sekundPříklad:
  PING

  ---
  5.17 PLAYER_DISCONNECTED

  Účel: Oznámení o výpadku hráče (broadcast)Formát:
  PLAYER_DISCONNECTED <nickname>
  <disconnect_type>Parametry:
  - nickname – přezdívka odpojeného hráče
  - disconnect_type – typ výpadku (SHORT nebo LONG)

  Kdy se posílá:
  - SHORT – když hráč neodpověděl na PING, ale je v
  tolerančním okně (čeká se na návrat)
  - LONG – když timeout vypršel, hráč je vyhozen ze hry

  Příklad: PLAYER_DISCONNECTED Bob SHORTPříklad:
  PLAYER_DISCONNECTED Bob LONG

  ---
  5.18 PLAYER_RECONNECTED

  Účel: Oznámení o návratu hráče (broadcast)Formát:
  PLAYER_RECONNECTED <nickname>Parametry:
  - nickname – přezdívka hráče, který se vrátil

  Kdy se posílá: Když se hráč úspěšně připojí po
  krátkodobém výpadkuPoznámka: Server pošle hráči
  aktuální stav hry (GAME_STATE)Příklad:
  PLAYER_RECONNECTED Bob

  ---
  5.19 GAME_STATE

  Účel: Poslání aktuálního stavu hry (po
  reconnect)Formát: GAME_STATE <board_size>
  <revealed_count> [<card_id> <card_value>] ...
  <player_count> [<nickname> <score>] ...
  <current_turn>Parametry:
  - board_size – počet kart
  - revealed_count – počet již odhalených (spárovaných)
  karet
  - Pro každou odhalenou kartu: card_id a card_value
  - player_count – počet hráčů
  - Pro každého hráče: nickname a score
  - current_turn – přezdívka hráče, který je aktuálně na
  tahu

  Kdy se posílá: Po úspěšném RECONNECT, aby klient dostal
   aktuální stavPříklad: GAME_STATE 24 8 0 3 1 3 5 7 6 7
  14 9 15 9 20 11 21 11 3 Alice 4 Bob 2 Charlie 2 Alice

  ---
  5.20 OK

  Účel: Obecné potvrzení operaceFormát: OKParametry:
  žádnéKdy se posílá: Když je potřeba potvrdit příkaz,
  který nemá specifickou odpověďPříklad: OK

  ---
  6. STAVOVÝ DIAGRAM

  6.1 Stavy klienta

  1. DISCONNECTED (počáteční stav)
     └─> [TCP connect]
         └─> CONNECTED

  2. CONNECTED (TCP spojení navázáno)
     └─> [pošle HELLO]
         └─> AUTHENTICATED

  3. AUTHENTICATED (identifikován serverem)
     └─> [dostane WELCOME]
         └─> IN_LOBBY

  4. IN_LOBBY (v lobby, vidí seznam místností)
     ├─> [pošle CREATE_ROOM nebo JOIN_ROOM]
     │   └─> IN_ROOM
     └─> [pošle DISCONNECT]
         └─> DISCONNECTED

  5. IN_ROOM (v místnosti, čeká na start)
     ├─> [dostane GAME_START]
     │   └─> IN_GAME
     ├─> [pošle LEAVE_ROOM]
     │   └─> IN_LOBBY
     └─> [pošle DISCONNECT nebo timeout]
         └─> DISCONNECTED

  6. IN_GAME (probíhá hra)
     ├─> [dostane GAME_END]
     │   └─> IN_LOBBY
     ├─> [pošle DISCONNECT nebo timeout]
     │   └─> DISCONNECTED
     └─> [pošle FLIP, dostává CARD_REVEAL, 
  MATCH/MISMATCH, TURN/YOUR_TURN]
         └─> zůstává v IN_GAME

  7. (DISCONNECTED)
     └─> [TCP reconnect + RECONNECT]
         └─> návrat do původního stavu
  (AUTHENTICATED/IN_LOBBY/IN_ROOM/IN_GAME)

  6.2 Stavy serveru (pro každého klienta)

  1. NOT_CONNECTED (čeká na připojení)
     └─> [accept TCP]
         └─> CONNECTED

  2. CONNECTED (TCP spojení aktivní, čeká na HELLO)
     ├─> [dostane HELLO]
     │   └─> AUTHENTICATED
     └─> [timeout nebo chyba]
         └─> NOT_CONNECTED

  3. AUTHENTICATED / IN_LOBBY
     ├─> [dostane CREATE_ROOM nebo JOIN_ROOM]
     │   └─> IN_ROOM
     └─> [dostane DISCONNECT nebo chyba]
         └─> NOT_CONNECTED

  4. IN_ROOM (v místnosti)
     ├─> [dostane START_GAME (jako owner)]
     │   └─> IN_GAME
     ├─> [dostane LEAVE_ROOM]
     │   └─> IN_LOBBY
     └─> [timeout nebo chyba]
         └─> SHORT_DISCONNECT

  5. IN_GAME (hra probíhá)
     ├─> [dostane FLIP (pokud je na tahu)]
     │   └─> zůstává v IN_GAME
     ├─> [hra končí]
     │   └─> IN_LOBBY
     └─> [timeout nebo chyba]
         └─> SHORT_DISCONNECT

  6. SHORT_DISCONNECT (krátkodobý výpadek, čeká se na 
  návrat)
     ├─> [dostane RECONNECT do timeout limitu, např. 60s]
     │   └─> návrat do původního stavu (IN_ROOM nebo
  IN_GAME)
     └─> [timeout expire]
         └─> LONG_DISCONNECT

  7. LONG_DISCONNECT (dlouhodobý výpadek, hráč vyhozen)
     └─> server uvolní prostředky, ostatní dostanou 
  PLAYER_DISCONNECTED LONG
         └─> NOT_CONNECTED

  6.3 Průběh typické hry (sekvenční diagram)

  KLIENT                          SERVER

  [TCP connect]
     │
     ├──> HELLO Alice ───────────>
     │
     <─────────── WELCOME 1001 <──┤
     │                            │
     │        (stav: IN_LOBBY)    │
     │                            │
     ├──> LIST_ROOMS ────────────>
     │
     <─── ROOM_LIST 1 ... <───────┤
     │                            │
     ├──> CREATE_ROOM MyRoom 3 24 >
     │
     <─── ROOM_CREATED 5 <────────┤
     │                            │
     │     (stav: IN_ROOM)        │
     │                            │

  [Další klient Bob se připojí a vstoupí]

     │                            │
     <─── PLAYER_JOINED Bob <─────┤
     │                            │
     ├──> START_GAME ────────────>
     │                            │
     <─── GAME_START 24 2 Alice Bob <─┤
     │                            │
     │     (stav: IN_GAME)        │
     │                            │
     <─── YOUR_TURN <─────────────┤
     │                            │
     ├──> FLIP 10 ───────────────>
     │                            │
     <─── CARD_REVEAL 10 5 Alice <┤
     │                            │
     ├──> FLIP 14 ───────────────>
     │                            │
     <─── CARD_REVEAL 14 5 Alice <┤
     │                            │
     <─── MATCH Alice 1 <─────────┤
     │                            │
     <─── YOUR_TURN <─────────────┤
     │                            │
     ├──> FLIP 3 ────────────────>
     │                            │
     <─── CARD_REVEAL 3 2 Alice <─┤
     │                            │
     ├──> FLIP 7 ────────────────>
     │                            │
     <─── CARD_REVEAL 7 9 Alice <─┤
     │                            │
     <─── MISMATCH <──────────────┤
     │                            │
     <─── TURN Bob <──────────────┤
     │                            │
     │   ... (hra pokračuje) ...  │
     │                            │
     <─── GAME_END Alice 3 Alice 7 Bob 5 <─┤
     │                            │
     │     (stav: IN_LOBBY)       │
     │                            │
     ├──> DISCONNECT ────────────>
     │                            │
     [TCP close]

  6.4 Průběh výpadku a zotavení

  KLIENT                          SERVER

     │     (stav: IN_GAME)        │
     │                            │
     <─── PING <──────────────────┤
     │                            │
     ├──> PONG ───────────────────> [server: klient OK]
     │                            │
     │                            │
     X  [síťový výpadek]          │
     │                            │
     │                            server čeká na PONG...
     │                            timeout (5s) -> 
  SHORT_DISCONNECT
     │                            │
     │                            broadcast:
  PLAYER_DISCONNECTED Alice SHORT
     │                            │
     │                            hra pozastavena (čeká 
  60s)
     │                            │
     │  [síť obnovena]            │
     │                            │
     ├──> RECONNECT Alice ───────>
     │                            │
     <─── WELCOME 1001 <──────────┤
     │                            │
     <─── GAME_STATE ... <────────┤ [aktuální stav hry]
     │                            │
     │                            broadcast:
  PLAYER_RECONNECTED Alice
     │                            │
     │     (stav: IN_GAME)        │
     │     hra pokračuje          │


  --- Pokud by výpadek trval > 60s ---

     │                            server timeout ->
  LONG_DISCONNECT
     │                            │
     │                            broadcast: 
  PLAYER_DISCONNECTED Alice LONG
     │                            │
     │                            hra končí (pokud méně
  než 2 aktivní hráči)
     │                            nebo pokračuje bez
  Alice
     │                            │
     <─── GAME_END ... <──────────┤ [ostatní hráči -> 
  lobby]

  ---
  7. ŘEŠENÍ VÝPADKŮ

  7.1 Krátkodobý výpadek (SHORT_DISCONNECT)

  - Detekce: Server posílá PING každých 15s, pokud
  nedochází k jiné komunikaci
  - Timeout: Pokud klient neodpoví PONG do 5s →
  SHORT_DISCONNECT
  - Reakce serveru:
    - Označí klienta jako dočasně nedostupného
    - Broadcast PLAYER_DISCONNECTED <nick> SHORT všem
  hráčům ve hře
    - Hra se pozastaví (nikdo není na tahu)
    - Čeká na návrat (např. 60s)
  - Reakce klienta:
    - Klient se automaticky pokusí o znovupřipojení (TCP
  reconnect)
    - Pošle RECONNECT <nickname>
    - Dostane WELCOME a GAME_STATE s aktuálním stavem
    - Broadcast PLAYER_RECONNECTED <nick> ostatním
    - Hra pokračuje

  7.2 Dlouhodobý výpadek (LONG_DISCONNECT)

  - Detekce: Timeout pro SHORT_DISCONNECT vyprší (např.
  60s bez odpovědi)
  - Reakce serveru:
    - Broadcast PLAYER_DISCONNECTED <nick> LONG
    - Hráč je odstraněn ze hry
    - Pokud zbývá < 2 hráčů → hra končí → GAME_END →
  návrat do lobby
    - Pokud zbývá >= 2 hráčů → hra může pokračovat
  - Reakce klienta:
    - Pokud se hráč pokusí připojit po timeoutu, dostane
  ERROR 500 nebo je vrácen do lobby

  7.3 Korektní odpojení

  - Klient pošle DISCONNECT před uzavřením TCP spojení
  - Server okamžitě odstraní klienta z místnosti/hry
  - Broadcast PLAYER_LEFT nebo PLAYER_DISCONNECTED
  ostatním

  ---
  8. NEVALIDNÍ ZPRÁVY A JEJICH OŠETŘENÍ

  8.1 Typy nevalidních zpráv

  1. Náhodná data (např. z /dev/urandom) → nerozpoznaný
  příkaz
  2. Špatný formát (chybějící parametry, špatný typ)
  3. Neplatné hodnoty (např. card_id=-1, nickname="")
  4. Příkaz ve špatném stavu (např. FLIP když není na
  tahu)
  5. Porušení pravidel (např. otočení stejné karty 2×)

  8.2 Reakce serveru

  - Server odešle ERROR <code> <message>
  - Server zvýší počítadlo nevalidních zpráv pro daného
  klienta
  - Po 3 nevalidních zprávách (konfigurovatelné) → server
   odpojí klienta
  - Všechny chyby jsou logovány

  ---
  9. ROZŠIŘITELNOST PROTOKOLU

  Protokol je navržen tak, aby umožňoval budoucí
  rozšíření:

  1. Nové herní režimy: Přidání parametru game_mode do
  CREATE_ROOM
  2. Chat: Nový příkaz CHAT <message> a odpověď CHAT_MSG
  <nick> <msg>
  3. Statistiky: Nový příkaz GET_STATS → odpověď STATS
  ...
  4. Žebříčky: Nový příkaz LEADERBOARD → odpověď
  LEADERBOARD ...
  5. Pozorování her: Nový příkaz SPECTATE <room_id> pro
  sledování bez hraní
  6. Autentizace: Přidání hesla do HELLO <nick>
  <password_hash>

  ---
  10. LOGOVÁNÍ

  10.1 Server loguje:

  - Připojení/odpojení klientů (s timestamp, IP,
  nickname)
  - Vytvoření/zrušení místností
  - Start/konec her (účastníci, výsledky)
  - Výpadky hráčů (SHORT/LONG)
  - Všechny nevalidní zprávy (typ chyby, nick, obsah
  zprávy)
  - Interní chyby serveru

  10.2 Klient loguje:

  - Připojení/odpojení ze serveru
  - Vstup/výstup z místností
  - Průběh her (tahy, výsledky)
  - Chyby (síťové, od serveru)
  - Výpadky spojení a reconnect

  ---
  11. PŘÍKLAD KOMPLETNÍ KOMUNIKACE

  // Alice se připojuje
  C->S: HELLO Alice
  S->C: WELCOME 1

  // Alice v lobby
  C->S: LIST_ROOMS
  S->C: ROOM_LIST 0

  // Alice vytváří místnost
  C->S: CREATE_ROOM Game1 2 16
  S->C: ROOM_CREATED 1

  // Bob se připojuje
  C2->S: HELLO Bob
  S->C2: WELCOME 2

  C2->S: LIST_ROOMS
  S->C2: ROOM_LIST 1 1 Game1 1 2 WAITING

  C2->S: JOIN_ROOM 1
  S->C2: ROOM_JOINED 1 2 Alice Bob
  S->C: PLAYER_JOINED Bob

  // Alice spouští hru
  C->S: START_GAME
  S->C: GAME_START 16 2 Alice Bob
  S->C2: GAME_START 16 2 Alice Bob

  S->C: YOUR_TURN
  S->C2: TURN Alice

  // Alice hraje
  C->S: FLIP 0
  S->C: CARD_REVEAL 0 3 Alice
  S->C2: CARD_REVEAL 0 3 Alice

  C->S: FLIP 5
  S->C: CARD_REVEAL 5 7 Alice
  S->C2: CARD_REVEAL 5 7 Alice

  S->C: MISMATCH
  S->C2: MISMATCH

  S->C: TURN Bob
  S->C2: YOUR_TURN

  // Bob hraje
  C2->S: FLIP 1
  S->C: CARD_REVEAL 1 5 Bob
  S->C2: CARD_REVEAL 1 5 Bob

  C2->S: FLIP 9
  S->C: CARD_REVEAL 9 5 Bob
  S->C2: CARD_REVEAL 9 5 Bob

  S->C: MATCH Bob 1
  S->C2: MATCH Bob 1

  S->C: TURN Bob
  S->C2: YOUR_TURN

  // ... hra pokračuje ...

  // Konec hry
  S->C: GAME_END Bob 2 Alice 3 Bob 5
  S->C2: GAME_END Bob 2 Alice 3 Bob 5

  // Oba jsou zpět v lobby

  ---
  12. POZNÁMKY K IMPLEMENTACI

  1. Ukončení zpráv: Každá zpráva musí být ukončena \n
  2. Buffer handling: Server i klient musí správně
  zpracovat částečné zprávy (buffering)
  3. Timeouty: Doporučené hodnoty:
    - PING interval: 15s
    - PONG timeout: 5s
    - SHORT_DISCONNECT timeout: 60s
  4. Thread safety: Server musí zajistit synchronizaci
  přístupu ke sdíleným datům (místnosti, hry)
  5. Garbage collection: Server musí uvolňovat neaktivní
  místnosti a odpojené klienty
  6. Validace: Veškeré vstupy od klienta musí být
  validovány před použitím

  ---
  13. SHRNUTÍ KLÍČOVÝCH VLASTNOSTÍ PROTOKOLU

  ✓ Textový, čitelný, snadno laditelný✓ Jasná struktura
  zpráv s pevným formátem✓ Explicitní odpovědi na každý
  požadavek✓ Podpora lobby a více místností současně✓
  Kompletní řešení krátkodobých i dlouhodobých výpadků✓
  Keepalive mechanismus (PING/PONG)✓ Robustní error
  handling s číselnými kódy✓ Broadcast mechanismus pro
  informování všech hráčů✓ Stavový model s jasnými
  přechody✓ Rozšiřitelný pro budoucí funkce✓ Bez
  závislosti na externích knihovnách

  ---