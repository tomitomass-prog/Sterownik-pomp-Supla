# Sterownik pomp ESP32 + SUPLA

Sterownik dwóch pomp obiegowych oparty na **ESP32 DevKit V1**, z wyświetlaczem **OLED SSD1306 128x64** i integracją z **SUPLA**.

Repozytorium jest przygotowane jako projekt **PlatformIO** z frameworkiem Arduino.

## Logika priorytetów

Sterownik zawsze rozstrzyga polecenia w tej kolejności:

1. **ALARM** – obie pompy są bezwarunkowo włączone, niezależnie od `BUF_OK`.
2. **BUF_OK = 0** – obie pompy są wyłączone.
3. **Sterowanie ręczne / SUPLA** – ma pierwszeństwo przed automatyką.
4. **Automatyka**:
   - `Grzejniki` -> pompa domowa + pompa zasilająca,
   - `Podłogówka` -> pompa zasilająca,
   - `CWU` -> pompa zasilająca.

Szczegóły: [`docs/LOGIC.md`](docs/LOGIC.md)

## Funkcje

- 2 wyjścia przekaźnikowe pomp,
- 5 wejść sterujących:
  - Grzejniki,
  - Podłogówka,
  - CWU,
  - Alarm,
  - BUF_OK,
- 3 przyciski chwilowe:
  - zmiana ekranu OLED,
  - ręczne sterowanie pompą domową,
  - ręczne sterowanie pompą zasilającą,
- długie przytrzymanie przycisku pompy -> obie pompy `MAN ON`,
- OLED z informacją o stanach pomp, wejść i powodach działania,
- SUPLA:
  - zdalne wymuszenie pompy domowej ON,
  - zdalne wymuszenie pompy zasilającej ON,
  - podgląd rzeczywistych stanów pomp,
  - podgląd wszystkich wejść.

## Przyciski pomp

Krótkie naciśnięcie przełącza tryb danej pompy:

```text
AUTO -> MAN ON -> MAN OFF -> AUTO
```

Długie przytrzymanie przycisku pompy domowej lub zasilającej ustawia:

```text
DOMOWA      = MAN ON
ZASILAJĄCA  = MAN ON
```

Sterowanie ręczne **nie omija blokady BUF_OK**. Jedynie aktywny Alarm może wymusić pracę obu pomp przy `BUF_OK = 0`.

## GPIO

| Funkcja | GPIO |
|---|---:|
| Pompa domowa | 25 |
| Pompa zasilająca | 26 |
| Grzejniki | 16 |
| Podłogówka | 17 |
| CWU | 18 |
| Alarm | 19 |
| BUF_OK | 23 |
| Przycisk OLED | 27 |
| Przycisk pompy domowej | 32 |
| Przycisk pompy zasilającej | 33 |
| OLED SDA | 21 |
| OLED SCL | 22 |
| SUPLA CONFIG / BOOT | 0 |
| LED statusu | 2 |

Pełny opis połączeń: [`docs/WIRING.md`](docs/WIRING.md)

## BUF_OK

Wejście `BUF_OK` powinno być dostarczone ze sterownika kotłowni jako **styk bezpotencjałowy**.

Domyślna konfiguracja:

```text
styk zamknięty do GND = BUF_OK = 1
otwarty styk          = BUF_OK = 0
```

Dzięki temu przerwany przewód lub utrata zasilania urządzenia źródłowego powoduje zablokowanie pomp, o ile nie jest aktywny Alarm.

## Alarm

Domyślnie kod zakłada obwód typu **NC**:

```text
normalnie / brak alarmu   = wejście zwarte do GND
alarm / przerwany przewód = wejście HIGH
```

W razie użycia styku NO należy zmienić stałą `ALARM_ACTIVE_LOW` w `src/main.cpp`.

## SUPLA – kanały

| Kanał | Funkcja |
|---:|---|
| 0 | Zdalne wymuszenie pompy domowej ON |
| 1 | Zdalne wymuszenie pompy zasilającej ON |
| 2 | Stan pompy domowej |
| 3 | Stan pompy zasilającej |
| 4 | Grzejniki |
| 5 | Podłogówka |
| 6 | CWU |
| 7 | Alarm |
| 8 | BUF_OK |

W SUPLA `OFF` na kanałach zdalnego sterowania oznacza **brak zdalnego wymuszenia**, a nie wymuszenie pompy OFF.

## OLED

Sterownik ma trzy ekrany:

1. stan pomp i powód ich pracy,
2. stany wejść,
3. tryby lokalne i SUPLA.

Przykładowe skróty powodów działania:

- `ALARM` – wymuszenie alarmowe,
- `BUF` – blokada BUF_OK,
- `MAN+` – lokalne wymuszenie ON,
- `MAN-` – lokalne wymuszenie OFF,
- `SUPLA` – zdalne wymuszenie ON,
- `GRZ` – grzejniki,
- `POD` – podłogówka,
- `CWU` – ciepła woda użytkowa,
- `AUTO` – brak aktywnego żądania.

## PlatformIO

Projekt używa:

- platformy `espressif32`,
- płytki `esp32dev`,
- frameworka Arduino,
- bibliotek zadeklarowanych w `platformio.ini`:
  - `SuplaDevice`,
  - `Adafruit GFX Library`,
  - `Adafruit SSD1306`.

### Kompilacja w VS Code + PlatformIO

1. Zainstaluj **Visual Studio Code**.
2. Zainstaluj rozszerzenie **PlatformIO IDE**.
3. Otwórz katalog repozytorium jako projekt.
4. PlatformIO odczyta `platformio.ini` i pobierze wymagane biblioteki.
5. Wybierz **PlatformIO: Build**.
6. Podłącz ESP32 i wybierz **PlatformIO: Upload**.
7. Monitor portu szeregowego pracuje z prędkością `115200`.

### Kompilacja z terminala

```bash
pio run
```

Wgranie programu:

```bash
pio run --target upload
```

Monitor portu szeregowego:

```bash
pio device monitor
```

Więcej informacji: [`docs/BUILDING.md`](docs/BUILDING.md)

## Automatyczna kompilacja na GitHubie

Repozytorium zawiera workflow:

```text
.github/workflows/platformio.yml
```

Przy `push` do gałęzi `main` lub `master` oraz przy Pull Request GitHub Actions uruchamia `pio run`. Dzięki temu błędy kompilacji mogą być wykryte przed wgraniem firmware do ESP32.

## Bezpieczeństwo

ESP32 pracuje z logiką 3,3 V. **Nie wolno podawać 230 V, 24 V ani 12 V bezpośrednio na GPIO.**

Sygnały z urządzeń instalacji grzewczej powinny być podawane przez styki bezpotencjałowe, transoptory albo odpowiednie moduły separujące. Pompy sieciowe powinny być sterowane przez właściwe przekaźniki/styczniki dobrane do ich obciążenia.

## Struktura repozytorium

```text
sterownik-pomp-esp32-supla/
├── .github/
│   └── workflows/
│       └── platformio.yml
├── docs/
│   ├── BUILDING.md
│   ├── LOGIC.md
│   ├── SUPLA.md
│   ├── TESTING.md
│   └── WIRING.md
├── include/
│   └── README.md
├── lib/
│   └── README.md
├── src/
│   └── main.cpp
├── test/
│   └── README.md
├── .gitignore
├── CHANGELOG.md
├── README.md
└── platformio.ini
```

## Status

Wersja repozytorium: **1.1.0 – PlatformIO**.

Przed podłączeniem rzeczywistych pomp należy wykonać test na stole i sprawdzić rzeczywistą polaryzację wejść oraz przekaźników. Scenariusze testowe znajdują się w [`docs/TESTING.md`](docs/TESTING.md).
