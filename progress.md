# Progress — Seminární práce PIA 1

Mezipaměť stavu projektu: co je hotové a co ještě zbývá udělat.

- **Poslední aktualizace:** 2026-11-02
- **Stav:** rozpracováno

## Projekt

| | |
|---|---|
| **Téma** | Rozpoznávání buněk a media pomocí OpenCV |
| **Aplikace** | Výpočet obsahu erytrocytů v průřezu cévy |
| **Jazyk** | C++ (OpenCV) |
| **Repo** | https://github.com/Hancekxxl/Seminarni_prace |
| **Vzorový obrázek** | `vzorek.jpg` |

## Hotovo

| # | Úkol | Soubor | Poznámka |
|---|------|--------|----------|
| 1 | Stažení vzorového obrázku z GitHubu | `vzorek.jpg` | 1024×1024, šedotónový snímek erytrocytů |
| 2 | Kompletní pipeline (kroky 1–6) | `main.cpp` | načtení → grayscale → blur + Otsu + morfologie → findContours → klasifikace + plochy → výstup |
| 3 | Oprava pádu na `cv::imshow` | `main.cpp` | headless-safe (zobrazí se jen při DISPLAY/WAYLAND_DISPLAY) |
| 4 | Otestování na `vzorek.jpg` | `vystup/` | 77 buněk; výstupy jako PNG a CSV |

## Bude ještě udělat

| # | Úkol | Priorita | Soubor | Poznámka |
|---|------|----------|--------|----------|
| 1 | Doplnit popis kroků 5 a 6 | vysoká | `README.md` | kroky 5 (obsah obrysů) a 6 (procentuální výstup) jsou prázdné |
| 2 | Doladit klasifikaci buněk | střední | `main.cpp` | slepené buňky (nízká kruhovitost) se řadí jako „ostatni“ |
| 3 | Závěrečná zpráva práce | střední | `README.md` | formální text: úvod, teorie, postup, výsledky, závěr |
| 4 | Test na dalších vzorcích | nízká | — | ověřit robustnost prahování a parametrů |

## Výsledek (na `vzorek.jpg`)

- Počet buněk: **77**
- Plocha buněk: **311 427 px → 29,7 %**
- Plocha media: **737 149 px → 70,3 %**
- Použitý práh (Otsu): **124**

## Sestavení a spuštění

```bash
g++ main.cpp -o semestralni_prace $(pkg-config --cflags --libs opencv4) -std=c++17
./semestralni_prace vzorek.jpg
```

Výstupy se ukládají do složky `vystup/` (masky, výsledek, `souhrn.csv`).
