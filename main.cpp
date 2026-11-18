// =====================================================================
// SEMESTRÁLNÍ PRÁCE PIA 1
// Rozpoznávání erytrocytů v průřezu cévy pomocí OpenCV
//
// Celá pipeline:
//   1) načtení obrazu
//   2) převod do odstínů šedi
//   2b) omezení na kruhovou pracovní oblast uprostřed snímku
//   3) rozostření + práh z histogramu (údolí mezi peaky) + morfologické čištění
//   3b) oddělení slepených buněk (distance transform + watershed)
//   4) detekce obrysů (findContours)
//   5) klasifikace buněk a výpočet ploch
//   6) výstup: obrázek s popisky + CSV + souhrn do konzole
//
// Překlad:
//   g++ main.cpp -o semestralni_prace \
//       $(pkg-config --cflags --libs opencv4) -std=c++17
//
// Spuštění:
//   ./semestralni_prace vzorek.jpg
// =====================================================================

#include <opencv2/opencv.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
#include "Parametry.h"
// // =====================================================================
// // LADITELNÉ PARAMETRY - upravuj podle konkrétního snímku
// =====================================================================
// struct Parametry {
//    // ---- Prahování -------------------------------------------------
//     // Práh se určuje automaticky z histogramu (údolí mezi peaky buněk a
//     // pozadí). Ruční práh slouží jen pro ladění (pouzitPevnyPrah).
//     bool   pouzitPevnyPrah       = false;  // true = pevný práh místo histogramu
//     double prahPevny             = 127.0;  // hodnota pro pouzitPevnyPrah
//     int    velikostOknaHistogramu = 15;    // vyhlazení histogramu (liché okno)
//     double minProminencePeaku    = 0.01;   // min. výška peaku jako podíl maxima
//     int    minOdstupPeaku        = 40;     // min. vzdálenost peaků v binech

//     // ---- Pracovní oblast (kruh uprostřed) --------------------------
//     // Zpracovává se jen kruh uprostřed snímku, kde je pozadí rovnoměrné.
//     // Hodnota = poloměr jako podíl kratší strany (0.50 -> průměr 100 %).
//     double polomerKruhu = 0.50;

//     // ---- Morfologie ------------------------------------------------
//     int velikostJadra = 3;          // velikost strukturního elementu (liché)

//     // ---- Oddělení slepených buněk (watershed) ----------------------
//     bool   pouzitWatershed = true;  // rozdělí dotýkající se buňky
//     double prahJader       = 8.0;   // práh vzdálenosti od pozadí (px) pro jádra

//     // ---- Detekce buněk (hodnoty v pixelech) ------------------------
//     double minPlochaBunky   = 150.0;    // menší obrysy = šum, ignorovat
//     double maxPlochaBunky   = 50000.0;  // větší obrysy = stěna cévy / okraj kruhu
//     double minKruhovitost   = 0.40;     // tolerance pro lehce oválné buňky
// };

// =====================================================================
// Datové struktury
// =====================================================================
struct ObrysInfo {
    int    index        = -1;    // index ve vektoru kontur
    std::vector<cv::Point> hull; // konvexní obal (doplněná buňka)
    double plocha       = 0.0;   // px^2
    double obvod        = 0.0;   // px
    double kruhovitost  = 0.0;   // 1.0 = ideální kruh
    bool   jeBunka      = false;
    cv::Rect  obal;
    cv::Point stred;
};

// ---------------------------------------------------------------------
// KROK 1: Načtení vstupního obrázku
// ---------------------------------------------------------------------
bool nactiObrazek(const std::string& cesta, cv::Mat& obrazek) {
    if (!fs::exists(cesta)) {
        std::cerr << "Chyba: soubor '" << cesta << "' neexistuje!\n";
        return false;
    }

    obrazek = cv::imread(cesta, cv::IMREAD_COLOR);
    if (obrazek.empty()) {
        std::cerr << "Chyba: obrazek '" << cesta << "' se nepodarilo nacist!\n";
        return false;
    }

    std::cout << "Nacteno: " << cesta << " (" << obrazek.cols << " x "
              << obrazek.rows << ", " << obrazek.channels() << " kanaly)\n";
    return true;
}

// Pokud uzivatel nezada cestu, zkusime najit nejcastejsi nazvy vzorku
std::string najdiVychoziObrazek() {
    const std::vector<std::string> kandidati = {
        "vzorek.jpg", "vzorek.png", "vzorek.jpeg",
        "prurez.jpg", "prurez.png",
        "ceva.jpg", "ceva.png"
    };

    for (const auto& kandidat : kandidati) {
        if (fs::exists(kandidat)) {
            return kandidat;
        }
    }
    return "vzorek.jpg"; // vychozi nazev pro chybovou zpravu
}

// ---------------------------------------------------------------------
// KROK 2: Převod BGR -> odstíny šedi
// ---------------------------------------------------------------------
cv::Mat naOdstinySedi(const cv::Mat& bgr) {
    cv::Mat sedy;
    cv::cvtColor(bgr, sedy, cv::COLOR_BGR2GRAY);
    return sedy;
}

// ---------------------------------------------------------------------
// KROK 2b: Maska kruhové pracovní oblasti uprostřed snímku
// ---------------------------------------------------------------------
cv::Mat vytvorMaskuKruhu(const cv::Mat& obrazek, const Parametry& p,
                         cv::Point& stred, int& polomer) {
    stred = cv::Point(obrazek.cols / 2, obrazek.rows / 2);
    polomer = static_cast<int>(p.polomerKruhu * std::min(obrazek.cols, obrazek.rows));

    cv::Mat maska(obrazek.size(), CV_8U, cv::Scalar(0));
    cv::circle(maska, stred, polomer, cv::Scalar(255), cv::FILLED);
    return maska;
}

// ---------------------------------------------------------------------
// KROK 3a: Práh z histogramu (údolí mezi peaky buněk a pozadí)
// ---------------------------------------------------------------------
// Spočítá histogram intenzit, vyhladí ho klouzavým průměrem, najde dva
// dominantní peaky (pozadí = světlé, buňky = tmavé) a jako práh vrátí
// minimum (údolí) mezi nimi.
double najdiPrahZHistogramu(const cv::Mat& sedy, const cv::Mat& maska,
                            const Parametry& p,
                            int& peakPozadi, int& peakBunek) {
    peakPozadi = -1;
    peakBunek  = -1;

    // 1) histogram intenzit (256 binů) jen z pracovní oblasti (maska)
    cv::Mat hist;
    int kanaly[]   = {0};
    int velikost[] = {256};
    float rozsah[] = {0.0f, 256.0f};
    const float* rozsahy[] = {rozsah};
    cv::calcHist(&sedy, 1, kanaly, maska, hist, 1, velikost, rozsahy,
                 true, false);

    std::vector<double> h(256), hl(256);
    for (int i = 0; i < 256; ++i) h[i] = hist.at<float>(i);

    // 2) vyhlazení klouzavým průměrem (potlačí šum a falešné extrémy)
    int okno = p.velikostOknaHistogramu;
    if (okno < 3) okno = 3;
    if (okno % 2 == 0) ++okno;
    const int pol = okno / 2;

    for (int i = 0; i < 256; ++i) {
        double suma = 0.0;
        int pocet = 0;
        for (int j = i - pol; j <= i + pol; ++j) {
            if (j >= 0 && j < 256) { suma += h[j]; ++pocet; }
        }
        hl[i] = suma / pocet;
    }

    // 3) lokální maxima s minimální prominencí
    const double maxCetnost = *std::max_element(hl.begin(), hl.end());
    const double minVyska   = p.minProminencePeaku * maxCetnost;

    std::vector<int> maxima;
    for (int i = 1; i < 255; ++i) {
        if (hl[i] > hl[i - 1] && hl[i] >= hl[i + 1] && hl[i] >= minVyska) {
            maxima.push_back(i);
        }
    }

    if (maxima.empty()) {
        return cv::mean(sedy)[0];  // fallback: průměrná intenzita
    }

    // 4) peak pozadí = nejvyšší maximum
    const int p1 = *std::max_element(maxima.begin(), maxima.end(),
                                     [&hl](int a, int b) {
                                         return hl[a] < hl[b];
                                     });
    peakPozadi = p1;

    // 5) peak buněk = nejvyšší maximum dostatečně vzdálené od peaku pozadí
    int p2 = -1;
    for (int m : maxima) {
        if (std::abs(m - p1) >= p.minOdstupPeaku && (p2 == -1 || hl[m] > hl[p2])) {
            p2 = m;
        }
    }

    if (p2 == -1) {
        // histogram není zřetelně bimodální → fallback na průměrnou intenzitu
        return cv::mean(sedy)[0];
    }
    peakBunek = p2;

    // 6) práh = minimum (údolí) mezi oběma peaky
    const int a = std::min(p1, p2);
    const int b = std::max(p1, p2);
    int valley = a;
    for (int i = a; i <= b; ++i) {
        if (hl[i] < hl[valley]) valley = i;
    }
    return static_cast<double>(valley);
}

// ---------------------------------------------------------------------
// KROK 3: Rozostření + prahování
// ---------------------------------------------------------------------
cv::Mat vytvorMasku(const cv::Mat& sedy, const cv::Mat& kruhMaska, const Parametry& p,
                    double& pouzityPrah, int& peak1, int& peak2) {
    cv::Mat rozostreny;
    // Liché jádro 5x5 potlaci drobny sum v obrazu
    cv::GaussianBlur(sedy, rozostreny, cv::Size(5, 5), 0);

    if (p.pouzitPevnyPrah) {
        pouzityPrah = p.prahPevny;
        peak1 = peak2 = -1;
    } else {
        pouzityPrah = najdiPrahZHistogramu(rozostreny, kruhMaska, p, peak1, peak2);
    }

    // THRESH_BINARY_INV => tmave objekty (bunky) bile na cernem pozadi
    cv::Mat maska;
    cv::threshold(rozostreny, maska, pouzityPrah, 255, cv::THRESH_BINARY_INV);

    // Vně kruhu = pozadí (černá); zpracovává se jen pracovní oblast.
    // (Nelze zčernit venek před prahováním: THRESH_BINARY_INV by z černé
    //  udělal bílé popředí.)
    cv::bitwise_and(maska, kruhMaska, maska);
    return maska;
}

// ---------------------------------------------------------------------
// KROK 3b: Morfologické čištění masky
// ---------------------------------------------------------------------
cv::Mat vycistiMasku(const cv::Mat& maska, const Parametry& p) {
    cv::Mat cistena;
    cv::Mat jadro = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                              cv::Size(p.velikostJadra, p.velikostJadra));

    // OPEN smaze drobne bile tecky (sum), CLOSE zaplni male dirky
    cv::morphologyEx(maska, cistena, cv::MORPH_OPEN, jadro);
    cv::morphologyEx(cistena, cistena, cv::MORPH_CLOSE, jadro);
    return cistena;
}

// ---------------------------------------------------------------------
// KROK 3c: Oddělení slepených buněk (distance transform + watershed)
// ---------------------------------------------------------------------
// Dotýkající se buňky by jinak splynuly do jednoho obrysu s nízkou
// kruhovitostí a nebyly by započítány. Vzdálenostní transformace najde
// jádra jednotlivých buněk (lokální maxima vzdálenosti od pozadí), ty
// poslouží jako markery pro watershed, který buňky rozdělí v místě styku.
cv::Mat oddelSlepeneBunky(const cv::Mat& cistaMaska, const cv::Mat& sedy,
                          const Parametry& p) {
    if (!p.pouzitWatershed || cv::countNonZero(cistaMaska) == 0) {
        return cistaMaska;
    }

    // 1) vzdálenost každého pixelu popředí od pozadí (v pixelech)
    cv::Mat dist;
    cv::distanceTransform(cistaMaska, dist, cv::DIST_L2, 5);

    // Lehce vyhladíme, aby se omezila nadměrná segmentace (falešná jádra)
    cv::Mat distHl;
    cv::GaussianBlur(dist, distHl, cv::Size(0, 0), 1.0);

    // 2) jádra buněk = pixely dostatečně daleko od pozadí (středy buněk).
    //    Absolutní práh v pixelech (zhruba polovina poloměru buňky).
    //    Nelze normalizovat podle globálního maxima: velké slepené shluky
    //    by ho natáhly a jádra malých buněk by zanikla.
    cv::Mat jadra32;
    cv::threshold(distHl, jadra32, p.prahJader, 255, cv::THRESH_BINARY);
    cv::Mat jadra;
    jadra32.convertTo(jadra, CV_8U);  // 0/255

    if (cv::countNonZero(jadra) == 0) {
        return cistaMaska;  // žádná jádra → nic nerozdělujeme
    }

    // 3) sure background = dilatovaná maska (okolí popředí)
    cv::Mat jadro = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(3, 3));
    cv::Mat sureBg;
    cv::dilate(cistaMaska, sureBg, jadro);

    // 4) unknown = sureBg - jádra (těla buněk bez jader)
    cv::Mat unknown;
    cv::subtract(sureBg, jadra, unknown);

    // 5) markery: jádra očíslovaná 2..N+1, pozadí 1, neznámé 0
    cv::Mat markery;
    cv::connectedComponents(jadra, markery);
    markery += 1;
    markery.setTo(0, unknown == 255);

    // 6) watershed na šedotónovém obraze (standardní postup)
    cv::Mat bgr;
    cv::cvtColor(sedy, bgr, cv::COLOR_GRAY2BGR);
    cv::watershed(bgr, markery);

    // 7) oddělená maska: buňky = oblasti s labelem > 1, hranice = -1
    return (markery > 1);
}

// ---------------------------------------------------------------------
// KROK 4+5: Klasifikace obrysů na buňky a ostatní
// ---------------------------------------------------------------------
void analyzujKontury(const std::vector<std::vector<cv::Point>>& kontury,
                     const Parametry& p,
                     std::vector<ObrysInfo>& vystup) {
    vystup.clear();

    for (size_t i = 0; i < kontury.size(); ++i) {
        ObrysInfo info;
        info.index  = static_cast<int>(i);

        // Konvexní obal doplní neúplnou buňku (světlejší část / dírka
        // uprostřed) na plný tvar, takže se započítá její reálná plocha.
        cv::convexHull(kontury[i], info.hull);
        info.plocha = cv::contourArea(info.hull);
        info.obvod  = cv::arcLength(info.hull, true);

        // Příliš malé = šum; příliš velké = stěna cévy / okraj kruhu
        if (info.plocha < p.minPlochaBunky || info.plocha > p.maxPlochaBunky) {
            continue;
        }

        // Kruhovitost: 1.0 = ideální kruh, blíží se 0 = protáhlý útvar
        info.kruhovitost = (info.obvod > 0.0)
            ? (4.0 * CV_PI * info.plocha) / (info.obvod * info.obvod)
            : 0.0;

        info.obal  = cv::boundingRect(info.hull);
        info.stred = cv::Point(info.obal.x + info.obal.width / 2,
                               info.obal.y + info.obal.height / 2);

        // Buňka = dostatečně kulatý tvar (velikost prošla filtrem výše)
        info.jeBunka = (info.kruhovitost >= p.minKruhovitost);

        vystup.push_back(info);
    }
}

// ---------------------------------------------------------------------
// KROK 6: Vykreslení výsledku do obrázku
// ---------------------------------------------------------------------
void vykresliVysledek(const cv::Mat& original,
                      const std::vector<std::vector<cv::Point>>& kontury,
                      const std::vector<ObrysInfo>& infos,
                      const cv::Point& stredKruhu,
                      int polomerKruhu,
                      double plochaCeva,
                      double plochaBunek,
                      double plochaMedia,
                      cv::Mat& vysledek) {
    vysledek = original.clone();

    const cv::Scalar barvaCeva  (255,   0,   0); // modrá
    const cv::Scalar barvaBunka (  0, 255,   0); // zelená
    const cv::Scalar barvaOstatni( 0,   0, 255); // červená

    // Hranice pracovní oblasti (cévy)
    cv::circle(vysledek, stredKruhu, polomerKruhu, barvaCeva, 2);

    // Jednotlivé obrysy
    for (const auto& info : infos) {
        if (info.jeBunka) {
            // Buňka vykreslená jako doplněný konvexní obal (plný tvar)
            cv::polylines(vysledek, info.hull, true, barvaBunka, 2);
            cv::putText(vysledek, std::to_string(info.index), info.stred,
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, barvaBunka, 1);
        } else {
            // Nezařazené (slepené / nepravidelné) = tenký červený obrys
            cv::drawContours(vysledek, kontury, info.index, barvaOstatni, 2);
        }
    }

    // Souhrnný text
    const double pctBunky = (plochaCeva > 0.0) ? (plochaBunek / plochaCeva) * 100.0 : 0.0;
    const double pctMedia = (plochaCeva > 0.0) ? (plochaMedia / plochaCeva) * 100.0 : 0.0;

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1)
        << "Bunky: " << pctBunky << " %   |   Medium: " << pctMedia << " %";
    cv::putText(vysledek, oss.str(), cv::Point(10, 25),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
}

// ---------------------------------------------------------------------
// Uložení obrázku do složky s výstupy
// ---------------------------------------------------------------------
void ulozVystup(const fs::path& slozka, const std::string& nazev,
                const cv::Mat& obrazek) {
    fs::create_directories(slozka);
    const fs::path cesta = slozka / nazev;

    if (!cv::imwrite(cesta.string(), obrazek)) {
        std::cerr << "Varovani: nepodarilo se ulozit '" << cesta << "'\n";
    }
}

// ---------------------------------------------------------------------
// Uložení CSV souhrnu
// ---------------------------------------------------------------------
void zapisSouhrn(const fs::path& slozka,
                 const std::vector<ObrysInfo>& infos,
                 double plochaCeva, double plochaBunek, double plochaMedia) {
    fs::create_directories(slozka);
    const fs::path cesta = slozka / "souhrn.csv";

    std::ofstream csv(cesta);
    if (!csv.is_open()) {
        std::cerr << "Varovani: nepodarilo se otevrit '" << cesta << "'\n";
        return;
    }

    csv << std::fixed << std::setprecision(2);
    csv << "id,plocha,obvod,kruhovitost,typ\n";
    for (const auto& info : infos) {
        csv << info.index << ","
            << info.plocha << ","
            << info.obvod << ","
            << info.kruhovitost << ","
            << (info.jeBunka ? "bunka" : "ostatni") << "\n";
    }

    csv << "\nplocha_ceva," << plochaCeva << "\n";
    csv << "plocha_bunek," << plochaBunek << "\n";
    csv << "plocha_media," << plochaMedia << "\n";
    csv.close();

    std::cout << "CSV souhrn ulozen do '" << cesta << "'\n";
}

// ---------------------------------------------------------------------
// HLAVNÍ PROGRAM
// ---------------------------------------------------------------------
int main(int argc, char** argv) {
    const std::string cesta = (argc > 1) ? argv[1] : najdiVychoziObrazek();
    const fs::path vystupniSlozka = "vystup";
    const Parametry p; // laditelné konstanty na začátku souboru

    // KROK 1
    cv::Mat obrazek;
    if (!nactiObrazek(cesta, obrazek)) {
        return 1;
    }

    // KROK 2
    cv::Mat sedy = naOdstinySedi(obrazek);

    // Pracovní oblast: kruh uprostřed snímku
    cv::Point stredKruhu;
    int polomerKruhu = 0;
    cv::Mat kruhMaska = vytvorMaskuKruhu(obrazek, p, stredKruhu, polomerKruhu);

    // KROK 3 + 3b + 3c
    double prah = 0.0;
    int peak1 = -1, peak2 = -1;
    cv::Mat maska      = vytvorMasku(sedy, kruhMaska, p, prah, peak1, peak2);
    cv::Mat cistaMaska = vycistiMasku(maska, p);
    cv::Mat oddelena   = oddelSlepeneBunky(cistaMaska, sedy, p);

    std::cout << "Pracovni kruh: stred (" << stredKruhu.x << ", " << stredKruhu.y
              << "), polomer " << polomerKruhu << " px\n";
    if (p.pouzitPevnyPrah) {
        std::cout << "Pouzity pevny prah: " << prah << "\n";
    } else {
        std::cout << "Pouzity prah (z histogramu): " << prah
                  << "  (peaky: " << peak1 << " a " << peak2 << ")\n";
    }

    // KROK 4: hledání obrysů (klon, findContours nesmí upravit zdroj)
    std::vector<std::vector<cv::Point>> kontury;
    std::vector<cv::Vec4i> hierarchie;
    cv::Mat maskaKopie = oddelena.clone();
    cv::findContours(maskaKopie, kontury, hierarchie,
                     cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::cout << "Nalezeno obrysu: " << kontury.size() << "\n";

    if (kontury.empty()) {
        std::cerr << "Zadne obrysy nenalezeny - zkontroluj prahovani.\n";
        return 1;
    }

    // KROK 5: klasifikace a výpočet ploch
    std::vector<ObrysInfo> infos;
    analyzujKontury(kontury, p, infos);

    double plochaBunek = 0.0;
    int    pocetBunek  = 0;
    for (const auto& info : infos) {
        if (info.jeBunka) {
            plochaBunek += info.plocha;
            ++pocetBunek;
        }
    }

    // Oblast cévy = plocha kruhové pracovní oblasti
    const double plochaCeva = CV_PI * polomerKruhu * polomerKruhu;

    const double plochaMedia = std::max(0.0, plochaCeva - plochaBunek);
    const double pctBunky = (plochaCeva > 0.0) ? (plochaBunek / plochaCeva) * 100.0 : 0.0;
    const double pctMedia = (plochaCeva > 0.0) ? (plochaMedia / plochaCeva) * 100.0 : 0.0;

    // KROK 6: výstup do konzole
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "----------------------------------------\n";
    std::cout << "Obrysu celkem : " << infos.size() << "\n";
    std::cout << "Pocet bunek   : " << pocetBunek << "\n";
    std::cout << "Plocha cevy   : " << plochaCeva << " px\n";
    std::cout << "Plocha bunek  : " << plochaBunek << " px (" << pctBunky << " %)\n";
    std::cout << "Plocha media  : " << plochaMedia << " px (" << pctMedia << " %)\n";
    std::cout << "----------------------------------------\n";

    // Detaily obrysů (pro ladění parametrů)
    std::cout << "\nDetaily obrysu (pro ladeni):\n";
    for (const auto& info : infos) {
        std::cout << "  #" << info.index
                  << " plocha=" << std::setw(8) << info.plocha
                  << " kruhovitost=" << std::setw(5) << info.kruhovitost
                  << " -> " << (info.jeBunka ? "bunka" : "ostatni") << "\n";
    }

    // Vykreslení a uložení
    cv::Mat vysledek;
    vykresliVysledek(obrazek, kontury, infos, stredKruhu, polomerKruhu,
                     plochaCeva, plochaBunek, plochaMedia, vysledek);

    ulozVystup(vystupniSlozka, "01_sedy.png",           sedy);
    ulozVystup(vystupniSlozka, "02_maska.png",          maska);
    ulozVystup(vystupniSlozka, "03_maska_cista.png",    cistaMaska);
    ulozVystup(vystupniSlozka, "04_maska_oddeleno.png", oddelena);
    ulozVystup(vystupniSlozka, "05_vysledek.png",       vysledek);
    zapisSouhrn(vystupniSlozka, infos, plochaCeva, plochaBunek, plochaMedia);

    std::cout << "Vystupy ulozeny do slozky '" << vystupniSlozka << "'.\n";

    // Nahled - zobrazi se jen tehdy, kdyz je k dispozici graficke prostredi.
    // Bez DISPLAY/WAYLAND_DISPLAY by cv::imshow skoncilo chybou (SIGABRT),
    // takze v headless rezimu nahled preskocime.
    const char* xDisplej    = std::getenv("DISPLAY");
    const char* waylandDisp = std::getenv("WAYLAND_DISPLAY");
    const bool  maGui = (xDisplej && *xDisplej) || (waylandDisp && *waylandDisp);

    if (maGui) {
        try {
            cv::imshow("Maska", cistaMaska);
            cv::imshow("Oddeleno", oddelena);
            cv::imshow("Vysledek", vysledek);
            cv::waitKey(0);
        } catch (const cv::Exception& e) {
            std::cerr << "Nahled nelze zobrazit: " << e.what() << "\n";
        }
    } else {
        std::cout << "Bez grafickeho prostredi - nahled preskocen.\n";
    }

    return 0;
}
