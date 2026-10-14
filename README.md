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
	- Obrázek průřezu cévy který obsahuje buňky erytrocitů a zbytku media - následně počítáme obsahy jednotlivých částí

2) Odbarvení (Převod do Grayscale)
   - Převod barevného RGB obrazu na jednokanálový obraz v odstínech šedi 
     pomocí funkce cv::cvtColor.
   - Význam: Výrazná redukce datového objemu (z 3 barevných kanálů na 1) 
     a snížení výpočetní náročnosti při dalším zpracování.

3) Stanovení prahu
   - Prahování: Nastavení prahové hodnoty (cv::threshold nebo cv::Canny).
     Pixely nad práh se převedou na bílou, pod práh na černou. 
     Vzniká čistě kontrastní černobílý obraz (objekty vs. pozadí).

4) Detekce obrysů
   - Program prohledá hranice mezi černou a bílou plochou a najde 
     souvislé křivky jednotlivých objektů pomocí cv::findContours.

5) Výpočet obsahů jednotlivých obrysů buněk
   

6) Výstup (Procentuální obsazení jednotlivých částí)
