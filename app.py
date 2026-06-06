"""
╔══════════════════════════════════════════════════════╗
║  FAMILYBOARD — BACKEND (app.py)                      ║
║                                                      ║
║  Ce fichier est le "serveur" de FamilyBoard.         ║
║  Il reçoit les données du site web et les            ║
║  sauvegarde dans un fichier JSON que le boîtier      ║
║  ESP32 lira toutes les minutes via WiFi.             ║
║                                                      ║
║  Technologie utilisée : Flask (Python)               ║
║  Pour lancer le serveur : python app.py              ║
╚══════════════════════════════════════════════════════╝
"""

# ─────────────────────────────────────────────
# IMPORTS — on charge les bibliothèques dont
# on a besoin pour faire fonctionner le serveur.
# ─────────────────────────────────────────────

from flask import Flask, request, jsonify
# Flask    : le framework qui crée notre serveur web
# request  : permet de lire les données envoyées par le site
# jsonify  : transforme un dictionnaire Python en réponse JSON

from flask_cors import CORS
# CORS : autorise le site web (sur un autre domaine) à parler
# au serveur. Sans ça, le navigateur bloquerait les requêtes.

import json
# json : pour lire et écrire des fichiers .json

import os
# os : pour vérifier si un fichier existe, créer des dossiers, etc.

from datetime import datetime
# datetime : pour obtenir la date et l'heure actuelles


# ─────────────────────────────────────────────
# CONFIGURATION
# ─────────────────────────────────────────────

# Nom du fichier JSON qui stocke les pages du boîtier.
# C'est ce fichier que l'ESP32 lira toutes les minutes.
FICHIER_PAGES = "pages.json"

# Code d'accès pour se connecter au site.
# ⚠️ Pour le prototype — à remplacer par une vraie gestion
#    d'authentification plus tard.
CODE_ACCES = "famille2024"

# Numéro de série du boîtier prototype.
NUMERO_SERIE = "FB-0001"


# ─────────────────────────────────────────────
# CRÉATION DE L'APPLICATION FLASK
# ─────────────────────────────────────────────

# On crée l'application Flask.
# "__name__" dit à Flask quel fichier il est en train d'exécuter.
app = Flask(__name__)

# On active CORS pour tous les domaines.
# Cela permet au site (hébergé sur GitHub Pages par exemple)
# d'envoyer des requêtes à ce serveur.
CORS(app)


# ─────────────────────────────────────────────
# FONCTIONS UTILITAIRES
# Ces fonctions sont utilisées par les "routes"
# définies plus bas.
# ─────────────────────────────────────────────

def lire_pages():
    """
    Lit le fichier pages.json et retourne son contenu
    sous forme de dictionnaire Python.
    Si le fichier n'existe pas encore, retourne une
    structure vide par défaut.
    """
    # os.path.exists vérifie si le fichier existe
    if not os.path.exists(FICHIER_PAGES):
        # Le fichier n'existe pas : on retourne une structure vide
        return creer_pages_vides()

    # Ouvre le fichier en lecture ("r") avec l'encodage UTF-8
    # (important pour les accents français)
    with open(FICHIER_PAGES, "r", encoding="utf-8") as f:
        return json.load(f)  # json.load transforme le JSON en dict Python


def sauvegarder_pages(donnees):
    """
    Sauvegarde le dictionnaire Python dans le fichier pages.json.
    C'est ce fichier que l'ESP32 lira.

    Paramètre :
        donnees (dict) : les données à sauvegarder
    """
    # Ouvre le fichier en écriture ("w"), le crée s'il n'existe pas
    with open(FICHIER_PAGES, "w", encoding="utf-8") as f:
        # json.dump transforme le dict Python en JSON et l'écrit dans le fichier
        # indent=2 : formate le JSON avec une indentation de 2 espaces (lisible)
        # ensure_ascii=False : autorise les accents (é, à, ç...)
        json.dump(donnees, f, indent=2, ensure_ascii=False)


def creer_pages_vides():
    """
    Retourne la structure JSON par défaut avec 5 pages vides.
    Utilisée quand le fichier n'existe pas encore.
    """
    return {
        "boitier_id": NUMERO_SERIE,
        "derniere_mise_a_jour": datetime.now().isoformat(),
        "pages": [
            {"page": i, "date": "", "message": ""}
            for i in range(1, 6)  # range(1, 6) = [1, 2, 3, 4, 5]
        ]
    }


# ─────────────────────────────────────────────
# ROUTES DU SERVEUR
#
# Une "route" est une adresse URL que le serveur
# écoute. Quand quelqu'un envoie une requête à
# cette adresse, Flask exécute la fonction associée.
#
# Exemple :
#   GET  http://serveur/api/pages  → retourne les pages
#   POST http://serveur/api/pages  → sauvegarde les pages
# ─────────────────────────────────────────────


# ── ROUTE 1 : Vérification que le serveur fonctionne ──
@app.route("/", methods=["GET"])
def accueil():
    """
    Route d'accueil — juste pour vérifier que le serveur tourne.
    Accessible en ouvrant http://localhost:5000 dans un navigateur.
    """
    return jsonify({
        "statut": "ok",
        "message": "FamilyBoard Backend opérationnel 🏠",
        "version": "0.1"
    })


# ── ROUTE 2 : Connexion (vérification du code d'accès) ──
@app.route("/api/connexion", methods=["POST"])
def connexion():
    """
    Vérifie le numéro de série et le code d'accès envoyés
    par la page de connexion du site web.

    Reçoit (JSON) :
        { "numero_serie": "FB-0001", "code": "famille2024" }

    Retourne (JSON) :
        { "succes": true }  ou  { "succes": false, "erreur": "..." }
    """
    # Récupère les données JSON envoyées par le site
    # get_json() transforme le corps de la requête en dict Python
    donnees = request.get_json()

    # Vérifie que les deux champs sont bien présents
    if not donnees or "numero_serie" not in donnees or "code" not in donnees:
        return jsonify({
            "succes": False,
            "erreur": "Données manquantes"
        }), 400  # 400 = erreur de la part du client

    # Compare les valeurs reçues aux valeurs attendues
    if (donnees["numero_serie"] == NUMERO_SERIE and
            donnees["code"] == CODE_ACCES):
        return jsonify({"succes": True})
    else:
        return jsonify({
            "succes": False,
            "erreur": "Numéro de série ou code incorrect"
        }), 401  # 401 = non autorisé


# ── ROUTE 3 : Lire les pages actuelles ──
@app.route("/api/pages", methods=["GET"])
def lire_pages_route():
    """
    Retourne le contenu actuel des 5 pages du boîtier.

    Utilisé par :
    - Le site web pour afficher les pages déjà enregistrées
    - L'ESP32 pour lire les messages toutes les minutes (polling)

    Retourne (JSON) :
        {
          "boitier_id": "FB-0001",
          "derniere_mise_a_jour": "2024-05-24T18:00:00",
          "pages": [
            {"page": 1, "date": "24 mai", "message": "Dentiste"},
            ...
          ]
        }
    """
    pages = lire_pages()
    return jsonify(pages)


# ── ROUTE 4 : Sauvegarder les nouvelles pages ──
@app.route("/api/pages", methods=["POST"])
def sauvegarder_pages_route():
    """
    Reçoit les 5 pages depuis le site web et les sauvegarde
    dans pages.json. C'est cette route qu'on appelle quand
    l'utilisateur clique sur "Envoyer au boîtier".

    Reçoit (JSON) :
        {
          "boitier_id": "FB-0001",
          "pages": [
            {"page": 1, "date": "24 mai 18h", "message": "Dentiste Thomas"},
            ...
          ]
        }

    Retourne (JSON) :
        { "succes": true, "message": "Pages sauvegardées" }
    """
    # Récupère les données envoyées par le site
    donnees = request.get_json()

    # Vérifie que les données sont valides
    if not donnees or "pages" not in donnees:
        return jsonify({
            "succes": False,
            "erreur": "Données manquantes ou invalides"
        }), 400

    # Vérifie qu'on a bien 5 pages
    if len(donnees["pages"]) != 5:
        return jsonify({
            "succes": False,
            "erreur": "Il faut exactement 5 pages"
        }), 400

    # Construit l'objet à sauvegarder
    # On ajoute la date/heure de mise à jour automatiquement
    a_sauvegarder = {
        "boitier_id": NUMERO_SERIE,
        "derniere_mise_a_jour": datetime.now().isoformat(),
        "pages": []
    }

    # Traite chaque page reçue
    for page in donnees["pages"]:
        a_sauvegarder["pages"].append({
            "page":    page.get("page", 0),       # .get() évite une erreur si la clé manque
            "date":    page.get("date", ""),
            "message": page.get("message", "")
        })

    # Sauvegarde dans le fichier JSON
    sauvegarder_pages(a_sauvegarder)

    print(f"✅ Pages sauvegardées à {datetime.now().strftime('%H:%M:%S')}")

    return jsonify({
        "succes": True,
        "message": "Pages sauvegardées avec succès",
        "mise_a_jour": a_sauvegarder["derniere_mise_a_jour"]
    })


# ─────────────────────────────────────────────
# LANCEMENT DU SERVEUR
#
# Ce bloc s'exécute uniquement quand on lance
# directement ce fichier avec "python app.py".
# Il ne s'exécute PAS si le fichier est importé
# par un autre script.
# ─────────────────────────────────────────────
if __name__ == "__main__":
    print("═" * 50)
    print("  FamilyBoard Backend — Démarrage")
    print("═" * 50)
    print(f"  Boîtier : {NUMERO_SERIE}")
    print(f"  Fichier JSON : {FICHIER_PAGES}")
    print(f"  Adresse locale : http://localhost:5000")
    print("═" * 50)

    # debug=True : relance automatiquement le serveur si on modifie le code
    # host="0.0.0.0" : accessible depuis d'autres appareils sur le réseau local
    # port=5000 : numéro du port d'écoute
    app.run(debug=True, host="0.0.0.0", port=5000)
