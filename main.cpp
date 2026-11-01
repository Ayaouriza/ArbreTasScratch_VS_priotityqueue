// Comparaison : tas max "from scratch" (sur un vector utilisé comme simple tableau) vs std::priority_queue (STL)
// Compilation : g++ -O2 -std=c++17 tas_comparaison.cpp -o tas
//
// Pour chaque taille n, on mesure (en secondes) :
//   1) l'insertion de n valeurs aléatoires
//   2) l'insertion d'UNE valeur choisie par l'utilisateur (dans le tas déjà rempli)
//   3) l'extraction de la racine (le maximum est lu puis retiré de l'arbre)
//   4) l'ajustement de l'arbre (la propriété du tas est rétablie)
//   5) le total extraction + ajustement
#include <iostream>
#include <vector>
#include <queue>
#include <random>
#include <chrono>
#include <utility>
#include <iomanip>
#include <string>
#include <sstream>
using namespace std;

// ---------------------------------------------------------------
// 1) Tas "from scratch" : arbre complet stocké dans un vector
//    Le vector sert uniquement de zone mémoire (t[i]).
//    On n'utilise AUCUNE de ses fonctions (push_back, pop_back, back, size,
//    empty, swap...) : la taille, l'agrandissement et l'échange sont codés à la main.
// ---------------------------------------------------------------
class TasScratch {
    vector<int> t;      // zone de stockage
    int taille = 0;     // nombre d'éléments réellement dans le tas
    int capacite = 0;   // nombre de cases disponibles dans t

    // échange manuel de deux cases
    void echanger(int i, int j) {
        int tmp = t[i];
        t[i] = t[j];
        t[j] = tmp;
    }

    // quand le tableau est plein, on en crée un deux fois plus grand
    // et on recopie les éléments un par un
    void agrandir() {
        int nouvelle = (capacite == 0) ? 1 : capacite * 2;
        vector<int> nv(nouvelle);
        for (int i = 0; i < taille; i++) nv[i] = t[i];
        t = move(nv);
        capacite = nouvelle;
    }

    void monter(int i) {
        while (i > 0 && t[i] > t[(i - 1) / 2]) {
            echanger(i, (i - 1) / 2);
            i = (i - 1) / 2;
        }
    }

    void descendre(int i) {
        while (true) {
            int g = 2 * i + 1, d = 2 * i + 2, m = i;
            if (g < taille && t[g] > t[m]) m = g;
            if (d < taille && t[d] > t[m]) m = d;
            if (m == i) break;
            echanger(i, m);
            i = m;
        }
    }

public:
    // l'extraction et l'ajustement peuvent être mesurés séparément
    static constexpr bool separable = true;

    void inserer(int x) {
        if (taille == capacite) agrandir();
        t[taille] = x;          // à la place de push_back
        taille++;
        monter(taille - 1);
    }

    // Étape 1 : EXTRACTION de la racine
    // on lit le maximum, puis le dernier élément prend la place de la racine
    // et la taille diminue de 1 (l'arbre reste complet, mais la racine n'est
    // peut-être plus à la bonne place)
    int extraire_racine() {
        int maximum = t[0];
        t[0] = t[taille - 1];
        taille--;
        return maximum;
    }

    // Étape 2 : AJUSTEMENT de l'arbre
    // le nouvel élément de la racine descend jusqu'à sa bonne place
    void ajuster() {
        if (taille > 0) descendre(0);
    }
};

// ---------------------------------------------------------------
// 2) Tas avec le conteneur STL std::priority_queue
//    (tas max par défaut : top() = le plus grand élément)
// ---------------------------------------------------------------
class TasPQ {
    priority_queue<int> pq;

public:
    // pop() retire la racine ET réajuste le tas en une seule opération :
    // on ne peut pas mesurer les deux étapes séparément
    static constexpr bool separable = false;

    void inserer(int x) { pq.push(x); }

    int extraire_et_ajuster() {
        int maximum = pq.top();
        pq.pop();
        return maximum;
    }
};

// ---------------------------------------------------------------
// Benchmark
// ---------------------------------------------------------------
struct Resultats {
    double ins_n = 0;        // insertion de n valeurs
    double ins_1 = 0;        // insertion de la valeur de l'utilisateur
    double extraction = 0;   // extraction de la racine
    double ajustement = 0;   // ajustement de l'arbre
    double total = 0;        // extraction + ajustement
    int maximum = 0;         // valeur lue à la racine
    bool separable = false;  // true si extraction et ajustement sont mesurés séparément
};

template <class Tas>
Resultats tester(const vector<int>& donnees, int valeur) {
    using horloge = chrono::steady_clock;
    Tas tas;
    Resultats r;
    r.separable = Tas::separable;

    // 1) insertion des n valeurs (le tas part de vide)
    auto a0 = horloge::now();
    for (int x : donnees) tas.inserer(x);
    auto a1 = horloge::now();

    // 2) insertion d'une seule valeur : celle entrée par l'utilisateur
    auto b0 = horloge::now();
    tas.inserer(valeur);
    auto b1 = horloge::now();

    r.ins_n = chrono::duration<double>(a1 - a0).count();
    r.ins_1 = chrono::duration<double>(b1 - b0).count();

    // 3) et 4) extraction de la racine, puis ajustement de l'arbre
    auto d0 = horloge::now();
    if constexpr (Tas::separable) {
        r.maximum = tas.extraire_racine();
        auto d1 = horloge::now();
        tas.ajuster();
        auto d2 = horloge::now();
        r.extraction = chrono::duration<double>(d1 - d0).count();
        r.ajustement = chrono::duration<double>(d2 - d1).count();
        r.total      = chrono::duration<double>(d2 - d0).count();
    } else {
        r.maximum = tas.extraire_et_ajuster();
        auto d2 = horloge::now();
        r.total = chrono::duration<double>(d2 - d0).count();
    }
    return r;
}

// ---------------------------------------------------------------
// Affichage en tableau
// ---------------------------------------------------------------
const int W0 = 34;   // largeur de la colonne "Opération"
const int W1 = 16;   // largeur des colonnes de résultats
const int W2 = 16;

// nombre de caractères affichés d'une chaîne UTF-8 (un accent compte pour 1)
int longueur_affichee(const string& s) {
    int n = 0;
    for (unsigned char c : s)
        if ((c & 0xC0) != 0x80) n++;
    return n;
}

string pad_droite(const string& s, int largeur) {   // texte aligné à gauche
    int manque = largeur - longueur_affichee(s);
    return s + string(manque > 0 ? manque : 0, ' ');
}

string pad_gauche(const string& s, int largeur) {   // texte aligné à droite
    int manque = largeur - longueur_affichee(s);
    return string(manque > 0 ? manque : 0, ' ') + s;
}

string en_secondes(double v) {
    ostringstream o;
    o << fixed << setprecision(9) << v;
    return o.str();
}

void separateur() {
    cout << "+" << string(W0 + 2, '-')
         << "+" << string(W1 + 2, '-')
         << "+" << string(W2 + 2, '-') << "+\n";
}

void entete(const string& a, const string& b, const string& c) {
    cout << "| " << pad_droite(a, W0)
         << " | " << pad_droite(b, W1)
         << " | " << pad_droite(c, W2) << " |\n";
}

void ligne(const string& label, const string& a, const string& b) {
    cout << "| " << pad_droite(label, W0)
         << " | " << pad_gauche(a, W1)
         << " | " << pad_gauche(b, W2) << " |\n";
}

// temps d'une étape, ou "non séparable" si la structure ne permet pas de la mesurer seule
string etape(const Resultats& r, double v) {
    return r.separable ? en_secondes(v) : "non separable";
}

void afficher_tableau(int n, const Resultats& sc, const Resultats& pq) {
    cout << "n = " << n << "   (temps en secondes)\n";
    separateur();
    entete("Operation", "Tas from scratch", "priority_queue");
    separateur();
    ligne("Insertion de n valeurs",        en_secondes(sc.ins_n), en_secondes(pq.ins_n));
    ligne("Insertion de la valeur saisie", en_secondes(sc.ins_1), en_secondes(pq.ins_1));
    ligne("Extraction de la racine (max)", etape(sc, sc.extraction), etape(pq, pq.extraction));
    ligne("Ajustement de l'arbre",         etape(sc, sc.ajustement), etape(pq, pq.ajustement));
    ligne("Total extraction + ajustement", en_secondes(sc.total), en_secondes(pq.total));
    separateur();
    cout << "Racine extraite : " << sc.maximum << " (from scratch), " << pq.maximum << " (priority_queue)\n\n";
}

int main() {
    int valeur;
    cout << "Entrez une valeur entiere a inserer apres les n insertions : ";
    if (!(cin >> valeur)) {
        cout << "Valeur invalide.\n";
        return 1;
    }
    cout << "\nValeur saisie : " << valeur << "\n";
    cout << "Note : pour priority_queue, pop() retire la racine et réajuste l'arbre en une seule\n"
         << "operation, donc seul le total extraction + ajustement peut être mesure.\n\n";

    for (int n : {10000, 100000, 1000000, 10000000, 100000000}) {
        mt19937 gen(42);                          // même graine => mêmes données
        uniform_int_distribution<int> dist(0, 1000000000);
        vector<int> donnees(n);
        for (int& x : donnees) x = dist(gen);

        Resultats sc = tester<TasScratch>(donnees, valeur);
        Resultats pq = tester<TasPQ>(donnees, valeur);
        afficher_tableau(n, sc, pq);
    }
    return 0;
}
