===================================================================
SEMESTRÁLNÍ PRÁCE: PIA 1
Aplikace: Výpočet obsahu externích částic v mediu (průřez cévy - erytrocyty v krvi) 
Téma: Rozpoznávání buněk a media pomocí OpenCV
Jazyk: C++
===================================================================

1. ZÁKLADNÍ POJMY A KNHOVNA
---------------------------
* OpenCV (Open Source Computer Vision Library): 
  Otevřená C++ knihovna určená pro počítačové vidění a zpracování obrazu.
* Pixel (obrazový bod):
  Základní jednotka digitálního obrazu s přiřazenou číselnou hodnotou barvy.
  Pro představu: 1 pixel na Full HD monitoru měří přibližně 0,27 mm.
* Barevný model RGB / BGR:
  Způsob reprezentace barev pomocí tří složek: Red (červená), Green (zelená), 
  Blue (modrá). Každý pixel obsahuje kombinaci těchto tří barevných kanálů.


2. PRINCIP A POSTUP
--------------------------------------

1) Načtení vstupu
   - Načtení obrazku ze souboru do datové struktury OpenCV (cv::Mat).
	- Obrázek průřezu cévy, který obsahuje buňky erytrocytů a zbytek media
	  - následně počítáme obsahy jednotlivých částí.

2) Odbarvení (Převod do Grayscale)
   - Převod barevného RGB obrazu na jednokanálový obraz v odstínech šedi 
     pomocí funkce cv::cvtColor.
   - Význam: Výrazná redukce datového objemu (z 3 barevných kanálů na 1) 
     a snížení výpočetní náročnosti při dalším zpracování.

2b) Omezení na kruhovou pracovní oblast
   - Zpracovává se jen kruh uprostřed snímku (střed snímku, poloměr =
     50 % kratší strany), kde je pozadí rovnoměrné a nejlépe osvětlené.
   - Venku je maska černá (pozadí), hranice kruhu se kreslí modře.

3) Stanovení prahu z histogramu
   - Šedotónový obraz se nejprve rozostří (cv::GaussianBlur 5×5), aby se
     potlačil drobný šum.
   - Z rozostřeného obrazu se spočítá histogram intenzit (256 binů)
     jen z vnitřku kruhu, a vyhladí se klouzavým průměrem.
   - Najdou se dva dominantní peaky: tmavé buňky a světlé pozadí.
   - Práh se určí jako údolí (minimum) mezi těmito dvěma peaky.
   - Prahováním (cv::threshold, THRESH_BINARY_INV) vznikne binární maska:
     buňky (tmavé) → bílá, pozadí (světlé) → černá.
   - Morfologické operace (OPEN + CLOSE) odstraní drobný šum a zaplní dírky.

3b) Oddělení slepených buněk (watershed)
   - Dotýkající se buňky by splynuly do jednoho obrysu a nezapočítaly se.
   - Vzdálenostní transformace (cv::distanceTransform) najde středy buněk,
     které poslouží jako markery pro cv::watershed.
   - Watershed rozdělí slepené buňky čarou v místě jejich dotyku.

4) Detekce obrysů
   - Program prohledá hranice mezi černou a bílou plochou a najde 
     souvislé křivky jednotlivých objektů pomocí cv::findContours.

5) Doplnění a klasifikace buněk
   - Každý obrys se doplní konvexním obalem (cv::convexHull): buňka,
     která je „skoro kruh, ale část je světlejší“, se dopočítá na plný tvar.
   - Z doplněného tvaru se spočítá plocha (cv::contourArea), obvod
     (cv::arcLength) a kruhovitost.
   - Obrys se považuje za buňku, pokud jeho plocha leží v povoleném
     rozmezí a kruhovitost je dostatečně vysoká (erytrocyty jsou kulaté).
   - Součet ploch všech buněk = celkový obsah buněk.

6) Výstup (procentuální obsazení jednotlivých částí)
   - Spočítá se podíl plochy buněk vůči ploše kruhové pracovní oblasti
     (cévy, π·r²) v %.
   - Výsledek se uloží jako obrázek s vyznačenými buňkami (zeleně jako
     doplněné konvexní obaly, červeně nezařazené obrysy), CSV souhrn
     (plocha, obvod, kruhovitost, typ) a vypíše se do konzole.
