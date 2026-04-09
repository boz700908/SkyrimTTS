SkyrimNVDA — Guide du joueur

Plugin d'accessibilite pour Skyrim Special/Anniversary Edition. Vocalise automatiquement les menus du jeu, ajoute un scanner d'objets, la marche auto, la visee auto et le suivi de quetes via NVDA.

---

Soutenir le projet

Si vous souhaitez me soutenir et m'encourager dans mon travail d'accessibilite sur ce mod et les futurs que je ferai, vous pouvez retrouver ma page de dons ici et m'offrir un cafe ! Un grand merci !

https://buymeacoffee.com/pyrhame?l=en

---

Configuration requise

- Skyrim Special Edition ou Anniversary Edition
- SKSE64 (Skyrim Script Extender)
- Lecteur d'ecran NVDA (doit etre lance avant le jeu)
- **SkyUI fortement recommande** — SkyrimNVDA a ete concu et teste en priorite avec la disposition d'inventaire/conteneur/marchand de SkyUI. Sans SkyUI le plugin fonctionne quand meme mais certaines lectures de menus peuvent etre moins precises ou manquer des details.

---

Installation

Votre dossier d'installation de Skyrim se trouve generalement a :
C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\

Etape 1 : Installer SKSE64

Si ce n'est pas deja fait, telechargez et installez SKSE64 depuis https://skse.silverlock.org/. Suivez ses instructions d'installation. Vous devez lancer le jeu via skse64_loader.exe au lieu du lanceur normal de Skyrim.

Etape 2 : Copier les fichiers du plugin

Depuis l'archive de la release, copiez les fichiers suivants aux bons emplacements dans votre dossier d'installation Skyrim :

- SkyrimNVDA.dll : copier dans Data\SKSE\Plugins\SkyrimNVDA.dll
- nvdaControllerClient.dll : copier a la racine du dossier Skyrim (a cote de SkyrimSE.exe)
- SkyrimTTS_AutoWalk.esp : copier dans Data\SkyrimTTS_AutoWalk.esp
- SkyrimTTS_AutoWalk.pex : copier dans Data\Scripts\SkyrimTTS_AutoWalk.pex
- SkyrimTTS_MQ105Fix.pex : copier dans Data\Scripts\SkyrimTTS_MQ105Fix.pex

Si les dossiers SKSE\Plugins\ ou Scripts\ n'existent pas dans Data\, creez-les.

Note : Si vous installez via un gestionnaire de mods (Vortex, MO2), vous devez quand meme copier manuellement nvdaControllerClient.dll a la racine du dossier Skyrim — les gestionnaires de mods n'installent les fichiers que dans Data\.

Etape 3 : Activer l'ESP

Le fichier SkyrimTTS_AutoWalk.esp doit etre active dans votre ordre de chargement. Deux methodes :

Via un gestionnaire de mods (Vortex, Mod Organizer 2) : l'ESP devrait apparaitre dans votre liste de plugins. Assurez-vous qu'il est active (coche).

Manuellement : ouvrez le fichier Data\plugins.txt (ou %LOCALAPPDATA%\Skyrim Special Edition\plugins.txt) et ajoutez la ligne :
*SkyrimTTS_AutoWalk.esp

Etape 4 : Lancer le jeu

1. Lancez NVDA
2. Lancez le jeu via skse64_loader.exe (pas le lanceur normal)
3. NVDA devrait commencer a lire les menus automatiquement

---

Premiers pas — Guide detaille

Nouveau sur Skyrim ? Lisez notre [Guide des premiers pas](GUIDE_FR.md) — un guide complet de la creation de personnage jusqu'a la quete de la Griffe d'or, avec chaque etape expliquee grace aux fonctionnalites d'accessibilite.

---

Creation de personnage

NVDA annonce : "Character creation"

- Ctrl + Gauche / Droite : changer d'onglet (Ethnie, Corps, Tete et sous-categories)
- Haut / Bas : naviguer dans les options de l'onglet
- Gauche / Droite : ajuster un curseur ou changer le sexe (Male/Female)
- R : confirmer / valider

---

Menu en croix (touche Tab)

NVDA annonce : "Cross menu"

- Haut : Competences
- Bas : Carte
- Gauche : Magie
- Droite : Inventaire

---

Inventaire

NVDA annonce : "Inventory open"

- Haut / Bas : changer d'objet (nom, valeur, poids vocalises)
- Gauche / Droite (ou Q / E) : changer de categorie
- H : annonce l'or et le poids actuel / maximum

---

Conteneur (coffre, corps...)

NVDA annonce : "Container open"

- Haut / Bas : changer d'objet
- Gauche / Droite : basculer entre votre inventaire et le conteneur
- H : annonce l'or et le poids

---

Menu Magie

NVDA annonce : "Magic menu open"

- Haut / Bas : changer de sort (nom, effets, cout vocalises)
- Gauche / Droite : changer de categorie (Destruction, Restauration...)

---

Journal (touche J)

NVDA annonce : "Journal open"

- Haut / Bas : changer de quete ou d'entree
- Entree : activer ou desactiver la quete selectionnee (NVDA annonce "active" ou "inactive")
- Ctrl + Gauche / Droite : changer d'onglet (Quetes, Systeme, etc.)

Quand vous activez ou desactivez une quete dans le journal, cela se reflete dans la categorie Quetes du scanner.

---

Menu Competences

- Haut / Bas / Gauche / Droite : naviguer dans l'arbre de competences
- La description de la competence selectionnee est vocalisee automatiquement
- Les talents sont vocalises avec leur description et leurs prerequis

---

Favoris (touche Q)

NVDA annonce : "Favorites"

- Haut / Bas : changer d'objet ou de sort

---

Dialogue

La ligne de dialogue du PNJ est vocalisee automatiquement.
Haut / Bas pour choisir votre reponse.

---

HUD (en jeu)

- Objet, PNJ ou porte dans le viseur : nom et action vocalises automatiquement (ex : "Ouvrir porte")
- Notifications (mises a jour de quete, montee de niveau, augmentation de competence) : vocalisees automatiquement
- Mises a jour d'objectifs de quete : le texte precis de l'objectif est lu (ex : "Trouvez la Griffe d'or"), pas juste "Quete mise a jour"
- Messages de ramassage : "Or ajoute", "Objet ajoute", etc.
- Sous-titres : vocalises automatiquement
- Nouveau lieu decouvert : vocalise automatiquement
- Statut de furtivite : annonce Cache / Detecte / Attention en mode furtif
- Bascule accroupi : annonce "Sneaking" / "Standing"
- Vue camera : annonce "First person" / "Third person" quand on appuie sur F
- Info fleches : type et nombre de fleches annonces quand on equipe un arc
- H : annonce Sante / Magicka / Vigueur actuelles
- Conseils de tutoriel : les indications de debut de jeu vocalisees avec les noms de touches

---

Menu principal

NVDA annonce : "Main menu open"

Haut / Bas pour naviguer : Nouvelle partie, Continuer, Charger, Parametres, Quitter.

---

Montee de niveau

NVDA annonce : "Level gained! Choose your improvement."

Gauche / Droite pour choisir entre Sante, Magicka ou Vigueur. Entree pour confirmer.

---

Boite de message

Les messages du jeu (confirmations, avertissements) sont vocalises automatiquement.
Haut / Bas pour naviguer entre les boutons, Entree pour confirmer.

---

Menu Marchand

NVDA annonce : "Barter menu open"

- Haut / Bas : changer d'objet (nom, valeur, poids, degats, armure, description vocalises)
- Gauche / Droite : changer de categorie ou basculer entre vendeur/joueur
- H : annonce l'or du joueur, l'or du vendeur et le poids

---

Menu Cadeau (Echange avec compagnon)

Vocalisation complete pour donner/prendre des objets avec les compagnons — memes controles que le conteneur.

---

Stations de crafting

Toutes les stations de crafting sont entierement accessibles :
- Forge et Tannerie : navigation par categorie avec Ctrl + Gauche/Droite, objets avec Haut/Bas
- Meule et Etabli : liste simple avec Haut/Bas
- Fonderie, Table d'enchantement, Laboratoire d'alchimie : vocalisation complete

Nom de la recette, quantite produite, materiaux requis et stats de degats/armure sont tous vocalises.

---

Menu Livre

A l'ouverture d'un livre, NVDA lit : le titre, le contenu complet, et tout sort ou competence appris.

---

Menu Entrainement

En parlant a un entraineur, NVDA lit : nom de la competence, niveau de l'entraineur, nombre d'entrainements, cout par session et votre or actuel. Mis a jour apres chaque session.

---

Menu Repos / Attente

NVDA lit : la question (repos ou attente ?), l'heure actuelle et les heures selectionnees en ajustant le curseur.

---

Ecran de chargement

Les astuces et conseils de chargement sont vocalises automatiquement pendant les ecrans de chargement.

---

Console de commande (~)

La console developpeur est maintenant accessible. Le texte tape et les resultats des commandes sont lus par NVDA. Utile pour des commandes avancees comme `setstage`.

---

Gemmes spirituelles

Les gemmes spirituelles dans l'inventaire/conteneur/marchand affichent leur niveau d'ame (ex : "Grand", "Common"). Les gemmes vides n'affichent pas de niveau d'ame.

---

Scanner d'objets

Le scanner vous permet de detecter et naviguer parmi tous les objets autour de vous : PNJs, portes, objets, conteneurs, cibles de quete, lieux, et plus.

Raccourcis du scanner (hors menus)

- Pave num. 5 : scanner les objets autour de vous
- Page Bas : objet suivant dans la categorie actuelle
- Page Haut : objet precedent dans la categorie actuelle
- Shift + Page Bas : categorie suivante
- Shift + Page Haut : categorie precedente
- Debut (Home) : annoncer l'objet actuel avec distance et orienter la camera vers lui
- Fin (End) : alterner les sous-categories (ex : portes verrouillees/deverrouillees, cadavres fouilles/non fouilles)

Categories du scanner

- Tous : chaque objet detecte
- PNJs : personnages vivants (amicaux et hostiles)
- Portes : toutes les portes, avec le nom de destination pour les portes de cellule
- Conteneurs : coffres, tonneaux, etc. (indique "empty" si fouille)
- Objets : armes, potions, livres, or, etc.
- Activateurs : leviers, boutons, lits, filons de minerai, etc. (les piliers d'enigme montrent leur symbole actuel)
- Cadavres : corps morts
- Compagnons : vos suiveurs
- Quetes : objectifs de quete actifs avec distance vers la cible (suit la direction de la boussole pour les cibles dans d'autres cellules)
- Lieux : marqueurs de carte decouverts a proximite avec leur type (Ville, Grotte, Fort, Ruines nordiques...)

Support des enigmes

Quand vous scannez des piliers d'enigme ou des anneaux de porte a griffe de dragon, le scanner montre le symbole actuel entre parentheses (ex : "Pillar (Snake)", "Inner ring (Bear)"). Pour les piliers, la direction (nord, sud...) est aussi indiquee pour identifier quel pilier est lequel.

Quand vous activez un pilier ou un anneau, NVDA annonce automatiquement le nouveau symbole.

---

Marche automatique

La marche auto vous permet de marcher automatiquement vers un objet du scanner ou une cible de quete.

Raccourcis de la marche auto

- Shift + Debut : demarrer la marche auto vers l'objet selectionne
- Shift + Debut a nouveau : arreter la marche auto
- Z / Q / S / D / Echap / Espace : arreter la marche auto immediatement

Fonctionnement

- Le joueur court automatiquement vers la cible en utilisant le systeme de pathfinding IA du jeu
- Pour les objectifs de quete dans une autre cellule (ex : interieur d'un donjon), la marche auto suit la boussole pour trouver la bonne porte d'entree
- Si le joueur est bloque pendant 20 secondes, la marche auto s'arrete avec "Can't reach target"
- La marche auto s'arrete automatiquement quand vous arrivez a 200 unites de la cible

---

Verrouillage d'ennemi (touche X)

Appuyez sur X pour tourner votre camera vers l'ennemi hostile le plus proche. NVDA annonce le nom et la distance de l'ennemi. Appuyez sur Shift + X pour verrouiller la camera en permanence sur l'ennemi. Shift + X a nouveau pour deverrouiller.

- Fonctionne uniquement en combat
- Ignore les ennemis morts, les compagnons et les acteurs desactives
- Vise le centre du corps de l'ennemi pour un combat de melee precis

---

Visee automatique (Arc)

Quand vous bandez votre arc (maintenez le bouton d'attaque), le plugin automatiquement :

1. Trouve l'ennemi hostile le plus proche
2. Le verrouille et vise le centre de son corps
3. Compense la gravite de la fleche selon la distance (utilise les vraies donnees physiques du jeu)
4. Predit le mouvement de l'ennemi pour viser ou la cible sera quand la fleche arrivera
5. Joue un bip (1000 Hz) quand vous avez la ligne de vue vers la cible
6. Re-vise toutes les 500ms pour suivre les cibles en mouvement

Quand vous relachez l'arc, le suivi s'arrete. Quand un ennemi est tue par le joueur (arc, melee, magie), un son de kill retentit (3 bips descendants).

Combat de dragon

- La visee auto priorise les dragons hostiles par rapport aux ennemis non-dragons plus proches (cerfs, renards, etc.)
- Bip aigu quand votre fleche touche un dragon
- Annonces vocales automatiques : "Dragon in flight" / "Dragon landed" pendant le combat de dragon

---

Menu Carte (touche M)

La carte est entierement accessible avec la navigation clavier.

Raccourcis de la carte

- Page Bas : marqueur suivant
- Page Haut : marqueur precedent
- Debut (Home) : annoncer les details complets du marqueur (type, distance, direction, disponibilite du voyage rapide)
- Shift + Debut : definir le marqueur actuel comme point de reference (toutes les distances recalculees depuis ce marqueur). Appuyez a nouveau pour effacer
- Fin (End) : alterner les filtres (Tous, Decouverts, Non decouverts, Cibles de quete)
- Entree (deux fois) : voyage rapide vers le marqueur selectionne (premiere pression demande confirmation, deuxieme confirme)

Fonctionnalites de la carte

- Tous les marqueurs listes avec nom, type (Ville, Grotte, Fort...), distance et direction (nord, sud, est...)
- Survol souris : quand vous deplacez la souris sur un marqueur, NVDA lit son nom
- Les filtres permettent de voir seulement les lieux decouverts, non decouverts ou les cibles de quete
- Le filtre Cibles de quete montre les objectifs de quete actifs comme marqueurs
- Voyage rapide via la touche Entree — pas besoin de cliquer visuellement sur la carte
- Point de reference : definir un marqueur comme reference pour mesurer les distances entre les lieux

---

Suivi de quete

Les objectifs de quete apparaissent dans la categorie Quetes du scanner. Seules les quetes activees dans votre journal sont affichees.

Fonctionnement

- Activez ou desactivez les quetes dans le journal (touche J, puis Entree sur une quete)
- Les quetes actives apparaissent dans la categorie Quetes du scanner avec distance et direction
- Pour les cibles dans une autre cellule (donjon, batiment), le scanner suit la direction de la boussole et montre la distance vers la porte a emprunter
- Utilisez Debut pour vous orienter vers la cible de quete
- Utilisez Shift + Debut pour la marche auto vers la cible de quete

Murs de mots

Les Murs de mots (ou vous apprenez les cris de dragon) apparaissent dans la categorie Activateurs du scanner. Approchez-vous pour apprendre le mot automatiquement.

---

Mods recommandes pour l'accessibilite

- [Puzzle Pillar Auto-Solve](https://www.nexusmods.com/skyrimspecialedition/mods/125875) — Tous les piliers sont pre-resolus, tirez simplement le levier
- [Dragon Claws Auto-Unlock](https://www.nexusmods.com/skyrimspecialedition/mods/47329) — Les portes a griffe s'ouvrent automatiquement quand vous avez la griffe

---

Resume des raccourcis clavier

En jeu (aucun menu ouvert)

- H : Sante / Magicka / Vigueur
- Pave num. 5 : scanner les objets
- Page Bas : objet suivant
- Page Haut : objet precedent
- Shift + Page Bas : categorie suivante
- Shift + Page Haut : categorie precedente
- Debut (Home) : annoncer l'objet actuel et orienter la camera
- Shift + Debut : demarrer / arreter la marche auto
- Fin (End) : alterner les sous-categories
- X : verrouiller l'ennemi le plus proche (combat uniquement)
- Bander l'arc : la visee auto s'active automatiquement

En inventaire / conteneur

- H : or et poids
- Haut / Bas : changer d'objet
- Gauche / Droite : changer de categorie ou basculer inventaire/conteneur

Sur la carte (touche M)

- Page Bas : marqueur suivant
- Page Haut : marqueur precedent
- Debut (Home) : details du marqueur
- Shift + Debut : definir / effacer le point de reference
- Fin (End) : alterner les filtres (Tous, Decouverts, Non decouverts, Cibles de quete)
- Entree (deux fois) : voyage rapide

Dans le journal (touche J)

- Haut / Bas : changer de quete
- Entree : activer / desactiver la quete

Creation de personnage

- Ctrl + Gauche / Droite : changer d'onglet
- Haut / Bas : naviguer dans les options
- Gauche / Droite : ajuster le curseur / changer le sexe
- R : confirmer

---

Support manette / Xbox / PlayStation

Support complet des manettes Xbox et PlayStation. LB est la touche modificateur pour toutes les combinaisons d'accessibilite. Vous pouvez reconfigurer quel bouton fait quoi dans la page MCM "Gamepad" si vous voulez changer les mappings par defaut.

Remappages automatiques faits par le plugin :

- Sprint : remappe de LB vers Stick gauche cliquable (aucune configuration manuelle necessaire)
- Furtivite : remappe de Stick gauche cliquable vers LB + Stick gauche cliquable
- Actions vanilla de LB dans les menus inventaire / conteneur / marchand : desactivees pour eviter les conflits avec nos combinaisons
- L'action vanilla du bouton Y (Favori) est temporairement desactivee pendant que LB est maintenu dans un menu d'objets, donc LB + Y ne declenche pas un favori par accident. Y tout seul continue de fonctionner normalement pour marquer les favoris.
- Sur la carte : l'inclinaison de camera via le joystick droit est desactivee (elle provoquait des lectures NVDA parasites des marqueurs survoles). Nos propres combinaisons sur le stick droit restent actives.
- La touche P (Place Player Marker) est desactivee sur la carte au clavier pour eviter le conflit avec notre propre systeme de marqueur personnalise.

Scanner (en jeu, LB comme modificateur) :

- LB + D-pad Bas : objet suivant (plus loin)
- LB + D-pad Haut : objet precedent (plus proche)
- LB + D-pad Gauche : annoncer la cible actuelle
- LB + Stick droit Gauche / Droite : changer de categorie (Tous, PNJ, Portes, etc.)
- LB + Stick droit Haut / Bas : changer de sous-filtre
- LB + A : demarrer / arreter la marche auto (detecte si vous etes a cheval et utilise automatiquement le mode monte)
- LB + B : teleporter vers la cible scannee
- LB + Y : stats contextuelles — en jeu annonce sante / magicka / vigueur ; dans un inventaire, conteneur ou marchand annonce l'or et le poids porte
- LB + Stick gauche cliquable : basculer la furtivite (accroupi / debout)
- LB + Stick droit cliquable : basculer la camera premiere / troisieme personne
- Stick droit cliquable seul : verrouiller l'ennemi le plus proche

Menu carte (manette) :

- LB + D-pad Bas / Haut : marqueur suivant / precedent dans le filtre actuel (trie par distance)
- LB + D-pad Gauche : annoncer les details du marqueur
- LB + D-pad Droite : definir le point de reference pour le calcul des distances
- LB + A : voyage rapide vers le marqueur selectionne (systeme custom, plus fiable que le vanilla)
- LB + Y : poser ou retirer un marqueur personnalise sur le marqueur actuellement selectionne — le marqueur devient une cible navigable dans le scanner en jeu
- LB + Stick droit Gauche / Droite : cycler le sous-filtre de lieux (Tous types, Villes, Bourgs, Donjons, Forts, Camps)
- LB + Stick droit Haut / Bas : cycler le filtre principal (Tous, Decouverts, Non decouverts, Cibles de quete)

Menus (message box, level up, etc.) a la manette :

- D-pad Gauche / Droite / Haut / Bas : naviguer dans les choix (oui/non pour les message boxes, sante/magicka/vigueur pour le level up)
- Bouton A : confirmer le choix selectionne
- Bouton B : annuler la message box
- Chaque changement de selection est vocalise automatiquement

Demarrer la marche auto a cheval : montez manuellement sur votre cheval d'abord, puis declenchez la marche auto (LB + A a la manette). Le plugin detecte que vous etes a cheval et c'est le cheval lui-meme qui marche vers la destination au lieu de vous forcer a descendre. Apres l'arret, il est possible que vous deviez descendre et remonter manuellement (E deux fois) pour recuperer le controle clavier / stick du cheval — c'est une particularite du moteur Skyrim qui affecte tous les mods utilisant ce systeme.

Le mouvement avec le stick gauche annule la marche auto — comme WASD au clavier.

---

*Plugin developpe par Pyrhame. Necessite NVDA.*
