# Progress — Seminární práce PIA 1

Mezipaměť stavu projektu: co je hotové a co ještě zbývá udělat.

- **Poslední aktualizace:** 2026-11-17
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
| 2 | Kompletní pipeline (kroky 1–6) | `main.cpp` | načtení → grayscale → práh → morfologie → findContours → klasifikace → výstup |
| 3 | Oprava pádu na `cv::imshow` | `main.cpp` | headless-safe |
| 4 | Práh z histogramu (nahrazuje Otsu) | `main.cpp` | dva dominantní peaky → práh = údolí mezi nimi |
| 5 | Kruhová pracovní oblast uprostřed | `main.cpp` | práh + detekce jen uvnitř kruhu, procento vůči π·r², modrá = kruh |
| 6 | Oddělení slepených buněk (watershed) | `main.cpp` | distanceTransform → jádra (markery) → cv::watershed |
| 7 | Doplnění neúplných buněk (konvexní obal) | `main.cpp` | `cv::convexHull` doplní světlejší část buňky na plný tvar |
| 8 | Volnější kruhovitost (0,40) + větší kruh (0,50) | `main.cpp` | tolerance pro oválné buňky, kruh vepsaný do snímku |
| 9 | Doplnění popisu kroků v README | `README.md` | histogram, kruh, watershed, klasifikace, výstup |
| 10 | Test na všech vzorcích | `vzorky/` | 10 snímků, výsledky níže |

## Bude ještě udělat

| # | Úkol | Priorita | Soubor | Poznámka |
|---|------|----------|--------|----------|
| 1 | Omezit slučování shluků konvexním obalem | vysoká | `main.cpp` | konvexní obal „splácne“ i 2–7 slepených buněk do jednoho velkého kruhu (plocha > 7000 px = shluk) |
| 2 | Doladit parametry | střední | `main.cpp` | `polomerKruhu`, `prahJader`, `minKruhovitost` |
| 3 | Závěrečná zpráva práce | střední | `README.md` | formální text: úvod, teorie, postup, výsledky, závěr |
| 4 | Ověřit robustnost na dalších vzorcích | nízká | — | porovnat s ručním počtem (ground truth) |

## Výsledek (na `vzorek.jpg`)

- Pracovní kruh: poloměr **512 px** (50 % kratší strany)
- Práh (z histogramu, uvnitř kruhu): **144** (peaky 33 a 231)
- Počet buněk: **87**
- Plocha buněk: **424 591 px → 51,6 %** (vůči ploše kruhu)
- Plocha media: **398 959 px → 48,4 %**

Přehled všech vzorků (procento = vůči ploše kruhu):

| Vzorek | Práh | Buněk | % buněk |
|--------|------|-------|---------|
| vzorek.jpg | 144 | 87 | 51,6 % |
| vzorek2 | 141 | 81 | 47,6 % |
| vzorek3 | 191 | 58 | 15,6 % |
| vzorek4 | 146 | 58 | 30,0 % |
| vzorek5 | 151 | 43 | 21,0 % |
| vzorek6 | 155 | 90 | 26,6 % |
| vzorek7 | 148 | 51 | 27,3 % |
| vzorek8 | 152 | 52 | 26,8 % |
| vzorek9 | 169 | 49 | 25,1 % |
| vzorek10 | 154 | 53 | 25,4 % |

## Sestavení a spuštění

```bash
g++ main.cpp -o semestralni_prace $(pkg-config --cflags --libs opencv4) -std=c++17
./semestralni_prace vzorek.jpg
```

Výstupy se ukládají do složky `vystup/` (maska, oddělená maska, výsledek, `souhrn.csv`).
