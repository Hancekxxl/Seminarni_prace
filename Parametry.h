


// =====================================================================
// LADITELNÉ PARAMETRY - upravuj podle konkrétního snímku
// =====================================================================
struct Parametry {
    // ---- Prahování -------------------------------------------------
    // Práh se určuje automaticky z histogramu (údolí mezi peaky buněk a
    // pozadí). Ruční práh slouží jen pro ladění (pouzitPevnyPrah).
    bool   pouzitPevnyPrah       = false;  // true = pevný práh místo histogramu
    double prahPevny             = 127.0;  // hodnota pro pouzitPevnyPrah
    int    velikostOknaHistogramu = 15;    // vyhlazení histogramu (liché okno)
    double minProminencePeaku    = 0.01;   // min. výška peaku jako podíl maxima
    int    minOdstupPeaku        = 40;     // min. vzdálenost peaků v binech

    // ---- Pracovní oblast (kruh uprostřed) --------------------------
    // Zpracovává se jen kruh uprostřed snímku, kde je pozadí rovnoměrné.
    // Hodnota = poloměr jako podíl kratší strany (0.50 -> průměr 100 %).
    double polomerKruhu = 0.50;

    // ---- Morfologie ------------------------------------------------
    int velikostJadra = 3;          // velikost strukturního elementu (liché)

    // ---- Oddělení slepených buněk (watershed) ----------------------
    bool   pouzitWatershed = true;  // rozdělí dotýkající se buňky
    double prahJader       = 8.0;   // práh vzdálenosti od pozadí (px) pro jádra

    // ---- Detekce buněk (hodnoty v pixelech) ------------------------
    double minPlochaBunky   = 150.0;    // menší obrysy = šum, ignorovat
    double maxPlochaBunky   = 50000.0;  // větší obrysy = stěna cévy / okraj kruhu
    double minKruhovitost   = 0.40;     // tolerance pro lehce oválné buňky
};