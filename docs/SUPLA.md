# SUPLA

## Kanały

| Nr | Kanał | Typ | Znaczenie |
|---:|---|---|---|
| 0 | Pompa domowa – wymuszenie | VirtualRelay | ON = wymuś pompę ON |
| 1 | Pompa zasilająca – wymuszenie | VirtualRelay | ON = wymuś pompę ON |
| 2 | Stan pompy domowej | GPM | 0/1 |
| 3 | Stan pompy zasilającej | GPM | 0/1 |
| 4 | Grzejniki | GPM | 0/1 |
| 5 | Podłogówka | GPM | 0/1 |
| 6 | CWU | GPM | 0/1 |
| 7 | Alarm | GPM | 0/1 |
| 8 | BUF_OK | GPM | 0/1 |

## Zdalne sterowanie

Kanały 0 i 1 są zdalnymi **wymuszeniami ON**.

```text
SUPLA ON  -> żądanie ON
SUPLA OFF -> anulowanie zdalnego żądania
```

`SUPLA OFF` nie jest wymuszeniem pompy OFF. Gdy nie ma zdalnego wymuszenia, decyzję podejmuje lokalny tryb ręczny albo automatyka.

## Priorytety wobec SUPLA

SUPLA działa tylko wtedy, gdy:

```text
Alarm = OFF
BUF_OK = ON
lokalny tryb danej pompy = AUTO
```

Lokalne `MAN ON` i `MAN OFF` mają większy priorytet niż SUPLA.
