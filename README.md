# SkyrimNVDA — Guide du joueur

Plugin d'accessibilité pour Skyrim Special/Anniversary Edition. Vocalise automatiquement les menus du jeu via NVDA.

---

## Prérequis

- Skyrim Special Edition ou Anniversary Edition
- SKSE64
- NVDA (actif avant de lancer le jeu)

---

## Démarrage — Création du personnage

À l'ouverture du jeu, après la cinématique d'introduction, vous arrivez sur le **menu de création de personnage**.

NVDA annonce : *"Character creation"*

### Navigation dans la création de personnage

| Touche | Action |
|--------|--------|
| Pavé numérique 5 / 8 | Changer d'onglet (Race, Sexe, Apparence…) |
| Haut / Bas | Naviguer dans les options de l'onglet |
| Gauche / Droite | Modifier la valeur d'un curseur |
| R | Confirmer / valider |

---

## Menu en croix (Touche Tab)

NVDA annonce : *"Cross menu"*

| Touche | Destination |
|--------|-------------|
| Haut | Magie |
| Bas | Inventaire |
| Gauche | Menu compétences |
| Droite | Journal |

---

## Inventaire

NVDA annonce : *"Inventory open"*

- Navigation haut/bas : change d'objet → nom, valeur, poids vocalisés
- Navigation gauche/droite (ou Q/E) : change de catégorie
- **H** : annonce l'or en poche et le poids transporté / maximum

---

## Conteneur (coffre, corps…)

NVDA annonce : *"Container open"*

- Navigation haut/bas : change d'objet
- Navigation gauche/droite : bascule entre votre inventaire et le conteneur
- **H** : annonce l'or et le poids

---

## Menu Magie

NVDA annonce : *"Magic menu open"*

- Navigation haut/bas : change de sort → nom, effets, coût vocalisés
- Navigation gauche/droite : change de catégorie (Destruction, Guérison…)

---

## Journal (Touche J)

NVDA annonce : *"Journal open"*

- Navigation haut/bas : change de quête ou d'entrée
- **Pavé numérique 5 / 8** : change d'onglet (Quêtes, Inventaire, Compétences, Magie)

---

## Menu Compétences (depuis le menu en croix)

- Navigation haut/bas/gauche/droite : navigue dans l'arbre des compétences
- La description de la compétence sélectionnée est vocalisée automatiquement
- Les atouts (perks) de la branche sont vocalisés avec leur description et prérequis

---

## Menu Favoris (Touche Q)

NVDA annonce : *"Favorites"*

- Navigation haut/bas : change d'objet ou de sort favori

---

## Dialogue

La ligne de dialogue du PNJ est vocalisée automatiquement.
Navigation haut/bas pour choisir votre réponse.

---

## HUD (en jeu)

| Situation | Vocalisation |
|-----------|-------------|
| Objet/PNJ/porte en vue | Nom + action (ex: "Ouvrir porte") vocalisé automatiquement |
| Notification (quête, niveau…) | Vocalisée automatiquement |
| Sous-titre | Vocalisé automatiquement |
| Nouveau lieu découvert | Vocalisé automatiquement |
| **H** (en jeu) | Santé / Magie / Endurance actuelles |

---

## Menu principal

NVDA annonce : *"Main menu open"*

Navigation haut/bas pour Nouvelle partie, Continuer, Charger, Paramètres, Quitter.

---

## Montée de niveau

NVDA annonce : *"Level gained! Choose your improvement."*

Navigation gauche/droite pour choisir entre Santé, Magie ou Endurance.
**Entrée** pour confirmer.

---

## Boîte de message

Les messages du jeu (confirmations, avertissements) sont vocalisés automatiquement.
Navigation haut/bas pour choisir parmi les boutons, **Entrée** pour confirmer.

---

## Ce qui n'est pas encore vocalisé

- Marchands (acheter/vendre)
- Forges et tables d'enchantement (crafting)
- Menu carte

---

## Raccourcis clavier récapitulatif

| Touche | Action |
|--------|--------|
| H | Santé/Magie/Endurance (en jeu) ou Or/Poids (inventaire/conteneur) |
| Tab | Ouvre/ferme le menu en croix |
| J | Journal |
| Q | Favoris |

---

*Plugin développé par Pyrhame. Nécessite NVDA.*
