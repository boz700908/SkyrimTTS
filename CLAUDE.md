# CLAUDE.md

Guide pour Claude Code sur le projet SkyrimNVDA.

## Apercu du projet

Plugin SKSE d'accessibilite pour Skyrim SE/AE/VR. Vocalise les menus du jeu via NVDA pour les joueurs aveugles. Utilise **nvdaController** (wchar_t natif) pour la synthese vocale.

**Menus couverts :** inventaire, conteneur, journal, magie, menu principal, level up, messagebox, tween (croix), dialogue, racesex, stats, HUD, tutoriel.

## Commandes de build

```bash
# Build (depuis Claude Code)
cmd.exe //c "C:\tmp\build_skyrim.bat"

# Le batch fait : vcvarsall x64 + cmake --build debug
```

Utilisez `/deploy` pour build + deploiement en une etape.
Utilisez `/check-logs` pour analyser les logs rapidement.

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

## Log

Le plugin ecrit dans `Data\SKSE\SkyrimNVDA.log`. Les menus geres (inventaire, magie, etc.) sont exclus du log generique des events pour eviter le spam. Seuls les menus non-geres sont logges.
