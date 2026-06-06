/*
╔══════════════════════════════════════════════════════════╗
║  FAMILYBOARD — CODE ESP32                                ║
║                                                          ║
║  Ce programme tourne sur l'ESP32 et fait :              ║
║    1. Se connecte au WiFi de la maison                   ║
║    2. Toutes les minutes, lit le fichier JSON            ║
║       sur le serveur pour voir s'il y a du nouveau       ║
║    3. Si le contenu a changé → bipppe le buzzer          ║
║    4. Affiche les 5 pages sur l'écran OLED               ║
║    5. Les 2 boutons permettent de changer de page        ║
║                                                          ║
║  Matériel :                                              ║
║    - ESP32                                               ║
║    - Écran OLED 1.30" SPI (SH1106 ou SSD1306)           ║
║    - 2 boutons poussoirs                                 ║
║    - 1 buzzer                                            ║
║                                                          ║
║  Bibliothèques à installer dans Arduino IDE :            ║
║    - U8g2  (pour l'écran OLED)                          ║
║    - ArduinoJson  (pour lire le JSON)                    ║
║    Ces deux bibliothèques s'installent via               ║
║    Outils > Gérer les bibliothèques dans Arduino IDE     ║
╚══════════════════════════════════════════════════════════╝
*/


// ─────────────────────────────────────────────
// INCLUSION DES BIBLIOTHÈQUES
// On charge les outils dont on a besoin.
// ─────────────────────────────────────────────

#include <WiFi.h>
// WiFi.h : permet à l'ESP32 de se connecter à un réseau WiFi
// C'est une bibliothèque incluse automatiquement avec l'ESP32

#include <HTTPClient.h>
// HTTPClient.h : permet d'envoyer des requêtes HTTP
// (comme un navigateur qui charge une page web)
// Aussi incluse automatiquement avec l'ESP32

#include <ArduinoJson.h>
// ArduinoJson.h : permet de lire et décoder du JSON
// ⚠️ À installer manuellement : Outils > Gérer les bibliothèques > "ArduinoJson"

#include <U8g2lib.h>
// U8g2lib.h : bibliothèque pour piloter les écrans OLED
// ⚠️ À installer manuellement : Outils > Gérer les bibliothèques > "U8g2"

#include <SPI.h>
// SPI.h : protocole de communication entre l'ESP32 et l'écran OLED
// Incluse automatiquement avec Arduino


// ─────────────────────────────────────────────
// CONFIGURATION WIFI
// ⚠️ MODIFIE CES DEUX LIGNES avec ton réseau !
// ─────────────────────────────────────────────

const char* WIFI_SSID     = "NOM_DE_TON_WIFI";       // Nom de ton réseau WiFi
const char* WIFI_PASSWORD = "MOT_DE_PASSE_WIFI";      // Mot de passe WiFi


// ─────────────────────────────────────────────
// CONFIGURATION SERVEUR
// ⚠️ MODIFIE L'URL selon où tourne ton backend :
//   - En test local sur ton PC → "http://192.168.X.X:5000/api/pages"
//     (remplace X.X par l'IP locale de ton PC,
//      visible dans les paramètres réseau de ton PC)
//   - En production sur Render → "https://familyboard.onrender.com/api/pages"
// ─────────────────────────────────────────────

const char* URL_SERVEUR = "https://familyboard-ed92.onrender.com/api/pages";


// ─────────────────────────────────────────────
// CONFIGURATION DES BROCHES (PINS)
//
// Une "broche" est un connecteur physique sur l'ESP32.
// On associe chaque composant à un numéro de broche.
//
// BROCHAGE ÉCRAN OLED SPI :
//   OLED CLK  → broche 18 (horloge SPI)
//   OLED MOSI → broche 23 (données SPI)
//   OLED CS   → broche  5 (chip select : active l'écran)
//   OLED DC   → broche  2 (data/command : mode données ou commande)
//   OLED RST  → broche  4 (reset : redémarre l'écran)
//
// BOUTONS :
//   Bouton SUIVANT   → broche 34
//   Bouton PRÉCÉDENT → broche 35
//
// BUZZER :
//   Buzzer → broche 32
// ─────────────────────────────────────────────

// Broches de l'écran OLED
#define OLED_CLK  18
#define OLED_MOSI 23
#define OLED_CS    5
#define OLED_DC    2
#define OLED_RST   4

// Broches des boutons
// INPUT_PULLUP signifie qu'on utilise la résistance interne de l'ESP32
// Le bouton est en circuit ouvert (HIGH) par défaut, et LOW quand appuyé
#define BOUTON_SUIVANT   34    // Bouton pour aller à la page suivante
#define BOUTON_PRECEDENT 35    // Bouton pour aller à la page précédente

// Broche du buzzer
#define BUZZER_PIN 32


// ─────────────────────────────────────────────
// CONFIGURATION DE L'ÉCRAN OLED
//
// U8g2 supporte beaucoup de modèles d'écrans.
// On choisit le bon constructeur selon le chip.
//
// Si ton écran est SH1106 (le plus courant sur 1.30") :
//   → Utilise la ligne U8G2_SH1106_...
// Si ton écran est SSD1306 :
//   → Commente la ligne SH1106 et décommente SSD1306
//
// "U8G2_R0" = pas de rotation
// "U8G2_R2" = rotation 180° (si ton écran est à l'envers)
// ─────────────────────────────────────────────

// ✅ SH1106 — décommente cette ligne si ton écran est SH1106
U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI ecran(
  U8G2_R0,       // Orientation (R0 = normale)
  OLED_CLK,      // Broche horloge
  OLED_MOSI,     // Broche données
  OLED_CS,       // Broche chip select
  OLED_DC,       // Broche data/command
  OLED_RST       // Broche reset
);

// ❌ SSD1306 — décommente cette ligne à la place si SSD1306
// U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI ecran(U8G2_R0, OLED_CLK, OLED_MOSI, OLED_CS, OLED_DC, OLED_RST);


// ─────────────────────────────────────────────
// VARIABLES GLOBALES
//
// Ces variables sont accessibles depuis toutes
// les fonctions du programme.
// ─────────────────────────────────────────────

// Pages reçues du serveur
// On stocke chaque page comme deux chaînes de caractères :
// la date et le message
struct Page {
  String date;      // Ex : "24 mai 18h"
  String message;   // Ex : "Dentiste Thomas"
};

// Tableau des 5 pages
Page pages[5];

// Numéro de la page actuellement affichée (0 à 4)
int pageActuelle = 0;

// Nombre total de pages reçues (normalement 5)
int nombrePages = 0;

// Mémorise le contenu JSON reçu lors du dernier polling
// Sert à détecter si quelque chose a changé
String dernierContenuJSON = "";

// Temps (en millisecondes) du dernier polling
// millis() retourne le nombre de ms depuis le démarrage de l'ESP32
unsigned long dernierPolling = 0;

// Intervalle de polling : 60 000 ms = 1 minute
const unsigned long INTERVALLE_POLLING = 60000;

// État actuel des boutons (true = appuyé lors du dernier cycle)
// Sert à détecter le moment où on appuie (pas le maintien)
bool etatBoutonSuivant   = HIGH;   // HIGH = non appuyé (avec INPUT_PULLUP)
bool etatBoutonPrecedent = HIGH;

// Indique si l'ESP32 est connecté au WiFi
bool wifiConnecte = false;


// ─────────────────────────────────────────────
// SETUP — S'exécute UNE SEULE FOIS au démarrage
// ─────────────────────────────────────────────

void setup() {

  // ── Démarrage du port série ──
  // Permet d'afficher des messages dans le "Moniteur série"
  // d'Arduino IDE pour déboguer (voir ce qui se passe)
  // 115200 est la vitesse de communication en bauds
  Serial.begin(115200);
  Serial.println("═══════════════════════════════");
  Serial.println("  FamilyBoard — Démarrage ESP32");
  Serial.println("═══════════════════════════════");

  // ── Configuration des broches ──

  // Les boutons sont en entrée avec résistance pull-up interne
  // INPUT_PULLUP = la broche est HIGH par défaut, LOW quand le bouton est pressé
  pinMode(BOUTON_SUIVANT,   INPUT_PULLUP);
  pinMode(BOUTON_PRECEDENT, INPUT_PULLUP);

  // Le buzzer est en sortie (on lui envoie du courant)
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW); // Éteint le buzzer au démarrage

  // ── Démarrage de l'écran OLED ──
  ecran.begin();
  ecran.setContrast(255); // Luminosité maximale (0-255)

  // Affiche un écran de démarrage
  afficherEcranDemarrage();

  // ── Connexion WiFi ──
  connecterWiFi();

  // ── Premier polling immédiat ──
  // On n'attend pas 1 minute pour la première lecture
  if (wifiConnecte) {
    lirePages();
  }
}


// ─────────────────────────────────────────────
// LOOP — S'exécute EN BOUCLE indéfiniment
// C'est le cœur du programme.
// ─────────────────────────────────────────────

void loop() {

  // ── Vérification du polling (toutes les minutes) ──
  // millis() donne le temps écoulé depuis le démarrage
  // Si la différence avec le dernier polling dépasse 1 minute → on relit
  if (wifiConnecte && (millis() - dernierPolling >= INTERVALLE_POLLING)) {
    lirePages();
    dernierPolling = millis(); // Remet le compteur à zéro
  }

  // ── Lecture des boutons ──
  lireBoutons();

  // ── Petit délai pour ne pas saturer le processeur ──
  // Sans délai, le loop tournerait des millions de fois par seconde
  // 50ms est un bon compromis : réactif mais économe
  delay(50);
}


// ─────────────────────────────────────────────
// FONCTION : connecterWiFi()
// Tente de se connecter au réseau WiFi.
// Affiche la progression sur l'écran OLED.
// ─────────────────────────────────────────────

void connecterWiFi() {
  Serial.print("Connexion WiFi à ");
  Serial.println(WIFI_SSID);

  // Affiche "Connexion WiFi..." sur l'écran
  ecran.clearBuffer();          // Efface le buffer interne (mémoire de l'écran)
  ecran.setFont(u8g2_font_6x10_tf); // Choisit une petite police
  ecran.drawStr(0, 12, "Connexion WiFi...");
  ecran.drawStr(0, 26, WIFI_SSID);
  ecran.sendBuffer();           // Envoie le buffer à l'écran (affichage réel)

  // Lance la connexion WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Attend que la connexion soit établie
  // WL_CONNECTED est une constante qui signifie "connecté"
  int tentatives = 0;
  while (WiFi.status() != WL_CONNECTED && tentatives < 20) {
    delay(500);               // Attend 500ms
    Serial.print(".");        // Affiche un point dans le moniteur série
    tentatives++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    // ✅ Connexion réussie
    wifiConnecte = true;
    Serial.println("\nWiFi connecté !");
    Serial.print("Adresse IP : ");
    Serial.println(WiFi.localIP()); // Affiche l'IP attribuée par le routeur

    // Affiche l'IP sur l'écran
    ecran.clearBuffer();
    ecran.setFont(u8g2_font_6x10_tf);
    ecran.drawStr(0, 12, "WiFi connecte !");
    ecran.drawStr(0, 28, "IP :");
    ecran.drawStr(0, 42, WiFi.localIP().toString().c_str());
    // .toString() convertit l'IP en texte
    // .c_str() convertit le String Arduino en "const char*" pour U8g2
    ecran.sendBuffer();
    delay(2000); // Laisse 2 secondes pour lire l'IP

  } else {
    // ❌ Connexion échouée
    wifiConnecte = false;
    Serial.println("\nEchec connexion WiFi !");

    ecran.clearBuffer();
    ecran.setFont(u8g2_font_6x10_tf);
    ecran.drawStr(0, 12, "Erreur WiFi !");
    ecran.drawStr(0, 28, "Verifie le nom");
    ecran.drawStr(0, 42, "et mot de passe");
    ecran.sendBuffer();
  }
}


// ─────────────────────────────────────────────
// FONCTION : lirePages()
// Se connecte au serveur, lit le JSON,
// et met à jour les pages si le contenu a changé.
// ─────────────────────────────────────────────

void lirePages() {
  Serial.println("Polling serveur...");

  // Vérifie qu'on est toujours connecté au WiFi
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi déconnecté, tentative de reconnexion...");
    connecterWiFi();
    return; // Arrête la fonction ici si pas reconnecté
  }

  // ── Crée un client HTTP et envoie la requête ──
  HTTPClient http;

  // Initialise la connexion vers l'URL du serveur
  http.begin(URL_SERVEUR);

  // Envoie la requête GET (lire des données)
  // La fonction retourne un code HTTP :
  //   200 = OK, 404 = page introuvable, -1 = erreur réseau...
  int codeReponse = http.GET();

  Serial.print("Code réponse HTTP : ");
  Serial.println(codeReponse);

  if (codeReponse == 200) {
    // ✅ Le serveur a répondu correctement

    // Récupère le contenu de la réponse (le JSON) sous forme de texte
    String contenuJSON = http.getString();

    Serial.println("JSON reçu : " + contenuJSON);

    // ── Vérifie si le contenu a changé depuis le dernier polling ──
    if (contenuJSON != dernierContenuJSON) {
      // Le contenu est différent !
      Serial.println("Nouveau contenu détecté !");

      // Mémorise le nouveau contenu pour la prochaine comparaison
      dernierContenuJSON = contenuJSON;

      // Décode le JSON et met à jour les pages
      bool succes = decoderJSON(contenuJSON);

      if (succes) {
        // Fait biper le buzzer pour alerter l'utilisateur
        bipper();

        // Affiche la première page (index 0)
        pageActuelle = 0;
        afficherPage(pageActuelle);
      }

    } else {
      // Le contenu n'a pas changé, rien à faire
      Serial.println("Pas de changement.");
    }

  } else {
    // ❌ Erreur de connexion au serveur
    Serial.print("Erreur serveur : ");
    Serial.println(codeReponse);
    afficherErreurServeur();
  }

  // Libère les ressources du client HTTP
  // Important : sans ça, l'ESP32 manquerait de mémoire
  http.end();
}


// ─────────────────────────────────────────────
// FONCTION : decoderJSON(contenu)
// Transforme le texte JSON en données utilisables.
// Remplit le tableau "pages[]".
// Retourne true si tout s'est bien passé.
// ─────────────────────────────────────────────

bool decoderJSON(String contenu) {

  /*
    ArduinoJson utilise un "document" pour stocker le JSON décodé.
    La taille (2048) doit être assez grande pour contenir tout le JSON.
    Si ton JSON est plus grand, augmente ce nombre.
    Tu peux calculer la taille exacte sur : https://arduinojson.org/v6/assistant/
  */
  StaticJsonDocument<2048> doc;

  // deserializeJson() lit le texte JSON et le met dans "doc"
  // "erreur" contiendra les détails si quelque chose ne va pas
  DeserializationError erreur = deserializeJson(doc, contenu);

  if (erreur) {
    // Le JSON est invalide ou mal formé
    Serial.print("Erreur JSON : ");
    Serial.println(erreur.c_str());
    return false;
  }

  // Récupère le tableau "pages" du JSON
  // doc["pages"] correspond à la clé "pages" dans le JSON
  JsonArray tableauPages = doc["pages"].as<JsonArray>();

  // Compte le nombre de pages reçues
  nombrePages = 0;

  // Parcourt chaque page du tableau JSON
  // "page" est un objet JSON contenant "page", "date", "message"
  for (JsonObject page : tableauPages) {

    // Récupère l'index de la page (1 à 5) et le convertit en index 0 à 4
    int index = page["page"].as<int>() - 1;

    // Vérifie que l'index est valide (entre 0 et 4)
    if (index >= 0 && index < 5) {

      // Stocke la date et le message dans le tableau pages[]
      // page["date"].as<String>() convertit la valeur JSON en String Arduino
      pages[index].date    = page["date"].as<String>();
      pages[index].message = page["message"].as<String>();

      nombrePages++;

      Serial.print("Page ");
      Serial.print(index + 1);
      Serial.print(" : ");
      Serial.print(pages[index].date);
      Serial.print(" | ");
      Serial.println(pages[index].message);
    }
  }

  Serial.print(nombrePages);
  Serial.println(" pages chargées.");
  return true;
}


// ─────────────────────────────────────────────
// FONCTION : bipper()
// Fait biper le buzzer une seule fois.
// ─────────────────────────────────────────────

void bipper() {
  Serial.println("BIP !");

  // Active le buzzer (envoie du courant)
  digitalWrite(BUZZER_PIN, HIGH);

  // Attend 800ms (durée du bip)
  delay(800);

  // Éteint le buzzer
  digitalWrite(BUZZER_PIN, LOW);
}


// ─────────────────────────────────────────────
// FONCTION : lireBoutons()
// Détecte quand un bouton est appuyé et
// change la page affichée en conséquence.
//
// On détecte le FRONT DESCENDANT (HIGH → LOW)
// c'est-à-dire le moment exact où on appuie,
// pas le maintien. Cela évite de changer
// plusieurs pages d'un seul appui.
// ─────────────────────────────────────────────

void lireBoutons() {

  // Lit l'état actuel des deux boutons
  // digitalRead retourne HIGH (1) ou LOW (0)
  bool etatActuelSuivant   = digitalRead(BOUTON_SUIVANT);
  bool etatActuelPrecedent = digitalRead(BOUTON_PRECEDENT);

  // ── Bouton SUIVANT ──
  // Front descendant = était HIGH, maintenant LOW = vient d'être appuyé
  if (etatBoutonSuivant == HIGH && etatActuelSuivant == LOW) {
    Serial.println("Bouton SUIVANT appuyé");

    // Passe à la page suivante
    // Le modulo (%) permet de revenir à 0 après la dernière page
    // Exemple avec 5 pages : 0→1→2→3→4→0→1...
    if (nombrePages > 0) {
      pageActuelle = (pageActuelle + 1) % nombrePages;
      afficherPage(pageActuelle);
    }

    delay(200); // Anti-rebond : ignore les signaux parasites pendant 200ms
  }

  // ── Bouton PRÉCÉDENT ──
  if (etatBoutonPrecedent == HIGH && etatActuelPrecedent == LOW) {
    Serial.println("Bouton PRÉCÉDENT appuyé");

    // Passe à la page précédente
    // Si on est à la page 0, on va à la dernière page (nombrePages - 1)
    if (nombrePages > 0) {
      pageActuelle = (pageActuelle - 1 + nombrePages) % nombrePages;
      // +nombrePages évite les valeurs négatives en C++
      // Exemple : (0 - 1 + 5) % 5 = 4 → dernière page
      afficherPage(pageActuelle);
    }

    delay(200); // Anti-rebond
  }

  // Mémorise l'état actuel pour le prochain cycle
  etatBoutonSuivant   = etatActuelSuivant;
  etatBoutonPrecedent = etatActuelPrecedent;
}


// ─────────────────────────────────────────────
// FONCTION : afficherPage(index)
// Affiche le contenu d'une page sur l'écran OLED.
// ─────────────────────────────────────────────

void afficherPage(int index) {

  // Vérifie qu'il y a des pages à afficher
  if (nombrePages == 0) {
    afficherAucunMessage();
    return;
  }

  Serial.print("Affichage page ");
  Serial.println(index + 1);

  // ── Construit l'affichage dans le buffer ──
  // On prépare tout en mémoire avant d'envoyer à l'écran
  // pour éviter le scintillement
  ecran.clearBuffer();

  // ── Ligne de titre en haut ──
  // Fond noir avec texte blanc inversé pour le titre
  ecran.setDrawColor(1);        // Couleur de dessin : blanc (allumé)
  ecran.drawBox(0, 0, 128, 14); // Rectangle plein en haut (fond de titre)
  ecran.setDrawColor(0);        // Couleur de dessin : noir (éteint)
  ecran.setFont(u8g2_font_6x10_tf);

  // Construit le texte du titre "Page X / Y"
  // String() convertit un int en String en C++ Arduino
  String titrePage = "Page " + String(index + 1) + " / " + String(nombrePages);
  ecran.drawStr(4, 11, titrePage.c_str());

  ecran.setDrawColor(1); // Remet la couleur en blanc pour la suite

  // ── Affichage de la date ──
  ecran.setFont(u8g2_font_6x10_tf); // Police normale pour la date
  String date = pages[index].date;

  if (date.length() > 0) {
    // Il y a une date → on l'affiche avec une petite icône calendrier
    ecran.drawStr(0, 28, ("@ " + date).c_str());
    // .c_str() est nécessaire car U8g2 attend un "const char*"
    // et non un "String" Arduino
  }

  // ── Affichage du message (avec retour à la ligne automatique) ──
  ecran.setFont(u8g2_font_6x10_tf);

  String message = pages[index].message;

  if (message.length() > 0) {
    // L'écran fait 128 pixels de large, chaque caractère fait ~6px
    // Donc environ 21 caractères par ligne
    // On découpe le message en lignes de 21 caractères maximum
    afficherTexteMultilignes(message, 0, 42, 21);
  } else {
    ecran.setFont(u8g2_font_6x10_tf);
    ecran.drawStr(0, 42, "(page vide)");
  }

  // ── Indicateur de page en bas à droite ──
  // Petits points pour indiquer quelle page est active
  afficherIndicateurPages(index);

  // Envoie tout le buffer à l'écran (affichage réel)
  ecran.sendBuffer();
}


// ─────────────────────────────────────────────
// FONCTION : afficherTexteMultilignes(texte, x, y, largeur)
// Découpe un texte long en plusieurs lignes
// pour qu'il tienne sur l'écran.
// ─────────────────────────────────────────────

void afficherTexteMultilignes(String texte, int x, int y, int largeurMax) {

  int longueur = texte.length();
  int debut    = 0;     // Position de début de la ligne courante
  int ligneY   = y;     // Position verticale de la ligne courante

  while (debut < longueur && ligneY < 64) {
    // Calcule la fin de cette ligne
    int fin = debut + largeurMax;

    if (fin >= longueur) {
      // On est sur la dernière ligne, on prend tout ce qui reste
      fin = longueur;
    } else {
      // On cherche le dernier espace avant la limite
      // pour ne pas couper un mot en plein milieu
      int dernierEspace = texte.lastIndexOf(' ', fin);
      if (dernierEspace > debut) {
        fin = dernierEspace; // Coupe à l'espace
      }
    }

    // Extrait la portion de texte pour cette ligne
    // substring(debut, fin) retourne les caractères de debut à fin-1
    String ligne = texte.substring(debut, fin);
    ecran.drawStr(x, ligneY, ligne.c_str());

    // Avance à la ligne suivante
    debut  = fin + 1;  // +1 pour sauter l'espace
    ligneY += 12;      // 12 pixels de hauteur par ligne
  }
}


// ─────────────────────────────────────────────
// FONCTION : afficherIndicateurPages(indexActuel)
// Affiche de petits points en bas de l'écran
// pour montrer quelle page est affichée.
// ─────────────────────────────────────────────

void afficherIndicateurPages(int indexActuel) {

  if (nombrePages <= 1) return; // Pas besoin si une seule page

  // Taille et espacement des points
  int taillePoint = 4;    // Diamètre du point en pixels
  int espacement  = 8;    // Distance entre les centres des points

  // Calcule la position x de départ pour centrer les points
  int largeurTotale = (nombrePages - 1) * espacement + taillePoint;
  int xDepart = (128 - largeurTotale) / 2;

  // Dessine un point par page
  for (int i = 0; i < nombrePages; i++) {
    int xPoint = xDepart + i * espacement;

    if (i == indexActuel) {
      // Page active : point plein (rempli)
      ecran.drawDisc(xPoint, 61, taillePoint / 2);
    } else {
      // Autres pages : cercle vide
      ecran.drawCircle(xPoint, 61, taillePoint / 2);
    }
  }
}


// ─────────────────────────────────────────────
// FONCTION : afficherEcranDemarrage()
// Affiche le logo au démarrage.
// ─────────────────────────────────────────────

void afficherEcranDemarrage() {
  ecran.clearBuffer();

  // Titre centré
  ecran.setFont(u8g2_font_9x15B_tf); // Police plus grande et grasse
  ecran.drawStr(14, 28, "FamilyBoard");

  ecran.setFont(u8g2_font_6x10_tf);
  ecran.drawStr(28, 44, "Demarrage...");

  // Ligne décorative
  ecran.drawHLine(0, 52, 128); // Ligne horizontale toute la largeur

  ecran.setFont(u8g2_font_5x7_tf); // Très petite police
  ecran.drawStr(28, 62, "Connexion WiFi...");

  ecran.sendBuffer();
  delay(1500);
}


// ─────────────────────────────────────────────
// FONCTION : afficherAucunMessage()
// Affiché quand aucune page n'est chargée.
// ─────────────────────────────────────────────

void afficherAucunMessage() {
  ecran.clearBuffer();
  ecran.setFont(u8g2_font_6x10_tf);
  ecran.drawStr(10, 20, "Aucun message");
  ecran.drawStr(4, 36, "Envoie un message");
  ecran.drawStr(10, 50, "depuis le site !");
  ecran.sendBuffer();
}


// ─────────────────────────────────────────────
// FONCTION : afficherErreurServeur()
// Affiché quand le serveur ne répond pas.
// ─────────────────────────────────────────────

void afficherErreurServeur() {
  ecran.clearBuffer();
  ecran.setFont(u8g2_font_6x10_tf);
  ecran.drawStr(0, 14, "Erreur serveur");
  ecran.drawStr(0, 30, "Verification dans");
  ecran.drawStr(0, 44, "1 minute...");
  ecran.sendBuffer();
}
