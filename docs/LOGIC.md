# Logika sterowania

## Priorytety

Sterownik używa czterech poziomów priorytetu.

### 1. Alarm

Jeżeli `ALARM = 1`:

```text
Pompa domowa      = ON
Pompa zasilająca  = ON
```

Alarm ignoruje stan `BUF_OK` oraz wszystkie polecenia ręczne, SUPLA i automatyki.

### 2. BUF_OK

Jeżeli Alarm jest nieaktywny i `BUF_OK = 0`:

```text
Pompa domowa      = OFF
Pompa zasilająca  = OFF
```

Sterowanie ręczne i SUPLA nie mogą ominąć tej blokady.

### 3. Sterowanie ręczne i SUPLA

Dla każdej pompy lokalny tryb ręczny ma trzy stany:

```text
AUTO
MAN ON
MAN OFF
```

Priorytet lokalny dla danej pompy:

```text
MAN ON  -> ON
MAN OFF -> OFF
AUTO    -> sprawdź SUPLA i automatykę
```

SUPLA działa jako zdalne **wymuszenie ON**:

```text
SUPLA ON  -> ON
SUPLA OFF -> brak wymuszenia, przejdź do automatyki
```

Lokalne `MAN OFF` ma pierwszeństwo przed `SUPLA ON`.

### 4. Automatyka

#### Pompa domowa

```text
Grzejniki = 1 -> ON
inaczej       -> OFF
```

#### Pompa zasilająca

```text
Grzejniki = 1 LUB Podłogówka = 1 LUB CWU = 1 -> ON
inaczej                                          -> OFF
```

## Pseudokod

```text
if ALARM:
    DOMOWA = ON
    ZASILAJĄCA = ON

else if not BUF_OK:
    DOMOWA = OFF
    ZASILAJĄCA = OFF

else:
    DOMOWA = resolve(
        local_manual_home,
        supla_home_force_on,
        GRZEJNIKI
    )

    ZASILAJĄCA = resolve(
        local_manual_supply,
        supla_supply_force_on,
        GRZEJNIKI or PODŁOGÓWKA or CWU
    )
```

## Tabela automatyki

Przy założeniu:

- Alarm = OFF,
- BUF_OK = ON,
- obie pompy w AUTO,
- SUPLA OFF.

| Grzejniki | Podłogówka | CWU | Domowa | Zasilająca |
|---:|---:|---:|---:|---:|
| 0 | 0 | 0 | OFF | OFF |
| 1 | 0 | 0 | ON | ON |
| 0 | 1 | 0 | OFF | ON |
| 0 | 0 | 1 | OFF | ON |
| 0 | 1 | 1 | OFF | ON |
| 1 | 1 | 0 | ON | ON |
| 1 | 0 | 1 | ON | ON |
| 1 | 1 | 1 | ON | ON |
