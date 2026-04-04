# CLAUDE.md

Guide pour Claude Code sur le projet SkyrimNVDA.

## Apercu du projet

Plugin SKSE d'accessibilite pour Skyrim SE/AE/VR. Vocalise les menus du jeu via NVDA pour les joueurs aveugles. Utilise **nvdaController** (wchar_t natif) pour la synthese vocale.

**Menus couverts :** inventaire, conteneur, marchand, journal, magie, menu principal, level up, messagebox, tween (croix), dialogue, racesex, stats, HUD, tutoriel, carte, crafting (forge, meule, tannerie, etabli).

**Systemes couverts :** scanner d'objets (10 categories), autowalk, visee auto (arc), verrouillage ennemi, suivi de quetes, furtivite, puzzles (piliers/anneaux).

## Commandes de build

```bash
# Build (depuis Claude Code)
cmd.exe //c "C:\tmp\build_skyrim.bat"

# Le batch fait : vcvarsall x64 + cmake --build debug
```

Utilisez `/deploy` pour build + deploiement en une etape.
Utilisez `/check-logs` pour analyser les logs rapidement.

```bash
# Decompiler un SWF (extraire les scripts ActionScript)
"C:/Program Files (x86)/FFDec/ffdec.bat" -export script "<dossier_sortie>" "<fichier.swf>"

# Exemple :
"C:/Program Files (x86)/FFDec/ffdec.bat" -export script "C:/Users/marcd/source/repos/SkyrimNVDA/UI/interface/sleepwaitmenu" "C:/Users/marcd/source/repos/SkyrimNVDA/UI/interface/sleepwaitmenu.swf"
```

```powershell
# Extraire les SWF d'un fichier BSA (utilise Sharp.BSA.BA2.dll de BSA Browser)
# Les SWF extraits vont dans UI/bsa_swf/
powershell.exe -Command "
[System.Reflection.Assembly]::LoadFrom('C:\Program Files (x86)\BSA Browser\Sharp.BSA.BA2.dll') | Out-Null
\$bsa = New-Object SharpBSABA2.BSAUtil.BSA '<chemin_vers_fichier.bsa>'
\$outDir = 'C:\Users\marcd\source\repos\SkyrimNVDA\UI\bsa_swf'
foreach (\$f in \$bsa.Files) {
    if (\$f.FullPath -match '\.swf$') {
        \$name = [System.IO.Path]::GetFileName(\$f.FullPath)
        \$stream = \$f.GetDataStream()
        \$fs = [System.IO.File]::Create((Join-Path \$outDir \$name))
        \$stream.CopyTo(\$fs); \$fs.Close(); \$stream.Close()
    }
}
"
```

## Architecture

### Structure du projet

```
SkyrimNVDA/
├── src/           Sources C++ (plugin.cpp, common.h, menu_*.h)
├── lib/           Librairies (nvdaControllerClient.dll/.lib)
├── gfx/           Headers Scaleform GFx SDK
├── UI/            Fichiers SWF et ActionScript decompiles
├── CMakeLists.txt
├── CMakePresets.json
├── vcpkg.json
└── build/         (genere par cmake)
```

### Fichiers cles

- **src/plugin.cpp** : Point d'entree. MenuListener (open/close), InputListener (clavier), enregistrement des hooks.
- **src/common.h** : Fonctions partagees (Speak, SpeakQueue, traduction, GFx helpers, markup stripping).
- **src/menu_*.h** : Un fichier par menu. Chacun contient : snapshot, lecture GFx, annonce vocale, polling.

### Dependances

- **CommonLibSSE-NG** (via vcpkg) : API reverse-engineered de Skyrim
- **nvdaControllerClient** (lib/.dll/.lib) : Communication directe avec NVDA en wchar_t

## Regles de speech

### Speak() vs SpeakQueue()

```cpp
// Speak() = cancelSpeech + speakText : interrompt et lit immediatement
// Utiliser pour la navigation (changement d'item, de categorie)
Speak(itemName);

// SpeakQueue() = speakText seul : s'enchaine sans interrompre
// Utiliser pour les infos secondaires (description, effets, cout)
SpeakQueue(description);
```

### Pattern firstRead (ouverture de menu)

A l'ouverture d'un menu, la premiere lecture doit utiliser `SpeakQueue` pour ne pas couper l'annonce d'ouverture :

```cpp
// Dans plugin.cpp :
Speak(L"Inventory open");
QueueInventoryRead();
StartInventoryPolling();

// Dans AnnounceXxxChangeImpl() :
const bool firstRead = g_lastXxx.empty();
if (firstRead) SpeakQueue(item); else Speak(item);
```

## Erreurs courantes

### 1. Speak() qui coupe une annonce precedente
```cpp
// MAUVAIS - coupe "Menu open"
Speak(L"Menu open");
Speak(L"Premier item");  // cancelSpeech() tue "Menu open"

// BON - s'enchaine
Speak(L"Menu open");
SpeakQueue(L"Premier item");
```

### 2. Logging dans le polling
```cpp
// MAUVAIS - flood le log a chaque cycle (80ms)
LOG("polling tick");

// BON - logger seulement les changements d'etat
if (item != g_lastItem) {
    LOG("item changed: {}", item);
}
```

### 3. Oublier le flood protection
```cpp
// MAUVAIS - plusieurs lectures empilees
QueueXxxRead();

// BON - une seule en file
if (g_xxxPendingRead.exchange(true)) return;
```

### 4. GFx sans verification
```cpp
// MAUVAIS
RE::GFxValue v;
movie->GetVariable(&v, "path");
std::string s = v.GetString();  // crash si undefined

// BON
RE::GFxValue v;
if (movie->GetVariable(&v, "path") && v.IsString()) {
    std::string s = v.GetString();
}
```

## Traduction (systeme hybride)

1. **BSScaleformTranslator** (priorite) : traductions du moteur + mods
2. **Fichier Translate_*.txt** (fallback) : parsing manuel au demarrage
3. **Fallback** : strip $ et remplace _ par espaces

```cpp
// Toujours utiliser ResolveUIString() pour les textes UI
std::wstring text = ResolveUIString(movie, rawString);
```

## Reactivite clavier

Chaque menu doit avoir un declencheur clavier dans InputListener (plugin.cpp) **en plus** du polling :

```cpp
if (g_xxxOpen.load(std::memory_order_relaxed)) {
    if (navKey) QueueXxxRead();
}
```

Le polling (80ms) est un backup pour gamepad/souris.

## Protection contre le flooding

```cpp
static std::atomic_bool g_xxxPendingRead{false};

static void QueueXxxRead() {
    if (g_xxxPendingRead.exchange(true)) return;  // une seule en file
    auto* task = SKSE::GetTaskInterface();
    task->AddUITask([]() {
        g_xxxPendingRead.store(false);
        AnnounceXxxChangeImpl();
    });
}
```

## Normalisation du texte

`NormalizeForSpeech()` convertit les caracteres typographiques (guillemets courbes, tirets longs, ellipses) en ASCII standard. Ne pas supprimer les apostrophes — NVDA les gere nativement en wchar_t.

## Agents disponibles

- **commonlibsse-api-analyst** : Utiliser quand on a besoin de comprendre une classe ou fonction de CommonLibSSE-NG (RE::, SKSE::). Fouille les headers dans `build/debug/vcpkg_installed/`.
- **skyrim-ui-explorer** : Utiliser quand on doit trouver des chemins GFx dans un menu SWF. Analyse les fichiers ActionScript decompiles dans `UI/`.
- **accessibility-reviewer** : Utiliser pour relire le code avant un commit ou apres avoir code un nouveau menu. Verifie les regles Speak/SpeakQueue, flood protection, GFx safety, etc.
- **log-analyzer** : Utiliser pour analyser en profondeur le fichier `SkyrimNVDA.log` quand un probleme survient.

## Regles de travail

### Toujours verifier avant de tester
Apres chaque modification importante, relire le code pour verifier qu'il n'y a pas d'erreur avant de demander a l'utilisateur de tester. Ne pas envoyer du code non verifie.

### Changelog
Mettre a jour `CHANGELOG.md` apres chaque ajout ou correction significative. Ne pas inclure les corrections de bugs sur des fonctionnalites en cours de developpement — seulement sur des fonctionnalites deja livrees dans une version precedente. Exemple : si on ajoute le crafting dans la v1.1 et qu'on corrige un bug du crafting avant de sortir la v1.1, ne pas mettre cette correction dans le changelog — les joueurs n'ont jamais eu le crafting buggue.

### Ne pas casser ce qui marche
Quand on corrige un bug sur un atelier (meule, tannerie), verifier que la forge continue de fonctionner. Chaque atelier a son propre mode de lecture (forge = categories, meule = liste simple, tannerie = categories).

### Crafting : deux modes
- **Mode forge** (CategoryList) : forge, tannerie — avec categories et items. `ReadCraftingSnapshotForge()`.
- **Mode simple** (ItemListTweener) : meule, etabli — liste d'items directe. `ReadCraftingSnapshotSimple()`.
- Detection automatique via `CategoryList.currentState` au demarrage.

### Furtivite
Suspendre la vocalisation de la furtivite (Hidden/Detected/Caution) quand le Crafting Menu est ouvert pour ne pas couper les tutoriels et annonces.

## Pathfinding custom (src/pathfinding.h)

Systeme de navigation A* sur navmesh, parallele a l'autowalk classique (autowalk.h).

- **Ctrl+Home** = pathfinding A* (nouveau), **Shift+Home** = autowalk classique (inchange)
- Lit les navmeshes du jeu (`BSNavmesh`, `BSNavmeshGrid`, `BSNavmeshTriangle`)
- A* sur graphe de triangles avec portails inter-navmesh (`extraEdgeInfo`)
- Lissage de chemin (string-pulling), ouverture de portes, saut auto, annonces vocales
- 5 niveaux de recuperation en cas de blocage
- Les deux systemes sont mutuellement exclusifs (lancer l'un stoppe l'autre)
- L'indexation `extraEdgeInfo` (portails) doit etre verifiee empiriquement au premier test

## GitHub Actions (CI/CD)

Release automatisee via `.github/workflows/build-and-release.yml`.

### Publier une release
```bash
git tag v1.3
git push origin v1.3
```
GitHub compile en Release, verifie la taille DLL (anti-Debug), cree le zip (structure MO2), et publie un brouillon de release.

### Structure du zip (sans dossier Data/)
```
SKSE/Plugins/SkyrimNVDA.dll
SKSE/Plugins/nvdaControllerClient.dll
SKSE/Plugins/SkyrimNVDA.ini
Scripts/SkyrimTTS_AutoWalk.pex
Scripts/SkyrimTTS_MQ105Fix.pex
Sound/fx/SkyrimTTS/*.wav
SkyrimTTS_AutoWalk.esp
fomod/info.xml
CHANGELOG.txt, README.txt, GUIDE.txt, GUIDE_FR.txt, LISEZMOI.txt
```

### Commits atomiques
Faire un commit par changement logique (un bug = un commit, une feature = un commit). Ne pas regrouper.

### Fichiers interdits dans le repo
Ne JAMAIS commiter de fichiers game assets ou binaires dans le repo Git :
- `.swf` (fichiers Flash du jeu)
- `.as` (scripts ActionScript decompiles)
- `.wav` (sons)
- Dossiers `UI/`, `gfx/`, `release/`, `sounds/`
- Les releases vont dans GitHub Releases, pas dans le repo
Le `.gitignore` est configure pour bloquer ces fichiers. Ne pas le modifier pour les inclure.

### Release : TOUJOURS en Release build
La DLL Release fait ~1 Mo, la Debug ~5 Mo. La Debug depend de DLL developpeur (MSVCP140D.dll) que les joueurs n'ont pas. Le CI verifie automatiquement la taille.

## Log

Le plugin ecrit dans `Data\SKSE\SkyrimNVDA.log`. Les menus geres (inventaire, magie, etc.) sont exclus du log generique des events pour eviter le spam. Seuls les menus non-geres sont logges.
