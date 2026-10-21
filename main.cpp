// =====================================================================
// SEMESTRÁLNÍ PRÁCE PIA 1
// Rozpoznávání erytrocytů v průřezu cévy pomocí OpenCV
//
// 1. část: načtení obrazu, převod do odstínů šedi, rozostření,
//          Otsu prahování a morfologické čištění masky
//
// Překlad:
//   g++ main.cpp -o semestralni_prace \
//       $(pkg-config --cflags --libs opencv4) -std=c++17
//
// Spuštění:
//   ./semestralni_prace vzorek.jpg
// =====================================================================

#include <opencv2/opencv.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

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
// KROK 3: Rozostření + Otsu prahování
// ---------------------------------------------------------------------
cv::Mat vytvorMasku(const cv::Mat& sedy, double& pouzityPrah) {
    cv::Mat rozostreny;
    // Liché jádro 5x5 potlaci drobny sum v obrazu
    cv::GaussianBlur(sedy, rozostreny, cv::Size(5, 5), 0);

    cv::Mat maska;
    // THRESH_BINARY_INV => tmave objekty (bunky) budou bile na cernem
    // pozadi. Otsu si prah urci automaticky z histogramu obrazu.
    pouzityPrah = cv::threshold(rozostreny, maska, 0, 255,
                                cv::THRESH_BINARY_INV | cv::THRESH_OTSU);
    return maska;
}

// ---------------------------------------------------------------------
// KROK 3b: Morfologické čištění masky
// ---------------------------------------------------------------------
cv::Mat vycistiMasku(const cv::Mat& maska) {
    cv::Mat cistena;
    cv::Mat jadro = cv::getStructuringElement(cv::MORPH_ELLIPSE,
                                              cv::Size(3, 3));

    // OPEN smaze drobne bile tecky (sum), CLOSE zaplni male dirky
    cv::morphologyEx(maska, cistena, cv::MORPH_OPEN, jadro);
    cv::morphologyEx(cistena, cistena, cv::MORPH_CLOSE, jadro);
    return cistena;
}

// ---------------------------------------------------------------------
// Pomocný přehled: kolik procent obrazu tvoří popředí
// ---------------------------------------------------------------------
double podilPopredi(const cv::Mat& maska) {
    const double bila   = static_cast<double>(cv::countNonZero(maska));
    const double celkem = static_cast<double>(maska.total());
    return (bila / celkem) * 100.0;
}

// ---------------------------------------------------------------------
// Uložení mezikroku do složky s výstupy
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
// HLAVNÍ PROGRAM
// ---------------------------------------------------------------------
int main(int argc, char** argv) {
    // Cesta k obrazku muze byt zadana argumentem, jinak se hleda vzorek
    const std::string cesta = (argc > 1) ? argv[1] : najdiVychoziObrazek();
    const fs::path vystupniSlozka = "vystup";

    cv::Mat obrazek;
    if (!nactiObrazek(cesta, obrazek)) {
        return 1;
    }

    // KROK 2
    cv::Mat sedy = naOdstinySedi(obrazek);

    // KROK 3 + 3b
    double prah = 0.0;
    cv::Mat maska      = vytvorMasku(sedy, prah);
    cv::Mat cistaMaska = vycistiMasku(maska);

    std::cout << "Otsu prah: " << prah << "\n";
    std::cout << "Podil popredi: " << podilPopredi(cistaMaska) << " %\n";

    // Ulozeni mezikroku pro kontrolu a ladeni prahu
    ulozVystup(vystupniSlozka, "01_sedy.png",        sedy);
    ulozVystup(vystupniSlozka, "02_maska.png",       maska);
    ulozVystup(vystupniSlozka, "03_maska_cista.png", cistaMaska);

    std::cout << "Mezikroky ulozeny do slozky '" << vystupniSlozka << "'.\n";

    // Nahled - pokud neni dostupne GUI, program nespadne
    try {
        cv::imshow("Maska", cistaMaska);
        cv::waitKey(0);
    } catch (const cv::Exception& e) {
        std::cerr << "Nahled nelze zobrazit (chybi GUI): " << e.what() << "\n";
    }

    return 0;
}
