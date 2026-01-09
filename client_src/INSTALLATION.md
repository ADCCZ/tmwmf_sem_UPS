# Instalace a spuštění Pexeso Klienta

Tento dokument popisuje, jak nainstalovat a spustit Pexeso klienta na různých platformách.

## Obsah
- [Quick Start - Instalátor pro Windows](#quick-start---instalátor-pro-windows)
- [Kompilace a spuštění přes Maven](#kompilace-a-spuštění-přes-maven)
  - [Linux](#linux)
  - [Windows](#windows)
- [Spuštění přes JAR soubor](#spuštění-přes-jar-soubor)

---

## Quick Start - Instalátor pro Windows

Pokud chcete jednoduchou instalaci na Windows bez vývojářských nástrojů:

1. Najděte soubor `installer/Pexeso.exe`
2. Spusťte instalátor a postupujte průvodcem
3. Aplikace se nainstaluje a bude připravena k použití

**Požadavky:** Pouze Windows OS (žádné další nástroje nejsou potřeba)

---

## Kompilace a spuštění přes Maven

### Předpoklady

Oba systémy (Linux a Windows):

```bash
# Ověřte Java verzi (vyžaduje Java 11+)
java -version

# Ověřte Maven (vyžaduje Maven 3.6+)
mvn -version
```

#### Instalace Mavenu

**Pokud nemáte Maven nainstalovaný:**

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install maven
```

**Linux (Fedora/RHEL/CentOS):**
```bash
sudo dnf install maven
```

**Linux (openSUSE):**
```bash
sudo zypper install maven
```

**macOS:**
```bash
brew install maven
```

**Windows:**
1. Stáhněte Maven z https://maven.apache.org/download.cgi
2. Rozbalte do vybraného adresáře (např. `C:\Maven`)
3. Přidejte `bin` adresář do PATH:
   - Systém → Pokročilé nastavení systému → Proměnné prostředí
   - Nová proměnná: `M2_HOME = C:\Maven`
   - Upravte PATH a přidejte `%M2_HOME%\bin`

---

### Linux

#### 1. Příprava
```bash
# Navigujte do adresáře projektu
cd /path/to/client_src

# Ověřte, že máte správné soubory
ls -la pom.xml src/
```

#### 2. Kompilace
```bash
# Zkompilujte projekt a vytvořte JAR soubor
mvn clean package
```

Tímto se vytvoří **fat JAR** s všemi závislostmi v `target/pexeso-client-1.0-SNAPSHOT.jar`.

#### 3. Spuštění

**Možnost A - Přes Maven (doporučeno):**
```bash
mvn javafx:run
```

**Možnost B - Přes JAR soubor:**
```bash
java -jar target/pexeso-client-1.0-SNAPSHOT.jar
```

#### Řešení problémů na Linuxu

**Chyba: "Cannot find display"**
- Máte bezheadový Linux (bez grafického prostředí)
- Řešení: Použijte X11 forwarding nebo SSH s `-X` volbou:
  ```bash
  ssh -X user@host
  ```

**Chyba: "javafx:run" nefunguje**
- Alternativa:
  ```bash
  mvn exec:java -Dexec.mainClass="cz.zcu.kiv.ups.pexeso.Main"
  ```

---

### Windows

#### 1. Příprava
```cmd
# Otevřete PowerShell nebo CMD
# Navigujte do adresáře projektu
cd C:\path\to\client_src

# Ověřte soubory
dir pom.xml
dir src\
```

#### 2. Kompilace
```cmd
# Zkompilujte projekt
mvn clean package
```

Na Windows se používají automaticky Windows-specifické JavaFX binárky (viz `pom.xml` s `classifier win`).

Tímto se vytvoří **fat JAR** v `target\pexeso-client-1.0-SNAPSHOT.jar`.

#### 3. Spuštění

**Možnost A - Přes Maven (doporučeno):**
```cmd
mvn javafx:run
```

**Možnost B - Přes JAR soubor:**
```cmd
java -jar target\pexeso-client-1.0-SNAPSHOT.jar
```

#### Řešení problémů na Windows

**Chyba: "mvn" není rozpoznán jako příkaz**
- Maven není v PATH
- Řešení: Nainstalujte Maven správně (viz sekce Instalace Mavenu výše)

**Chyba: "Cannot find module javafx"**
- Maven nestáhl závislosti správně
- Řešení:
  ```cmd
  mvn clean dependency:resolve
  mvn clean package
  ```

---

## Spuštění přes JAR soubor

Po kompilaci (oba systémy):

```bash
# Linux
java -jar target/pexeso-client-1.0-SNAPSHOT.jar

# Windows
java -jar target\pexeso-client-1.0-SNAPSHOT.jar
```

Příkaz `java` musí být v PATH. Ověřte:
```bash
java -version
```

---

## Technické podrobnosti

### Konfigurace projektu (pom.xml)

- **Java verze:** 11
- **JavaFX verze:** 17.0.2
- **Main class:** `cz.zcu.kiv.ups.pexeso.Main`
- **Build tool:** Maven 3.6+

### Pluginy použité pro build

1. **maven-compiler-plugin** - Kompilace Java zdrojů
2. **maven-shade-plugin** - Vytvoření fat JARu (všechny závislosti zabalené)
3. **javafx-maven-plugin** - Spouštění JavaFX aplikací
4. **maven-resources-plugin** - Kopírování FXML a dalších zdrojů

### Závislosti

- **javafx-controls** - UI komponenty
- **javafx-fxml** - Podpora FXML formátu
- **javafx-graphics** - Grafické primitiva
- **javafx-base** - Základní JavaFX třídy

---

## Shrnutí příkazů

### Linux
```bash
cd client_src
mvn clean package      # Kompilace
mvn javafx:run         # Spuštění přes Maven
# nebo
java -jar target/pexeso-client-1.0-SNAPSHOT.jar  # Spuštění přes JAR
```

### Windows
```cmd
cd C:\path\to\client_src
mvn clean package      # Kompilace
mvn javafx:run         # Spuštění přes Maven
# nebo
java -jar target\pexeso-client-1.0-SNAPSHOT.jar  # Spuštění přes JAR
```

---

## Další informace

- Projekt používá **JavaFX** pro grafické uživatelské rozhraní
- Aplikace se připojuje k serveru (pro hru)
- Konfigurační soubor: `installer/Pexeso/app/Pexeso.cfg`

Pokud máte problémy, ověřte:
1. Java verzi (Java 11+)
2. Maven instalaci
3. Přístup k internetu (Maven stahuje závislosti)
