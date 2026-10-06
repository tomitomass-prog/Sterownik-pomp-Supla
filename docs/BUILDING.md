# Budowanie projektu w PlatformIO

## Środowisko

Konfiguracja znajduje się w pliku `platformio.ini`.

Domyślne środowisko:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
```

Kod główny znajduje się w:

```text
src/main.cpp
```

## Wymagane biblioteki

PlatformIO pobiera je automatycznie na podstawie `platformio.ini`:

- SuplaDevice,
- Adafruit GFX Library,
- Adafruit SSD1306.

## Budowanie

W katalogu głównym projektu:

```bash
pio run
```

Po poprawnym buildzie pliki wynikowe znajdą się w katalogu `.pio/`.

## Wgrywanie przez USB

```bash
pio run --target upload
```

Jeżeli PlatformIO nie wybierze właściwego portu automatycznie, można podać go lokalnie przez opcję `upload_port` albo użyć parametru polecenia PlatformIO. Nie zapisuj w repozytorium ustawień zależnych od konkretnego komputera, jeśli nie są potrzebne wszystkim użytkownikom projektu.

## Monitor portu szeregowego

```bash
pio device monitor
```

Prędkość jest ustawiona w `platformio.ini` na:

```text
115200 baud
```

## Czyszczenie projektu

```bash
pio run --target clean
```

## GitHub Actions

Plik `.github/workflows/platformio.yml` uruchamia automatyczny build projektu na GitHubie.

Workflow wykonuje:

1. checkout repozytorium,
2. instalację Pythona,
3. instalację PlatformIO,
4. `pio run`.

Nie wgrywa firmware do żadnego urządzenia.
