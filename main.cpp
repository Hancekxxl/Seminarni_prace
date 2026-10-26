// =====================================================================
// SEMESTRÁLNÍ PRÁCE PIA 1
// Rozpoznávání erytrocytů v průřezu cévy pomocí OpenCV
//
// Celá pipeline:
//   1) načtení obrazu
//   2) převod do odstínů šedi
//   3) rozostření + Otsu prahování + morfologické čištění
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
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// =====================================================================
// LADITELNÉ PARAMETRY - upravuj podle konkrétního snímku
// =====================================================================
struct Parametry {
    // ---- Prahování -------------------------------------------------
    bool   pouzitOtsu = true;       // true = automatický práh (Otsu)
    double prahPevny = 127.0;       // použije se jen když je Otsu vypnuté

    // ---- Morfologie ------------------------------------------------
    int velikostJadra = 3;          // velikost strukturního elementu (liché)

    // ---- Detekce buněk (hodnoty v pixelech) ------------------------
    double minPlochaBunky   = 150.0;    // menší obrysy = šum, ignorovat
    double maxPlochaBunky   = 50000.0;  // větší obrysy = céva / stěna
    double minKruhovitost   = 0.55;     // erytrocyty jsou přibližně kruhové

    // ---- Oblast cévy ------------------------------------------------
    // true  = céva zabírá celý snímek (plocha cévy = plocha obrázku)
    // false = céva se určí jako největší nalezený obrys
    bool   celaPlochaJeCeva = true;
};

// =====================================================================
// Datové struktury
// =====================================================================
struct ObrysInfo {
    int    index        = -1;    // index ve vektoru kontur
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
// KROK 3: Rozostření + prahování
// ---------------------------------------------------------------------
cv::Mat vytvorMasku(const cv::Mat& sedy, const Parametry& p, double& pouzityPrah) {
    cv::Mat rozostreny;
    // Liché jádro 5x5 potlaci drobny sum v obrazu
    cv::GaussianBlur(sedy, rozostreny, cv::Size(5, 5), 0);

    cv::Mat maska;
    if (p.pouzitOtsu) {
        // THRESH_BINARY_INV => tmave objekty (bunky) bile na cernem pozadi
        // Otsu si prah urci automaticky z histogramu
        pouzityPrah = cv::threshold(rozostreny, maska, 0, 255,
                                    cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    } else {
        pouzityPrah = cv::threshold(rozostreny, maska, p.prahPevny, 255,
                                    cv::THRESH_BINARY_INV);
    }
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
// KROK 4+5: Klasifikace obrysů na buňky a ostatní
// ---------------------------------------------------------------------
void analyzujKontury(const std::vector<std::vector<cv::Point>>& kontury,
                     const Parametry& p,
                     std::vector<ObrysInfo>& vystup,
                     int& indexNejvetsiKontury) {
    vystup.clear();
    indexNejvetsiKontury = -1;
    double maxPlocha = -1.0;

    for (size_t i = 0; i < kontury.size(); ++i) {
        ObrysInfo info;
        info.index  = static_cast<int>(i);
        info.plocha = cv::contourArea(kontury[i]);
        info.obvod  = cv::arcLength(kontury[i], true);

        // Zcela drobné obrysy = šum na pozadí
        if (info.plocha < p.minPlochaBunky) {
            continue;
        }

        // Kruhovitost: 1.0 = ideální kruh, blíží se 0 = protáhlý útvar
        info.kruhovitost = (info.obvod > 0.0)
            ? (4.0 * CV_PI * info.plocha) / (info.obvod * info.obvod)
            : 0.0;

        info.obal  = cv::boundingRect(kontury[i]);
        info.stred = cv::Point(info.obal.x + info.obal.width / 2,
                               info.obal.y + info.obal.height / 2);

        // Buňka = správná velikost + dostatečně kulatý tvar
        info.jeBunka = (info.plocha <= p.maxPlochaBunky) &&
                       (info.kruhovitost >= p.minKruhovitost);

        vystup.push_back(info);

        if (info.plocha > maxPlocha) {
            maxPlocha = info.plocha;
            indexNejvetsiKontury = static_cast<int>(i);
        }
    }
}

// ---------------------------------------------------------------------
// KROK 6: Vykreslení výsledku do obrázku
// ---------------------------------------------------------------------
void vykresliVysledek(const cv::Mat& original,
                      const std::vector<std::vector<cv::Point>>& kontury,
                      const std::vector<ObrysInfo>& infos,
                      const Parametry& p,
                      int indexNejvetsiKontury,
                      double plochaCeva,
                      double plochaBunek,
                      double plochaMedia,
                      cv::Mat& vysledek) {
    vysledek = original.clone();

    const cv::Scalar barvaCeva  (255,   0,   0); // modrá
    const cv::Scalar barvaBunka (  0, 255,   0); // zelená
    const cv::Scalar barvaOstatni( 0,   0, 255); // červená

    // Oblast cévy
    if (p.celaPlochaJeCeva) {
        cv::rectangle(vysledek, cv::Point(0, 0),
                      cv::Point(vysledek.cols - 1, vysledek.rows - 1),
                      barvaCeva, 2);
    } else if (indexNejvetsiKontury >= 0) {
        cv::drawContours(vysledek, kontury, indexNejvetsiKontury, barvaCeva, 2);
    }

    // Jednotlivé obrysy
    for (const auto& info : infos) {
        const cv::Scalar barva = info.jeBunka ? barvaBunka : barvaOstatni;
        cv::drawContours(vysledek, kontury, info.index, barva, 2);

        if (info.jeBunka) {
            cv::putText(vysledek, std::to_string(info.index), info.stred,
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, barvaBunka, 1);
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

    // KROK 3 + 3b
    double prah = 0.0;
    cv::Mat maska      = vytvorMasku(sedy, p, prah);
    cv::Mat cistaMaska = vycistiMasku(maska, p);

    std::cout << "Pouzity prah: " << prah << "\n";

    // KROK 4: hledání obrysů (klon, findContours nesmí upravit zdroj)
    std::vector<std::vector<cv::Point>> kontury;
    std::vector<cv::Vec4i> hierarchie;
    cv::Mat maskaKopie = cistaMaska.clone();
    cv::findContours(maskaKopie, kontury, hierarchie,
                     cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    std::cout << "Nalezeno obrysu: " << kontury.size() << "\n";

    if (kontury.empty()) {
        std::cerr << "Zadne obrysy nenalezeny - zkontroluj prahovani.\n";
        return 1;
    }

    // KROK 5: klasifikace a výpočet ploch
    std::vector<ObrysInfo> infos;
    int indexNejvetsiKontury = -1;
    analyzujKontury(kontury, p, infos, indexNejvetsiKontury);

    double plochaBunek = 0.0;
    int    pocetBunek  = 0;
    for (const auto& info : infos) {
        if (info.jeBunka) {
            plochaBunek += info.plocha;
            ++pocetBunek;
        }
    }

    // Oblast cévy
    double plochaCeva = 0.0;
    if (p.celaPlochaJeCeva) {
        plochaCeva = static_cast<double>(obrazek.total());
    } else if (indexNejvetsiKontury >= 0) {
        plochaCeva = cv::contourArea(kontury[indexNejvetsiKontury]);
    } else {
        plochaCeva = static_cast<double>(obrazek.total());
    }

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
    vykresliVysledek(obrazek, kontury, infos, p, indexNejvetsiKontury,
                     plochaCeva, plochaBunek, plochaMedia, vysledek);

    ulozVystup(vystupniSlozka, "01_sedy.png",        sedy);
    ulozVystup(vystupniSlozka, "02_maska.png",       maska);
    ulozVystup(vystupniSlozka, "03_maska_cista.png", cistaMaska);
    ulozVystup(vystupniSlozka, "04_vysledek.png",    vysledek);
    zapisSouhrn(vystupniSlozka, infos, plochaCeva, plochaBunek, plochaMedia);

    std::cout << "Vystupy ulozeny do slozky '" << vystupniSlozka << "'.\n";

    // Nahled - pokud neni dostupne GUI, program nespadne
    try {
        cv::imshow("Maska", cistaMaska);
        cv::imshow("Vysledek", vysledek);
        cv::waitKey(0);
    } catch (const cv::Exception& e) {
        std::cerr << "Nahled nelze zobrazit (chybi GUI): " << e.what() << "\n";
    }

    return 0;
}
