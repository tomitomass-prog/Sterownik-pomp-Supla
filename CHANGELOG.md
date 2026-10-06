# Changelog

## 1.1.0 - PlatformIO

- przebudowa repozytorium z pojedynczego szkicu Arduino `.ino` na projekt PlatformIO,
- kod główny przeniesiony do `src/main.cpp`,
- dodany `platformio.ini`,
- zależności bibliotek przeniesione do `lib_deps`,
- dodane katalogi `include/`, `lib/` i `test/`,
- dodana instrukcja `docs/BUILDING.md`,
- dodany GitHub Actions workflow wykonujący `pio run`,
- bez zmian w logice sterowania pomp.

## 1.0.0

- pierwsza wersja repozytorium,
- sterowanie dwiema pompami,
- priorytety Alarm -> BUF_OK -> ręczne/SUPLA -> automatyka,
- wejścia Grzejniki / Podłogówka / CWU / Alarm / BUF_OK,
- trzy przyciski chwilowe,
- OLED SSD1306,
- integracja SUPLA.
