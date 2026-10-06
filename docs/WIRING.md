# Połączenia

## GPIO ESP32

| Funkcja | GPIO | Tryb |
|---|---:|---|
| Pompa domowa | 25 | OUTPUT |
| Pompa zasilająca | 26 | OUTPUT |
| Grzejniki | 16 | INPUT_PULLUP |
| Podłogówka | 17 | INPUT_PULLUP |
| CWU | 18 | INPUT_PULLUP |
| Alarm | 19 | INPUT_PULLUP |
| BUF_OK | 23 | INPUT_PULLUP |
| Przycisk OLED | 27 | INPUT_PULLUP |
| Przycisk domowa | 32 | INPUT_PULLUP |
| Przycisk zasilająca | 33 | INPUT_PULLUP |
| OLED SDA | 21 | I2C |
| OLED SCL | 22 | I2C |
| SUPLA CONFIG | 0 | INPUT |
| LED status | 2 | OUTPUT |

## Wejścia Grzejniki / Podłogówka / CWU

Domyślna konfiguracja programu zakłada styk zwierany do GND:

```text
ESP32 GPIO ----o/ o---- GND
```

Aktywny sygnał = zwarcie wejścia do GND.

## BUF_OK

Zalecane jest użycie styku bezpotencjałowego z drugiego sterownika.

```text
Sterownik kotłowni                  Sterownik pomp

przekaźnik BUF_OK
COM ------------------------------- GND
NO  ------------------------------- GPIO23
```

Znaczenie:

```text
styk zwarty  -> BUF_OK = 1
styk otwarty -> BUF_OK = 0
```

Daje to zachowanie fail-safe dla blokady pomp: przerwany przewód powoduje `BUF_OK = 0`.

## Alarm

Kod domyślnie zakłada styk NC:

```text
normalnie: GPIO19 zwarte do GND
alarm:     styk otwarty
awaria/przerwany przewód: wejście HIGH -> Alarm
```

To powoduje uruchomienie obu pomp także przy przerwaniu obwodu alarmowego.

Jeżeli urządzenie źródłowe udostępnia styk NO, należy dostosować stałą `ALARM_ACTIVE_LOW`.

## Przyciski lokalne

Każdy przycisk chwilowy podłączony jest pomiędzy GPIO a GND:

```text
GPIO27 ---- przycisk ---- GND   OLED
GPIO32 ---- przycisk ---- GND   Pompa domowa
GPIO33 ---- przycisk ---- GND   Pompa zasilająca
```

## OLED SSD1306

```text
ESP32  -> OLED
3.3 V  -> VCC (zgodnie z modułem)
GND    -> GND
GPIO21 -> SDA
GPIO22 -> SCL
```

Adres I2C w kodzie: `0x3C`.

## Wyjścia pomp

GPIO25 i GPIO26 powinny sterować wyłącznie wejściem odpowiedniego modułu przekaźnikowego / sterownika stycznika.

Domyślnie program zakłada moduły przekaźnikowe aktywne stanem LOW:

```cpp
constexpr bool PUMP_HOME_ACTIVE_HIGH = false;
constexpr bool PUMP_SUPPLY_ACTIVE_HIGH = false;
```

Przed podłączeniem pomp należy sprawdzić rzeczywiste zachowanie modułu.

## Ważne

Nie wolno doprowadzać 230 V, 24 V lub 12 V bezpośrednio do GPIO ESP32. Dla przewodów wychodzących poza obudowę sterownika zalecana jest separacja galwaniczna i ochrona przeciwprzepięciowa odpowiednia do instalacji.
