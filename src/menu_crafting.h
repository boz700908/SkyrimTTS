#pragma once

// VOCALISATION MENU CRAFTING (forge, meule, établi, fonderie, tannage) - DEBUT

static std::atomic_bool g_craftingOpen{false};
static std::atomic_bool g_craftingPendingUIRead{false};
static std::jthread     g_craftingPollThread;
static std::wstring     g_lastCraftingCat;
static std::wstring     g_lastCraftingItemAnnounce;
static std::wstring     g_lastCraftingDesc;
static std::atomic_bool g_craftingFirstReadDone{false};
// Mis à true par l'input handler clavier (Up/Down/W/S etc.) pour forcer la relecture
// même quand l'item résultant est textuellement identique (recettes dupliquées).
static std::atomic_bool g_craftingForceAnnounce{false};
// Slider d'equilibrage a l'autel d'enchantement : equilibre nombre de charges
// utilisables vs duree/magnitude de l'effet. Visible UNIQUEMENT a l'autel
// quand on confectionne un enchantement (apres avoir choisi arme + effet +
// gemme). Detecte via EnchantingSlider_mc._alpha > 0.
static bool             g_craftingEnchantSliderOpen{false};
static int              g_lastEnchantSliderValue{-1};

struct CraftingSnapshot {
    std::wstring itemText;
    std::wstring catText;
    std::wstring descText;
    std::wstring ingredientsText;  // matériaux requis
    std::wstring valueText;
    std::wstring weightText;
    std::wstring weaponDamageText;
    std::wstring apparelArmorText;
    std::wstring enchantmentText;  // effets enchantements (ex: "Carry Weight +50") pour sacs à dos modés et items enchantés
    bool         inCategoryMode{true};  // true = dans les catégories, false = dans les items
};

// Reformate la chaîne SkyUI des matériaux requis pour clarifier "requis vs possédé".
//
// Format SkyUI brut (UpdateIngredients) : "Required: [N ]Name[ (M)], [N ]Name[ (M)], ..."
//   - N (préfixe) = quantité requise. ABSENT si requis = 1 (vanilla SkyUI design).
//   - (M) (suffixe) = quantité possédée par le joueur. Présent seulement si M >= 1.
//
// Exemples bruts → reformulés :
//   "Required: Leather"          → "Required: 1 Leather"
//   "Required: Leather (5)"      → "Required: 1 Leather, you have 5"
//   "Required: 4 Leather (2)"    → "Required: 4 Leather, you have 2"
//   "Required: 4 Leather, Iron Ingot (5)" → "Required: 4 Leather, 1 Iron Ingot, you have 5"
//
// Sans cette reformulation, "Leather (5)" laisse croire que 5 cuirs sont requis
// alors que c'est l'inventaire du joueur — bug rapporté par les utilisateurs aveugles.
static std::wstring ReformatCraftingIngredients(const std::wstring& raw) {
    if (raw.empty()) return raw;

    // Séparer label (ex: "Required") du reste après le premier ":"
    size_t colon = raw.find(L':');
    std::wstring label;
    std::wstring body;
    if (colon != std::wstring::npos) {
        label = raw.substr(0, colon);
        body = raw.substr(colon + 1);
        // trim espaces de tête du body
        while (!body.empty() && body.front() == L' ') body.erase(body.begin());
    } else {
        body = raw;
    }

    // Découper sur ", "
    std::vector<std::wstring> tokens;
    {
        size_t pos = 0;
        while (pos < body.size()) {
            size_t next = body.find(L", ", pos);
            if (next == std::wstring::npos) {
                tokens.push_back(body.substr(pos));
                break;
            }
            tokens.push_back(body.substr(pos, next - pos));
            pos = next + 2;
        }
    }

    // Pour chaque token : extraire (N requis, Name, M possédé)
    std::wstring result;
    if (!label.empty()) result = label + L": ";
    for (size_t i = 0; i < tokens.size(); ++i) {
        std::wstring tok = tokens[i];
        // trim
        while (!tok.empty() && tok.front() == L' ') tok.erase(tok.begin());
        while (!tok.empty() && tok.back()  == L' ') tok.pop_back();
        if (tok.empty()) continue;

        // Extraire (M) à la fin si présent
        std::wstring owned;
        if (!tok.empty() && tok.back() == L')') {
            size_t openParen = tok.rfind(L" (");
            if (openParen != std::wstring::npos) {
                std::wstring inside = tok.substr(openParen + 2, tok.size() - openParen - 3);
                bool allDigits = !inside.empty();
                for (wchar_t c : inside) if (c < L'0' || c > L'9') { allDigits = false; break; }
                if (allDigits) {
                    owned = inside;
                    tok = tok.substr(0, openParen);
                }
            }
        }

        // Extraire N préfixe si présent (chiffres + espace)
        std::wstring required = L"1";
        size_t firstSpace = tok.find(L' ');
        if (firstSpace != std::wstring::npos) {
            std::wstring maybeNum = tok.substr(0, firstSpace);
            bool allDigits = !maybeNum.empty();
            for (wchar_t c : maybeNum) if (c < L'0' || c > L'9') { allDigits = false; break; }
            if (allDigits) {
                required = maybeNum;
                tok = tok.substr(firstSpace + 1);
            }
        }

        // Composer le token reformaté
        if (i > 0) result += L", ";
        result += required + L" " + tok;
        if (!owned.empty())
            result += L", you have " + owned;
    }

    return result;
}

// Lit le texte des effets/enchantements depuis l'ItemCard du crafting.
// Couvre 3 cas : forge (enchantement de l'objet a ameliorer), hotel
// d'enchantement (description de l'effet selectionne), table d'alchimie
// (effets de la potion a fabriquer). Le snapshot stocke tout dans le meme
// champ enchantmentText puisqu'on ne peut etre que sur un atelier a la fois.
//
// L'ItemCard du crafting bascule via gotoAndStop entre frames :
//   - Apparel / Weapon / Apparel_Enchanted / Weapon_Enchanted (forge classique)
//   - Craft_Enchanting / Craft_Enchanting_Enchantment / Craft_Enchanting_Weapon /
//     Craft_Enchanting_Armor / Craft_Enchanting_SoulGem (hotel d'enchantement)
//   - Potions_reg (table d'alchimie : reutilise la frame de l'item card de
//     potion classique de l'inventaire, il n'existe pas de frame Craft_Alchemy_*)
//
// Pour la forge classique : labels ApparelEnchantedLabel / WeaponEnchantedLabel
// (pour decrire l'enchantement deja present sur l'objet a ameliorer).
//
// Pour l'hotel d'enchantement : un seul label EnchantmentLabel partage entre
// les 4 sous-frames Craft_Enchanting_*. Il contient :
//   - sur la liste d'effets (ICT_CRAFT_ENCHANTING) : la description de
//     l'enchantement selectionne (ex "Inflige 10 points de degats de feu")
//   - sur l'arme/armure a enchanter : l'enchantement deja choisi a appliquer
//   - sur la gemme spirituelle : description de la charge
//
// Pour l'alchimie : un seul label PotionsLabel contient TOUS les effets de la
// potion concatenes en HTML avec <br/> comme separateur (ex "Restaure 25 PV
// pendant 1s<br/>Restaure 50 PM pendant 5s"). Pas besoin d'iterer effet par
// effet. StripMarkupForSpeech enleve les balises et garde le texte lisible.
//
// Verifie via decompilation de UI/bsa_scripts/craftingmenu/scripts/__Packages/
// ItemCard.as (lignes 467-478 pour enchantement, 238-244 cas ICT_POTION pour
// PotionsLabel) et UI/itemcard.swf. Cf rapports skyrim-ui-explorer 2026-05.
//
// On essaie sequentiellement, htmlText puis text en fallback. Premier non vide gagne.
static void ReadCraftingEnchantment(RE::GFxMovieView* movie, std::wstring& out) {
    const char* paths[] = {
        // Forge : objet a ameliorer deja enchante
        "_root.Menu.ItemInfoHolder.ItemInfo.ApparelEnchantedLabel.htmlText",
        "_root.Menu.ItemInfoHolder.ItemInfo.ApparelEnchantedLabel.text",
        "_root.Menu.ItemInfoHolder.ItemInfo.WeaponEnchantedLabel.htmlText",
        "_root.Menu.ItemInfoHolder.ItemInfo.WeaponEnchantedLabel.text",
        // Hotel d'enchantement : description de l'effet selectionne
        "_root.Menu.ItemInfoHolder.ItemInfo.EnchantmentLabel.htmlText",
        "_root.Menu.ItemInfoHolder.ItemInfo.EnchantmentLabel.text",
        // Alchimie : effets de la potion selectionnee (frame Potions_reg)
        "_root.Menu.ItemInfoHolder.ItemInfo.PotionsLabel.htmlText",
        "_root.Menu.ItemInfoHolder.ItemInfo.PotionsLabel.text",
    };
    std::string raw;
    for (auto* p : paths) {
        if (GetGFxString(movie, p, raw) && !raw.empty()) {
            out = StripMarkupForSpeech(Utf8ToWString(raw));
            if (!out.empty()) return;
        }
    }
}

static std::atomic_bool g_craftingIsSimpleList{false};  // true = tannerie/meule/établi (pas de catégories)
static std::atomic_bool g_craftingModeDetected{false};

// Lecture sûre pour mode simple — ne lit QUE les textfields de ItemInfo (pas de getters AS2)
static bool ReadCraftingSnapshotSimple(CraftingSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::CraftingMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
    std::string tmp;

    // Lire le nom de l'item
    // SkyUI : CategoryList.itemList.selectedEntry.text
    // Vanilla : ItemListTweener.List_mc.selectedEntry.text
    const char* itemPaths[] = {
        skyui ? "_root.Menu.CategoryList.itemList.selectedEntry.text" : "_root.Menu.ItemListTweener.List_mc.selectedEntry.text",
        "_root.Menu.ItemInfoHolder.ItemInfo.ItemText.ItemTextField.text",
    };
    for (auto* p : itemPaths) {
        if (GetGFxString(movie, p, tmp) && !tmp.empty()) {
            snap.itemText = StripMarkupForSpeech(Utf8ToWString(tmp));
            break;
        }
    }

    // Stats
    auto readField = [&](const char* path, std::wstring& out) {
        std::string s;
        if (GetGFxString(movie, path, s) && !s.empty())
            out = Utf8ToWString(s);
    };
    readField("_root.Menu.ItemInfoHolder.ItemInfo.ItemValueText.text", snap.valueText);
    readField("_root.Menu.ItemInfoHolder.ItemInfo.WeaponDamageValue.text", snap.weaponDamageText);
    readField("_root.Menu.ItemInfoHolder.ItemInfo.ApparelArmorValue.text", snap.apparelArmorText);

    snap.valueText = SanitizeNumericText(snap.valueText);
    snap.weaponDamageText = StripMarkupForSpeech(snap.weaponDamageText);
    snap.apparelArmorText = StripMarkupForSpeech(snap.apparelArmorText);

    // Matériaux requis (reformatés pour clarifier requis vs possédé)
    std::string ingredients;
    if (GetGFxString(movie, "_root.Menu.ItemInfoHolder.AdditionalDescriptionHolder.AdditionalDescription.text", ingredients) && !ingredients.empty())
        snap.ingredientsText = ReformatCraftingIngredients(StripMarkupForSpeech(Utf8ToWString(ingredients)));

    // Effets enchantements (sacs à dos modés, items enchantés)
    ReadCraftingEnchantment(movie, snap.enchantmentText);

    snap.inCategoryMode = false;
    return !snap.itemText.empty();
}

// Lecture pour mode forge (catégories) — utilise CategoryList et selectedEntry
static bool ReadCraftingSnapshotForge(CraftingSnapshot& snap) {
    snap = {};
    auto ui = RE::UI::GetSingleton();
    if (!ui) return false;
    auto menu = ui->GetMenu(RE::CraftingMenu::MENU_NAME);
    if (!menu) return false;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return false;

    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
    std::string tmp;

    // CategoryList fonctionne dans les deux modes (vérifié via logs)
    const char* catListBase = "_root.Menu.CategoryList";

    // Catégorie
    if (skyui) {
        std::string catLabelPath = std::string(catListBase) + ".categoryLabel.textField.text";
        std::string catEntryPath = std::string(catListBase) + ".CategoriesList.selectedEntry.text";
        if (GetGFxString(movie, catLabelPath.c_str(), tmp) && !tmp.empty())
            snap.catText = ResolveUIString(movie, tmp);
        else if (GetGFxString(movie, catEntryPath.c_str(), tmp) && !tmp.empty())
            snap.catText = ResolveUIString(movie, tmp);
    } else {
        if (GetGFxString(movie, "_root.Menu.CategoryList.CategoriesList.centeredEntry.text", tmp) && !tmp.empty())
            snap.catText = ResolveUIString(movie, tmp);
    }

    // Item sélectionné — lire depuis la liste d'abord (vide si focus sur catégories)
    {
        std::string listText, itemCardText;
        std::string listTextPath = std::string(catListBase) + (skyui ? ".itemList.selectedEntry.text" : ".ItemsList.selectedEntry.text");
        GetGFxString(movie, listTextPath.c_str(), listText);

        if (!listText.empty()) {
            // Focus sur les items — lire depuis la liste
            snap.itemText = ResolveUIString(movie, listText);
            snap.inCategoryMode = false;
        } else {
            // Liste vide = focus sur les catégories — ne PAS lire l'ItemCard (elle garde l'ancien)
            snap.inCategoryMode = true;
        }

        // Quantité depuis la liste
        std::string listCountPath = std::string(catListBase) + (skyui ? ".itemList.selectedEntry.count" : ".ItemsList.selectedEntry.count");
        double countNum = 0;
        GetGFxNumber(movie, listCountPath.c_str(), countNum);
        if (static_cast<int>(countNum) > 1 && !snap.itemText.empty())
            snap.itemText += L" (" + std::to_wstring(static_cast<int>(countNum)) + L")";
    }

    // Vanilla : utiliser aussi panelState pour le mode catégorie
    if (!skyui) {
        double panelState = 0;
        std::string statePath = std::string(catListBase) + ".currentState";
        GetGFxNumber(movie, statePath.c_str(), panelState);
        snap.inCategoryMode = (static_cast<int>(panelState) != 2);
    }

    // Stats
    auto readField = [&](const char* path, std::wstring& out) {
        std::string s;
        if (GetGFxString(movie, path, s) && !s.empty())
            out = Utf8ToWString(s);
    };
    readField("_root.Menu.ItemInfoHolder.ItemInfo.ItemValueText.text", snap.valueText);
    readField("_root.Menu.ItemInfoHolder.ItemInfo.WeaponDamageValue.text", snap.weaponDamageText);
    readField("_root.Menu.ItemInfoHolder.ItemInfo.ApparelArmorValue.text", snap.apparelArmorText);

    snap.valueText = SanitizeNumericText(snap.valueText);
    snap.weaponDamageText = StripMarkupForSpeech(snap.weaponDamageText);
    snap.apparelArmorText = StripMarkupForSpeech(snap.apparelArmorText);

    // Matériaux requis (reformatés pour clarifier requis vs possédé)
    std::string ingredients;
    if (GetGFxString(movie, "_root.Menu.AdditionalDescriptionHolder.AdditionalDescription.text", ingredients) && !ingredients.empty())
        snap.ingredientsText = ReformatCraftingIngredients(StripMarkupForSpeech(Utf8ToWString(ingredients)));
    else if (GetGFxString(movie, "_root.Menu.ItemInfoHolder.AdditionalDescriptionHolder.AdditionalDescription.text", ingredients) && !ingredients.empty())
        snap.ingredientsText = ReformatCraftingIngredients(StripMarkupForSpeech(Utf8ToWString(ingredients)));

    // Effets enchantements (sacs à dos modés, items enchantés)
    ReadCraftingEnchantment(movie, snap.enchantmentText);

    return !snap.itemText.empty() || !snap.catText.empty();
}

// Détecte le mode (forge vs simple) via MenuType (simple double read, pas de getter)
static void DetectCraftingMode() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::CraftingMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;


    // Détection : si currentState >= 1 → forge/tannerie (avec catégories), sinon simple (meule, établi)
    const bool skyui = g_skyuiMode.load(std::memory_order_relaxed);
    const char* statePath = "_root.Menu.CategoryList.currentState";
    double panelState = 0;
    bool hasPanelState = GetGFxNumber(movie, statePath, panelState);
    if (hasPanelState && panelState >= 1.0) {
        g_craftingIsSimpleList = false;  // mode forge/catégories (forge, tannerie)
    } else {
        g_craftingIsSimpleList = true;   // mode simple (meule, établi)
    }
    g_craftingModeDetected = true;
    LOG("Crafting mode detected: {} (panelState={})", g_craftingIsSimpleList ? "simple" : "forge", panelState);
}

static void AnnounceCraftingChangeImpl() {
    if (!g_craftingOpen.load()) return;

    // Slider d'enchantement (autel d'enchantement uniquement). Quand il est
    // visible, on n'annonce que la valeur du slider et son impact (nombre de
    // charges produites + cout par utilisation). Pendant ce temps on stoppe
    // la lecture normale du Crafting pour ne pas la noyer.
    {
        auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu(RE::CraftingMenu::MENU_NAME) : nullptr;
        auto* movie = (menu && menu->uiMovie) ? menu->uiMovie.get() : nullptr;
        if (movie) {
            double alpha = 0.0;
            const bool alphaOk = GetGFxNumber(movie,
                "_root.Menu_mc.ItemInfoHolder.ItemInfo.EnchantingSlider_mc._alpha",
                alpha);
            const bool sliderVisible = alphaOk && alpha > 50.0;

            if (sliderVisible) {
                double valD = 0.0;
                GetGFxNumber(movie,
                    "_root.Menu_mc.ItemInfoHolder.ItemInfo.QuantitySlider_mc.value", valD);
                std::string chargesS, costS;
                GetGFxString(movie,
                    "_root.Menu_mc.ItemInfoHolder.ItemInfo.TotalChargesValue.text", chargesS);
                GetGFxString(movie,
                    "_root.Menu_mc.ItemInfoHolder.ItemInfo.MagicCostValue.text", costS);

                const int val = static_cast<int>(valD);

                // Premiere fois qu'on voit le slider : annoncer le contexte + valeurs.
                if (!g_craftingEnchantSliderOpen) {
                    g_craftingEnchantSliderOpen = true;
                    g_lastEnchantSliderValue = val;
                    std::wstring msg = TR("Charges slider");
                    if (!chargesS.empty()) {
                        msg += L", " + TR("uses") + L" "
                             + Utf8ToWString(chargesS);
                    }
                    if (!costS.empty()) {
                        msg += L", " + TR("cost") + L" "
                             + Utf8ToWString(costS);
                    }
                    Speak(msg);
                } else if (val != g_lastEnchantSliderValue) {
                    // Le joueur a bouge le slider : annoncer juste les nouveaux
                    // chiffres (uses + cost), pas le label "Charges slider".
                    g_lastEnchantSliderValue = val;
                    std::wstring msg;
                    if (!chargesS.empty()) {
                        msg += TR("uses") + L" " + Utf8ToWString(chargesS);
                    }
                    if (!costS.empty()) {
                        if (!msg.empty()) msg += L", ";
                        msg += TR("cost") + L" " + Utf8ToWString(costS);
                    }
                    if (!msg.empty()) Speak(msg);
                }
                return;  // ne pas faire la lecture crafting normale tant que le slider est actif
            } else if (g_craftingEnchantSliderOpen) {
                // Slider ferme (joueur a confirme ou annule). Reset.
                g_craftingEnchantSliderOpen = false;
                g_lastEnchantSliderValue = -1;
                g_lastCraftingItemAnnounce.clear();  // forcer relecture de l'item
            }
        }
    }

    // Détecter le mode si pas encore fait
    if (!g_craftingModeDetected) DetectCraftingMode();

    CraftingSnapshot snap;
    bool ok = g_craftingIsSimpleList ? ReadCraftingSnapshotSimple(snap) : ReadCraftingSnapshotForge(snap);
    if (!ok) return;

    const bool catChanged = !snap.catText.empty() && snap.catText != g_lastCraftingCat;
    // Force = touche de navigation clavier pressée → on relit même si texte identique
    // (cas des recettes dupliquées affichées plusieurs fois dans la liste)
    const bool forced = g_craftingForceAnnounce.exchange(false);
    const bool itemChanged = !snap.itemText.empty() && (snap.itemText != g_lastCraftingItemAnnounce || forced);

    const bool firstRead = !g_craftingFirstReadDone;

    // Mode simple (tannerie, meule, etc.) — pas de catégories, lire l'item directement
    if (g_craftingIsSimpleList) {
        if (itemChanged || firstRead) {
            std::wstring announce = snap.itemText;
            auto isZero = [](const std::wstring& s) {
                try { return std::stof(s) == 0.0f; } catch (...) { return s.empty(); }
            };
            if (!snap.weaponDamageText.empty() && !isZero(snap.weaponDamageText))
                announce += L", damage " + snap.weaponDamageText;
            if (!snap.apparelArmorText.empty() && !isZero(snap.apparelArmorText))
                announce += L", armor " + snap.apparelArmorText;
            if (firstRead) SpeakQueue(announce); else Speak(announce);
            if (!snap.enchantmentText.empty()) SpeakQueue(snap.enchantmentText);
            g_lastCraftingItemAnnounce = snap.itemText;
            g_lastCraftingDesc.clear();
            g_craftingFirstReadDone = true;
        }
        if (!snap.ingredientsText.empty() && snap.ingredientsText != g_lastCraftingDesc) {
            SpeakQueue(snap.ingredientsText);
            g_lastCraftingDesc = snap.ingredientsText;
        }
        return;
    }

    // En mode catégorie (vanilla uniquement) : lire seulement la catégorie
    if (snap.inCategoryMode) {
        bool returnedFromItems = !g_lastCraftingItemAnnounce.empty();
        if (catChanged || firstRead || returnedFromItems) {
            if (!snap.catText.empty()) {
                if (firstRead) SpeakQueue(snap.catText); else Speak(snap.catText);
            }
            g_lastCraftingCat = snap.catText;
            g_craftingFirstReadDone = true;
        }
        g_lastCraftingItemAnnounce.clear();
        g_lastCraftingDesc.clear();
        return;
    }

    // En mode items : lire les items
    if (catChanged) {
        Speak(snap.catText);
        g_lastCraftingCat = snap.catText;
        g_lastCraftingDesc.clear();
        // Ne PAS clear g_lastCraftingItemAnnounce — l'ItemCard garde l'ancien texte
        // pendant quelques ticks. On évite de le relire en gardant le dernier annoncé.
    }
    if (itemChanged) {
        // Construire l'annonce avec stats
        std::wstring announce = snap.itemText;
        auto isZero = [](const std::wstring& s) {
            try { return std::stof(s) == 0.0f; } catch (...) { return s.empty(); }
        };
        if (!snap.weaponDamageText.empty() && !isZero(snap.weaponDamageText))
            announce += L", damage " + snap.weaponDamageText;
        if (!snap.apparelArmorText.empty() && !isZero(snap.apparelArmorText))
            announce += L", armor " + snap.apparelArmorText;

        if (firstRead) SpeakQueue(announce); else Speak(announce);
        if (!snap.enchantmentText.empty()) SpeakQueue(snap.enchantmentText);
        g_lastCraftingItemAnnounce = snap.itemText;
        g_lastCraftingDesc.clear();
    }
    // Matériaux requis
    if (!snap.ingredientsText.empty() && snap.ingredientsText != g_lastCraftingDesc) {
        SpeakQueue(snap.ingredientsText);
        g_lastCraftingDesc = snap.ingredientsText;
    }
}

static void QueueCraftingRead() {
    if (!g_craftingOpen.load(std::memory_order_relaxed)) return;
    if (g_craftingPendingUIRead.exchange(true)) return;
    auto* task = SKSE::GetTaskInterface();
    if (!task) { g_craftingPendingUIRead.store(false); return; }
    task->AddUITask([]() {
        g_craftingPendingUIRead.store(false);
        if (g_craftingOpen.load()) AnnounceCraftingChangeImpl();
    });
}

static void StartCraftingPolling() {
    if (g_craftingPollThread.joinable()) { g_craftingPollThread.request_stop(); g_craftingPollThread.join(); }
    g_craftingPollThread = std::jthread([](std::stop_token st) {
        // Attendre 500ms pour laisser le menu s'initialiser
        for (int i = 0; i < 5 && !st.stop_requested(); i++)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Détecter le mode via AddUITask et attendre le résultat
        if (!st.stop_requested() && g_craftingOpen.load(std::memory_order_relaxed)) {
            auto detected = std::make_shared<std::atomic_bool>(false);
            auto* task = SKSE::GetTaskInterface();
            if (task) {
                task->AddUITask([detected]() {
                    DetectCraftingMode();
                    detected->store(true);
                });
            }
            // Attendre que la détection soit faite (max 2 secondes)
            for (int i = 0; i < 40 && !detected->load() && !st.stop_requested(); i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        LOG("Crafting: {} mode detected, starting polling", g_craftingIsSimpleList ? "simple" : "forge");
        // Mode forge : polling normal
        while (!st.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(80));
            if (g_craftingOpen.load(std::memory_order_relaxed)) QueueCraftingRead();
        }
    });
}

static void StopCraftingPolling() {
    if (g_craftingPollThread.joinable()) { g_craftingPollThread.request_stop(); g_craftingPollThread.join(); }
}

// --- Tri SkyUI dans le menu Crafting (touches 5 et 6) ---
//
// La forge utilise une layout SkyUI distincte de l'inventaire :
//   path  : _root.Menu.CategoryList.itemList.layout
//   colonnes : <equipColumn(0), iconColumn(1), craftNameColumn(2),
//               subTypeColumn(3), damageColumn(4), arColumn(5),
//               weightColumn(6), valueColumn(7), valueWeightColumn(8)>
//
// Note : ne s'applique qu'a la forge "complete" (categories actives). La
// meule, la tannerie et l'etabli utilisent un mode simple sans categories
// (g_craftingIsSimpleList == true) ou cette layout n'existe pas. Dans ce
// cas, on annonce un feedback explicite et on n'envoie pas la commande GFx.
//
// Pattern copie de SkyUISortColumn (menu_inventory.h) pour rester coherent
// avec le tri inventaire/conteneur/marchand.
static void SkyUISortCraftingColumn(int columnIndex, int stateIndex, const std::wstring& label) {
    if (!g_skyuiMode.load(std::memory_order_relaxed)) return;
    if (!g_craftingOpen.load(std::memory_order_relaxed)) return;
    // Mode simple (meule, tannerie, etabli) : pas de categories ni de tri
    // SkyUI possible. Feedback explicite pour ne pas laisser l'utilisateur
    // sans retour.
    if (g_craftingIsSimpleList.load(std::memory_order_relaxed)) {
        Speak(L"Tri indisponible sur cet etabli");
        return;
    }
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    Speak(label);
    task->AddUITask([columnIndex, stateIndex]() {
        auto ui = RE::UI::GetSingleton();
        if (!ui) return;
        auto menu = ui->GetMenu(RE::CraftingMenu::MENU_NAME);
        if (!menu || !menu->uiMovie) {
            LOG("SkyUI crafting sort: CraftingMenu non ouvert");
            return;
        }
        auto* movie = menu->uiMovie.get();
        const char* layoutPath = "_root.Menu.CategoryList.itemList.layout";
        RE::GFxValue layout;
        if (SafeGetVariable(movie, layout, layoutPath) && SafeIsObject(layout)) {
            RE::GFxValue args[2];
            args[0].SetNumber(static_cast<double>(columnIndex));
            args[1].SetNumber(static_cast<double>(stateIndex));
            layout.Invoke("restoreColumnState", nullptr, args, 2);
            LOG("SkyUI crafting sort: column {} state {}", columnIndex, stateIndex);
        } else {
            LOG("SkyUI crafting sort: layout introuvable a {}", layoutPath);
        }
    });
}

// VOCALISATION MENU CRAFTING - FIN
