# Testy przed uruchomieniem

Testy należy wykonywać najpierw bez podłączonych pomp 230 V, obserwując LED-y modułów przekaźnikowych lub mierząc ich wyjścia sterujące.

## 1. Start sterownika

- oba wyjścia pomp powinny wystartować w stanie OFF,
- OLED powinien się uruchomić albo sterownik powinien kontynuować pracę bez OLED,
- urządzenie powinno wejść do SUPLA.

## 2. BUF_OK

Przy Alarm = OFF:

1. ustaw BUF_OK = OFF,
2. aktywuj Grzejniki, Podłogówkę i CWU,
3. obie pompy muszą pozostać OFF,
4. spróbuj `MAN ON`,
5. spróbuj SUPLA ON,
6. obie pompy nadal muszą pozostać OFF.

Następnie ustaw BUF_OK = ON i sprawdź powrót normalnej logiki.

## 3. Alarm

1. ustaw BUF_OK = OFF,
2. aktywuj Alarm,
3. obie pompy muszą przejść na ON,
4. ustaw oba tryby lokalne na MAN OFF,
5. obie pompy nadal muszą pozostać ON,
6. wyłącz Alarm,
7. przy BUF_OK = OFF obie pompy muszą przejść na OFF.

## 4. Automatyka

Przy Alarm = OFF, BUF_OK = ON, ręczne = AUTO, SUPLA OFF:

- Grzejniki -> obie pompy ON,
- Podłogówka -> tylko zasilająca ON,
- CWU -> tylko zasilająca ON,
- brak sygnałów -> obie OFF.

## 5. Sterowanie lokalne

Dla każdej pompy sprawdź cykl krótkich naciśnięć:

```text
AUTO -> MAN ON -> MAN OFF -> AUTO
```

Sprawdź także długie przytrzymanie dowolnego przycisku pompy:

```text
DOMOWA = MAN ON
ZASILAJĄCA = MAN ON
```

## 6. SUPLA

Przy lokalnym AUTO:

- SUPLA ON powinno wymusić ON,
- SUPLA OFF powinno zwrócić sterowanie automatyce.

Przy lokalnym MAN OFF SUPLA ON nie może uruchomić pompy.

## 7. Uszkodzenie przewodu BUF_OK

Jeżeli BUF_OK jest wykonane jako styk NO zwierający wejście do GND, odłączenie przewodu powinno spowodować:

```text
BUF_OK = OFF
pompy = OFF
```

z wyjątkiem aktywnego Alarmu.

## 8. Uszkodzenie przewodu Alarm

Dla zalecanego obwodu NC przerwanie przewodu powinno być interpretowane jako Alarm i uruchomić obie pompy.
