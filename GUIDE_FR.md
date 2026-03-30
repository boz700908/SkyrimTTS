# SkyrimNVDA — Guide des premiers pas

Un guide pas a pas pour les joueurs aveugles utilisant SkyrimNVDA et le mod d'accessibilite de Dio Kyrie. Ce guide couvre le jeu depuis le tout debut jusqu'a la quete de la Griffe d'or, en expliquant chaque etape avec les fonctionnalites d'accessibilite.

---

## Avant de commencer

Assurez-vous d'avoir :
- NVDA lance avant de demarrer le jeu
- Le jeu lance via `skse64_loader.exe` (pas le lanceur normal de Skyrim)
- Le plugin SkyrimNVDA et le mod d'accessibilite de Dio installes

Quand le jeu demarre, NVDA commence automatiquement a lire les menus et les elements du HUD.

---

## Reference rapide — Touches principales

### Navigation et scanner
| Touche | Action |
|--------|--------|
| Page Bas / Page Haut | Objet suivant / precedent dans le scanner |
| Shift + Page Bas / Haut | Changer de categorie du scanner |
| Fin | Alterner les sous-categories (ex : portes verrouillees/deverrouillees) |
| Debut (Home) | Annoncer les details de l'objet + orienter la camera vers lui |
| Shift + Debut | Demarrer / arreter la marche auto vers l'objet selectionne |
| Pave num. 5 | Scanner les objets dans la categorie actuelle |

### Combat
| Touche | Action |
|--------|--------|
| X | Verrouiller l'ennemi le plus proche (annonce nom + distance) |
| Maintenir attaque (arc) | La visee auto s'active — bip quand la cible est en vue |

### Menus
| Touche | Action |
|--------|--------|
| Tab | Ouvrir le menu en croix (Haut=Competences, Bas=Carte, Gauche=Magie, Droite=Inventaire) |
| H | Annoncer les stats (en jeu) ou or/poids (en inventaire) |
| Haut / Bas | Naviguer entre les objets, sorts, quetes |
| Gauche / Droite | Changer de categorie ou de cote (inventaire/conteneur) |

### Carte
| Touche | Action |
|--------|--------|
| Page Bas / Haut | Naviguer entre les marqueurs de la carte |
| Fin | Alterner les filtres (Tous, Decouverts, Non decouverts, Cibles de quete) |
| Entree (deux fois) | Voyage rapide vers le marqueur selectionne |
| Debut (Home) | Annoncer les details du marqueur |

### Creation de personnage
| Touche | Action |
|--------|--------|
| Ctrl + Gauche / Droite | Changer d'onglet (Ethnie, Corps, Tete...) |
| Haut / Bas | Naviguer dans les options de l'onglet |
| Gauche / Droite | Ajuster un curseur ou changer le sexe |
| R | Confirmer / valider |

---

## Partie 1 — La charette et creation de personnage

Le jeu commence avec votre personnage qui se reveille dans une charette de prisonniers, les mains liees, en route vers la ville d'Helgen. Vous etes prisonnier aux cotes d'autres captifs, dont Ralof (un rebelle Sombrerage) et Ulfric Sombrerage (chef de la rebellion).

**Vous n'avez aucun controle pendant cette sequence.** Ecoutez simplement les dialogues — ils posent l'histoire. Les soldats escortent les prisonniers vers Helgen pour les executer.

Quand la charette arrive a Helgen, un soldat imperial nomme Hadvar appelle les noms. Un prisonnier tente de fuir et est immediatement abattu par les archers. Vous etes ensuite mene vers le billot du bourreau — et c'est la que l'ecran de **creation de personnage** s'ouvre.

---

## Partie 2 — Creation de personnage

NVDA annonce : **"Character creation"** suivi des instructions de navigation. Vous devez creer votre personnage avant que l'histoire continue.

### Important — Comment fonctionne la creation de personnage

Les options du menu de creation de personnage **ne sont pas des elements a valider individuellement** — pas besoin d'appuyer sur Entree ou de confirmer chaque option. Naviguez simplement entre les onglets, placez votre curseur sur l'option souhaitee et ajustez-la. Rien n'est verrouille tant que vous n'appuyez pas sur **R**. Quand vous etes satisfait de vos choix sur tous les onglets, appuyez sur **R une seule fois** pour tout confirmer d'un coup.

### Comment naviguer

- **Ctrl + Droite / Gauche** : Changer d'onglet (Ethnie, Corps, Tete, et sous-categories comme Yeux, Bouche, Cicatrices...)
- **Haut / Bas** : Naviguer dans les options de l'onglet actuel
- **Gauche / Droite** : Ajuster les curseurs ou changer le sexe (Male/Female)
- **R** : Confirmer l'ensemble de votre personnage quand vous avez fini tous les onglets

### Que choisir

**Onglet Ethnie** : Choisissez votre race avec Haut/Bas. NVDA lit le nom de chaque race et sa description. Toutes les races sont viables — choisissez ce qui vous plait. Les Nordiques, Bretons et Rougegardes sont de bons choix pour les debutants.

**Onglet Corps** : La premiere option est le sexe (Male/Female), qu'on change avec Gauche/Droite. NVDA annonce "Male" ou "Female". Les autres curseurs controlent des details d'apparence comme le poids et la couleur de peau — ils sont purement visuels et n'affectent pas le gameplay, vous pouvez les ignorer.

**Onglet Tete et sous-categories** : Controle les traits du visage comme les cheveux, les yeux, les cicatrices, etc. C'est egalement purement visuel. Les valeurs sont des numeros (0, 1, 2...) representant differents presets visuels.

**Nom** : Apres avoir confirme avec R, vous entrez en mode saisie de nom. Tapez le nom de votre personnage et appuyez sur Entree pour confirmer.

### Conseil
Ne passez pas trop de temps ici — aucune option d'apparence n'affecte le gameplay. Choisissez une race, definissez votre sexe, tapez un nom et confirmez avec R.

---

## Partie 3 — L'attaque du dragon et l'evasion d'Helgen

Une fois votre personnage confirme, un immense dragon — **Alduin** — attaque la ville. Le chaos eclate avec du feu et de la destruction partout. **Ne touchez a rien** — attendez que la scene d'attaque du dragon se deroule et que votre personnage soit libre de bouger.

### Sauter l'intro (recommande)

La sequence d'intro necessite de sauter de toit en toit et de naviguer dans des batiments qui s'effondrent, ce qui est tres difficile sans voir. **Nous recommandons fortement de la sauter.**

Une fois que vous pouvez bouger, appuyez sur **L** pour ouvrir le menu d'accessibilite de Dio, puis selectionnez **"Walkthroughs"** → **"Main Quest Start"**. Le mod va automatiquement faire marcher votre personnage a travers toute la sequence d'intro — vous n'avez rien a faire, attendez simplement. A la fin, une boite de dialogue vous demandera de choisir entre **Hadvar** (Imperial) ou **Ralof** (Sombrerage). Les deux chemins sont presque identiques et le choix n'affecte pas significativement l'histoire principale. Selectionnez-en un et le walkthrough vous guidera au bon endroit.

Si vous ne sautez pas, vous devrez naviguer manuellement dans la sequence d'evasion scriptee, ce qui implique de suivre des PNJs a travers des batiments en flammes — c'est extremement difficile pour les joueurs aveugles.

### Choisir votre compagnon

Apres l'intro (ou apres l'avoir sautee), suivez le compagnon que vous avez choisi.

**Utiliser le scanner** : Appuyez sur **Page Bas / Page Haut** pour trouver les PNJs pres de vous. Utilisez **Shift + Page Bas** pour passer a la categorie **PNJs**. La touche Debut orientera votre camera vers le PNJ selectionne. Appuyez sur **Shift + Debut** pour marcher automatiquement vers lui.

### Entrer dans le fort

Suivez votre compagnon dans le Fort d'Helgen. A l'interieur, il coupe vos liens.

### Votre premier equipement

Votre compagnon vous dit de fouiller un corps proche pour vous equiper. Voici comment faire :

1. Appuyez sur **Shift + Page Bas** jusqu'a entendre la categorie **"Corpses"** (Cadavres)
2. Appuyez sur **Page Bas** pour trouver le cadavre le plus proche
3. Appuyez sur **Debut** pour le viser, puis **Shift + Debut** pour marcher vers lui
4. Quand vous etes assez pres, appuyez sur **E** pour interagir — cela ouvre le menu conteneur
5. NVDA lit : **"Container open"**
6. Utilisez **Haut / Bas** pour parcourir les objets (armes, armures)
7. Appuyez sur **E** ou **Entree** pour prendre les objets

Ouvrez maintenant votre inventaire (**Tab**, puis **Droite** pour Inventaire) et equipez votre nouvelle arme et armure.

### Premier combat — Apprendre a se battre

Peu apres, vous rencontrez vos premiers ennemis — deux soldats.

**Bases du combat de melee :**
- **Clic gauche** : Attaquer
- **Clic droit (maintenu)** : Bloquer
- **Touche X** : Tourne la camera vers l'ennemi hostile le plus proche — NVDA annonce le nom et la distance. **Shift + X** active le verrouillage permanent de la camera sur l'ennemi. **Shift + X** a nouveau pour desactiver le verrouillage.
- Marchez vers l'ennemi et attaquez. Votre compagnon se bat a vos cotes.

**Conseil** : La touche X est votre meilleur allie en combat. Elle vous dit qui est l'ennemi hostile le plus proche et tourne votre camera vers lui. Appuyez dessus regulierement pendant un combat pour rester oriente.

### Naviguer dans le fort

Le fort est une serie de salles et de couloirs. Utilisez le scanner pour naviguer :

Le plus simple est souvent de **suivre votre compagnon** (Ralof ou Hadvar) avec le scanner (categorie PNJs → Debut → Shift+Debut). S'il n'avance plus, cherchez l'objectif de quete dans la **categorie Quetes** du scanner et suivez-le avec l'autowalk. Vous pouvez aussi utiliser la **navigation audio** — un tac regulier qui vous guide vers l'objectif. Appuyez sur **O** pour activer ou desactiver ce guidage sonore.

Pour ouvrir les portes :
1. **Categorie Portes** (Shift + Page Bas jusqu'a "Doors") : Trouvez la prochaine porte
2. **Debut** : Orientez-vous vers la porte
3. **Shift + Debut** : Marche auto vers la porte
4. **E** : Ouvrir la porte

Vous traverserez plusieurs zones :
- **Une chambre de torture** avec d'autres ennemis
- **Des grottes naturelles** avec des Araignees givrees geantes (utilisez X pour les verrouiller, attaquez a distance si vous avez un arc)
- **Un ours endormi** — votre compagnon suggere de passer furtivement. Accroupissez-vous avec **Ctrl gauche** et passez lentement. NVDA annoncera "Sneaking". Si detecte, NVDA dit "Detected" — courez simplement.

### Sortie de la grotte

Vous atteignez enfin la sortie et emergez a l'exterieur. La quete "Sans entraves" est terminee. Vous etes libre d'explorer Bordeciel.

---

## Partie 4 — Rivebois

### S'y rendre

Votre compagnon suggere de se diriger vers Rivebois, un petit village proche. Vous pouvez :
- Suivre votre compagnon avec le scanner (categorie PNJs → Debut → Shift+Debut)
- Ou utiliser la categorie **Quetes** du scanner pour trouver la direction du marqueur de quete

### Navigation audio de Dio (Menu V)

Appuyez sur **V** pour acceder au menu d'accessibilite de Dio. Il fournit des outils de navigation supplementaires :
- **Son de Clairvoyance** : Une piste audio qui vous guide vers votre objectif de quete actuel. Suivez le son — il devient plus fort quand vous faites face a la bonne direction.
- **Touche O** : Activer/desactiver la navigation audio

La navigation audio et la marche auto se completent bien. Quand la marche auto ne trouve pas de chemin (terrain escarpe, zones complexes), utilisez le son pour marcher manuellement dans la bonne direction.

### Arrivee a Rivebois

Quand vous arrivez a Rivebois, NVDA annonce le nom du lieu. C'est votre premiere ville sure.

**Lieux importants** (utilisez le scanner pour les trouver) :
- **La forge d'Alvor** — Un forgeron avec des stations de crafting (forge, meule, etabli)
- **Le Commerce de Rivebois** — Un magasin general tenu par Lucan Valerius
- **L'Auberge du Geant Endormi** — Une auberge ou vous pouvez vous reposer

### Votre premier crafting (Optionnel — recommande plus tard)

Le crafting necessite des materiaux que vous n'aurez pas encore a ce stade du jeu. Nous le mentionnons ici pour que vous sachiez ou trouver les stations, mais il est conseille d'y revenir plus tard quand vous aurez collecte des ressources. Chez le forgeron, vous pouvez utiliser les stations de crafting. Quand vous en activez une (appuyez sur E en la visant), NVDA annonce le menu de crafting.

**Forge** (cree de nouveaux objets) :
- Utilisez **Ctrl + Gauche/Droite** pour changer de categorie (Armes, Armures, etc.)
- Utilisez **Haut/Bas** pour parcourir les recettes
- NVDA lit : nom de la recette, quantite produite, materiaux requis et stats de degats/armure

**Meule** (ameliore les armes) :
- Liste simple — utilisez **Haut/Bas** pour parcourir vos armes
- Appuyez sur Entree pour ameliorer l'arme selectionnee

**Etabli** (ameliore les armures) :
- Meme chose que la meule mais pour les pieces d'armure

---

## Partie 5 — Le Tertre des Chutes Glacees (Pierre de dragon et Griffe d'or)

Ce donjon est lie a deux quetes qui se font en meme temps :
- **La quete principale** : le mage Farengar a Fort-Dragon (Blancherive) vous demande de recuperer la **Pierre de dragon** dans le Tertre des Chutes Glacees
- **La Griffe d'or** : Lucan Valerius au Commerce de Rivebois vous demande de recuperer sa griffe doree volee, qui se trouve dans le meme donjon

Vous pouvez obtenir les deux quetes avant d'y aller, ou simplement suivre la quete principale. Dans tous les cas, vous recupererez les deux objets en explorant le donjon.

### Obtenir les quetes

**Quete principale (Pierre de dragon)** : Apres avoir parle au Jarl Balgruuf a Fort-Dragon, il vous envoie voir son mage **Farengar Secret-Fire**. Celui-ci vous demande de recuperer la Pierre de dragon au Tertre des Chutes Glacees.

**Quete optionnelle (Griffe d'or)** : Entrez dans le **Commerce de Rivebois** (utilisez le scanner → categorie Portes pour le trouver). Parlez a **Lucan Valerius**. Il explique que des voleurs ont vole un ornement en forme de griffe doree. Le voleur, Arvel le Rapide, s'est enfui vers le Tertre des Chutes Glacees. Lucan offre une recompense si vous la recuperez.

Les deux quetes apparaitront dans votre journal (**Tab → Bas pour la Carte, ou J** pour l'ouvrir directement).

### Se rendre au Tertre des Chutes Glacees

Depuis Rivebois, dirigez-vous vers le nord-ouest en montant la montagne. Vous pouvez :
- Utiliser la categorie **Quetes** du scanner pour trouver la direction
- Utiliser **Shift + Debut** pour la marche auto vers le marqueur de quete
- Si la marche auto galere dans le chemin de montagne, utilisez la **navigation audio** (touche O) et marchez manuellement
- Le chemin monte — si vous entendez "Can't reach target", essayez de marcher manuellement en suivant le son, puis relancez la marche auto

### Devant le tertre — Bandits

A l'entree, **3 bandits** gardent la zone.
1. Appuyez sur **X** pour verrouiller l'ennemi le plus proche
2. Si vous avez un arc, bandez-le (maintenez attaque) — le bip de visee auto guide votre tir
3. Pour le corps a corps, appuyez sur **X** puis marchez vers l'ennemi et attaquez
4. Le **son de kill** (3 bips descendants) confirme chaque elimination

### A l'interieur — Premieres salles

Entrez dans le tertre. D'autres bandits a l'interieur, generalement 2 dans la premiere grande salle. Utilisez X pour les trouver et les combattre.

En allant plus profond, vous trouverez un bandit mort pres d'un piege. Il y a des pieges a flechettes empoisonnees declenches par un levier — soyez prudent.

### L'enigme des piliers

Vous atteindrez une salle avec **trois piliers rotatifs en pierre** et une grille verrouillee avec un levier.

**Important** : Si vous avez installe le mod recommande **Puzzle Pillar Auto-Solve**, les piliers sont deja correctement positionnes. Tirez simplement le levier (trouvez-le avec le scanner → categorie Activateurs → E pour tirer).

Sans le mod, la solution est : **Serpent, Serpent, Baleine** (de gauche a droite). Utilisez la categorie Activateurs du scanner pour trouver chaque pilier. Notre mod lit le symbole actuel entre parentheses (ex : "Pillar (Snake)"). Activez le pilier avec E pour le tourner jusqu'au bon symbole.

### Le boss araignee

Plus profond, vous rencontrez une grande **Araignee givree** — un mini-boss. Elle a piege quelqu'un dans ses toiles.

1. Appuyez sur **X** pour verrouiller l'araignee
2. Utilisez des attaques a distance si possible (l'arc avec visee auto marche tres bien ici)
3. Gardez vos distances — l'araignee peut vous empoisonner
4. Le son de kill confirme sa mort

### Trouver la Griffe d'or

Apres avoir tue l'araignee, vous trouvez **Arvel le Rapide** — le voleur. Il est peut-etre mort ou vous devrez le combattre. Dans tous les cas, fouillez son corps :

1. Scanner → categorie **Cadavres** → trouvez Arvel
2. **E** pour ouvrir et piller — prenez la **Griffe d'or**

### Les Draugrs

En allant plus profond, vous rencontrez des **Draugrs** — des guerriers nordiques morts-vivants. Ils utilisent des armes de melee et certains peuvent crier.

- Appuyez sur **X** pour verrouiller chaque draugr
- Combattez-les un par un si possible
- Utilisez des objets de soin entre les combats (ouvrez l'inventaire → naviguez vers les potions)

### La porte de la Griffe d'or

Vous atteindrez une porte avec trois anneaux rotatifs et un trou de serrure en forme de griffe.

**Si vous avez le mod Dragon Claws Auto-Unlock** : Activez simplement la porte et elle s'ouvre automatiquement puisque vous avez la griffe dans votre inventaire.

**Sans le mod** : La solution est gravee sur la griffe elle-meme. Les symboles de haut en bas sur la Griffe d'or sont : **Ours, Papillon, Hibou**. Notre scanner lit les symboles des anneaux (ex : "Inner ring (Bear)"). Activez chaque anneau pour le tourner au bon symbole, puis activez le trou de serrure.

### Le Mur de mots et le boss final

Apres la porte de la griffe, vous entrez dans une grande salle avec un **Mur de mots** — un ancien mur de pierre couvert de runes lumineuses.

1. Utilisez le scanner → categorie **Activateurs** pour trouver le Mur de mots
2. Marchez vers lui — en vous approchant, vous apprenez automatiquement le Mot de puissance "Fus" (Force), le premier mot du cri Deferlante
3. Cela declenche le **boss final** : un **Seigneur Draugr** jaillit d'un cercueil

**Combattre le Seigneur Draugr :**
- Appuyez immediatement sur **X** pour le verrouiller
- Attaquez rapidement pendant qu'il se releve du cercueil
- Attention : il peut utiliser le cri Deferlante pour vous projeter au sol
- Utilisez des potions de soin abondamment
- Le son de kill confirme la victoire

4. Pillez le corps du Seigneur — il porte la **Pierre de dragon** (importante pour la quete principale plus tard)

### Sortir du tertre

Apres le boss, suivez le chemin pour trouver une sortie. Utilisez le scanner → Portes pour trouver le chemin. Vous emergerez sur le flanc de la montagne.

---

## Partie 6 — Rendre la griffe et la Pierre de dragon

### Retour a Rivebois (Griffe d'or)

Voyagez vers Rivebois :
- Ouvrez la **Carte** (Tab → Bas)
- Utilisez **Page Bas/Haut** pour trouver "Rivebois"
- Appuyez sur **Entree** deux fois pour le voyage rapide

Entrez dans le Commerce de Rivebois et parlez a **Lucan Valerius**. Il est ravi d'avoir la griffe et vous recompense en or. Quete de la Griffe d'or terminee !

---

## Partie 7 — Vers Blancherive et au-dela

### Prochaine destination : Blancherive

Votre quete principale vous dirige maintenant vers **Blancherive**, la grande ville au nord-est. Vous devez prevenir le Jarl de l'attaque du dragon sur Helgen.

Voyagez-y :
- Ouvrez la **Carte** → trouvez "Blancherive" (ou "Ecuries de Blancherive")
- **Entree** deux fois pour le voyage rapide
- Ou marchez en utilisant le marqueur de quete (categorie Quetes dans le scanner)

### Fort-Dragon

### Rendre la Pierre de dragon

Voyagez vers Blancherive et dirigez-vous vers **Fort-Dragon**. Parlez a **Farengar Secret-Fire** et remettez-lui la Pierre de dragon. La quete principale avance.

**Felicitations !** Vous avez termine l'arc d'ouverture de Skyrim. A partir d'ici, la quete principale continue avec des rencontres de dragons et la decouverte que vous etes l'Enfant de Dragon — mais le monde entier de Bordeciel est ouvert a votre exploration.

---

## Conseils generaux

### Marche auto et navigation audio
- La **marche auto** (Shift + Debut) fonctionne bien sur terrain plat et a l'interieur des batiments
- Quand la marche auto dit "Can't reach target", passez a la **marche manuelle avec navigation audio** (touche O pour le son de Clairvoyance)
- Les deux systemes se completent — utilisez la marche auto pour les chemins faciles et la navigation audio pour les terrains complexes

### Conseils de combat
- Appuyez toujours sur **X** avant d'engager les ennemis pour savoir qui et ou ils sont
- Pour le combat a distance, equipez un arc — le systeme de **visee auto** gere le ciblage automatiquement
- Le **bip** de visee auto vous dit quand vous avez la ligne de vue — relacher votre fleche quand vous l'entendez
- **Dragons** : La visee auto priorise les dragons par rapport aux autres ennemis, et vous entendrez les annonces "Dragon in flight" / "Dragon landed"
- **Son de kill** : 3 bips descendants confirment chaque elimination d'ennemi

### Conseils d'exploration
- Utilisez les categories du scanner strategiquement : PNJs pour trouver les gens, Portes pour naviguer, Objets pour trouver le butin
- La categorie **Lieux** montre les marqueurs de carte decouverts a proximite — utile pour trouver les villes
- Appuyez sur **H** a tout moment pour verifier votre Sante, Magicka et Vigueur
- En inventaire, appuyez sur **H** pour votre or et votre poids

### Conseils de crafting
- Visitez une forge pour creer de nouvelles armes et armures a partir de materiaux bruts
- Utilisez une meule pour affuter les armes (augmente les degats)
- Utilisez un etabli pour ameliorer les armures (augmente la valeur d'armure)
- Les recettes montrent les materiaux requis — vous devez les avoir dans votre inventaire

### Carte et voyage rapide
- Ouvrez la carte avec **Tab → Bas** ou **M**
- Naviguez entre les marqueurs avec **Page Bas / Haut**
- Utilisez **Fin** pour filtrer : Tous, Decouverts seulement, Non decouverts seulement, ou Cibles de quete seulement
- Appuyez sur **Entree** deux fois sur un lieu decouvert pour y voyager rapidement
- Vous ne pouvez voyager rapidement que vers des lieux deja visites
- Utilisez **Shift + Debut** pour definir un point de reference et mesurer les distances entre marqueurs
