# AutoWalk - Guide d'integration C++

## Fichiers fournis (dossier `autowalk/`)

| Fichier | Role |
|---------|------|
| `SkyrimTTS_AutoWalk.esp` | Plugin ESP a copier dans Skyrim `Data/` |
| `SkyrimTTS_AutoWalk.pex` | Script compile a copier dans `Data/Scripts/` |
| `SkyrimTTS_AutoWalk.psc` | Source Papyrus (reference, pas besoin en jeu) |

## Comment ca marche

L'ESP contient 3 records :

1. **QUST** `SkyrimTTS_AutoWalkQuest` (FormID 0x800) — Start Game Enabled
   - Alias 0 : `DstMarker` (vide, optionnel) — la destination dynamique
   - Alias 1 : `Traveler` (= PlayerRef) — avec le Travel package attache

2. **PACK** `SkyrimTTS_TravelPackage` (FormID 0x801) — template Travel de Skyrim
   - Destination = alias 0 (DstMarker) de la quete
   - Pathfinding navmesh complet (evitement obstacles, vrais pas)
   - Vitesse : Run (course)

3. **SCEN** `SkyrimTTS_WalkScene` (FormID 0x802) — present mais pas utilise
   - On utilise ALPC (package attache a l'alias) au lieu de la scene

## Fonctions Papyrus a appeler depuis le C++

### Demarrer la marche
```
OnWalkToTarget(int aiFormID, float afStopDistance)
```
- `aiFormID` : FormID de la reference cible (porte, PNJ, coffre, etc.)
- `afStopDistance` : distance d'arret en unites (100.0 = ~1.5m)
- La cible DOIT etre un ObjectReference existant dans le monde (sur/pres du navmesh)

### Arreter la marche
```
OnStopWalking()
```

## Comment appeler depuis le C++ (SKSE)

Utiliser `DispatchMethodCall` ou `SendPapyrusEvent` pour appeler les fonctions sur la quete.

### Trouver la quete
```cpp
// Par EditorID
auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SkyrimTTS_AutoWalkQuest");

// Ou par FormID (index de plugin variable, utiliser LookupByEditorID de preference)
```

### Appeler OnWalkToTarget
```cpp
// Via le systeme de scripts SKSE
auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
auto args = RE::MakeFunctionArguments(
    (int)targetFormID,      // FormID de la cible
    (float)stopDistance     // Distance d'arret
);
vm->DispatchMethodCall(
    questHandle,            // Handle de la quete
    "SkyrimTTS_AutoWalk",   // Nom du script
    "OnWalkToTarget",       // Nom de la fonction
    args.get(),
    nullptr                 // Callback (optionnel)
);
```

### Appeler OnStopWalking
```cpp
auto args = RE::MakeFunctionArguments();
vm->DispatchMethodCall(
    questHandle,
    "SkyrimTTS_AutoWalk",
    "OnStopWalking",
    args.get(),
    nullptr
);
```

## Flux de fonctionnement

```
C++ detecte cible (scanner)
    → OnWalkToTarget(formID, 100.0)
        → Script: DstMarker.ForceRefTo(target)
        → Script: SetPlayerAIDriven(true)
        → Script: EvaluatePackage()
        → L'IA du joueur prend le Travel package
        → Le joueur marche sur le navmesh vers la cible
        → Arrive : auto-stop + rendu du controle

C++ veut arreter
    → OnStopWalking()
        → Script: DstMarker.Clear()
        → Script: SetPlayerAIDriven(false)
        → Controle rendu au joueur
```

## Arret automatique

Le script arrete la marche automatiquement si :
- Le joueur arrive a destination (distance <= stopDistance + 20)
- Le joueur entre en combat
- La cible devient invalide (supprimee/desactivee)

## Deploiement

Les fichiers doivent etre copies dans le dossier Data de Skyrim :
```
Skyrim Special Edition/Data/SkyrimTTS_AutoWalk.esp
Skyrim Special Edition/Data/Scripts/SkyrimTTS_AutoWalk.pex
```

L'ESP doit etre active dans le load order (plugins.txt ou via un mod manager).

## Generer l'ESP (si modification necessaire)

L'ESP est genere par le projet `SkyrimCK-MCP` :
```
cd c:\Users\marcd\source\repos\SkyrimCK-MCP
dotnet run --project src/SkyrimCkMcp.csproj
```
Le fichier est genere dans `output/SkyrimTTS_AutoWalk.esp`.

Pour recompiler le script Papyrus :
```
"C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition 1946180\Papyrus Compiler\PapyrusCompiler.exe" scripts\SkyrimTTS_AutoWalk.psc -i="scripts;C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\Scripts\Source;C:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition 1946180\Data\Source\Scripts" -o="scripts" -f="TESV_Papyrus_Flags.flg"
```
