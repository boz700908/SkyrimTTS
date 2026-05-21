// puzzle_lexicon.h
// =============================================================================
// Resolution automatique du puzzle de la Tour de Mzark (quete DA04
// "Discerning the Transmundane") pour les joueurs aveugles.
//
// Le puzzle vanilla demande d'aligner deux anneaux concentriques en pressant
// 4 boutons dans un ordre precis (un des boutons est un piege qui recule
// l'anneau). Totalement inaccessible sans la vue.
//
// On detecte l'activation du Receptacle du Lexique et on enchaine
// automatiquement la sequence vanilla, en laissant le moteur jouer toutes
// ses animations et fragments Papyrus normalement. Resultat : le puzzle se
// resout comme si un voyant avait appuye sur les boutons, l'Elder Scroll
// descend, le Lexique se remplit, la quete avance.
//
// MAPPING DES BOUTONS (cellule TowerOfMzark 0x2D4E3, confirme via log) :
//   0x0001BA5D = ForwardLever Armillaire (presse 4 fois)
//   0x0001BA5C = RotateLever Hub         (presse 2 fois, jusqu'a 4 si timing)
//   0x0001BA5E = OpenLever Hub           (presse 1 fois en final)
//   0x0001BA5B = ReverseLever Armillaire (piege, jamais presse)
//   0x0001BA5F = Lexicon Receptacle      (notre declencheur)
//
// SOURCES DU MAPPING :
//   - https://en.uesp.net/wiki/Skyrim:Tower_of_Mzark
//   - https://raw.githubusercontent.com/digital-apple/TESVScripts/main/Base/DA04LexiconStand.psc
//   - https://raw.githubusercontent.com/digital-apple/TESVScripts/main/Base/DA04ArmillaryScript.psc
//   - https://raw.githubusercontent.com/digital-apple/TESVScripts/main/Base/DA04HubScript.psc
//
// SECURITE : flag atomique anti-double-declenchement. Si le mapping etait
// faux, la sequence echouerait silencieusement, sans crash ni save corrompue.
// =============================================================================
#pragma once

#include "common.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace PuzzleLexicon
{
    // FormIDs persistants dans Skyrim.esm (stables entre saves et load order).
    constexpr RE::FormID kReceptacleID  = 0x0001BA5F;
    constexpr RE::FormID kBlankLexicon  = 0x0003A3D2;
    constexpr RE::FormID kButtonForward = 0x0001BA5D;
    constexpr RE::FormID kButtonRotate  = 0x0001BA5C;
    constexpr RE::FormID kButtonOpen    = 0x0001BA5E;

    // Delai entre 2 pressions sur le meme bouton. Doit couvrir la duree de
    // l'animation Trigger01 du bouton + l'anim Pos0X/Trans0X de l'anneau,
    // sinon la pression suivante tombe pendant Busy et est ignoree.
    constexpr int kButtonDelayMs = 3000;

    // Delai entre etapes (laisse le script Papyrus deverrouiller le bouton
    // suivant via OpenUp()/ReadyToOpen()).
    constexpr int kStepGapMs = 2500;

    // Flag : sequence en cours.
    inline std::atomic_bool g_sequenceRunning{ false };

    // Lit le state Papyrus d'un script attache a une ref ("closed", "opened",
    // "busy" pour DA04ButtonScript). "" si pas trouve. Thread principal only.
    inline std::string GetScriptState(RE::FormID formID, const char* scriptName)
    {
        auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(formID);
        if (!ref) return "";
        auto* vm = RE::SkyrimVM::GetSingleton();
        if (!vm || !vm->impl) return "";
        auto* policy = vm->impl->GetObjectHandlePolicy();
        if (!policy) return "";

        auto handle = policy->GetHandleForObject(
            static_cast<RE::VMTypeID>(RE::FormType::Reference), ref);

        RE::BSTSmartPointer<RE::BSScript::Object> scriptObj;
        if (!vm->impl->FindBoundObject(handle, scriptName, scriptObj) || !scriptObj)
            return "";
        return scriptObj->currentState.c_str() ? scriptObj->currentState.c_str() : "";
    }

    // Active une ref par son FormID en simulant une activation joueur.
    inline bool ActivateByFormID(RE::FormID formID)
    {
        auto* ref = RE::TESForm::LookupByID<RE::TESObjectREFR>(formID);
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!ref || !player) return false;
        return ref->ActivateRef(player, 0, nullptr, 1, false);
    }

    // Helper : attend que le bouton 'checkID' devienne 'opened' apres une
    // pression sur 'pressID'. Boucle jusqu'a 'maxTries' fois. Retourne true
    // si le bouton suivant s'est ouvert.
    inline bool PressUntilNextOpens(RE::FormID pressID, RE::FormID checkID,
                                     const char* label, int maxTries)
    {
        for (int i = 0; i < maxTries; ++i) {
            auto* task = SKSE::GetTaskInterface();
            if (task) task->AddTask([pressID]() { ActivateByFormID(pressID); });
            std::this_thread::sleep_for(std::chrono::milliseconds(kButtonDelayMs));
            std::this_thread::sleep_for(std::chrono::milliseconds(kStepGapMs));

            std::atomic_bool done{ false };
            std::atomic_bool ok{ false };
            if (task) task->AddTask([&done, &ok, checkID]() {
                ok.store(GetScriptState(checkID, "DA04ButtonScript") == "opened");
                done.store(true);
            });
            for (int w = 0; w < 50 && !done.load(); ++w)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

            if (ok.load()) {
                LOG("PuzzleLexicon: {} OK apres {} pression(s)", label, i + 1);
                return true;
            }
        }
        LOG("PuzzleLexicon: WARN {} pas ouvert apres {} pressions", label, maxTries);
        return false;
    }

    // Enchaine la sequence vanilla (Forward x4-6, Rotate x2-4, Open x1) sur
    // un thread detache. Chaque pression est planifiee sur le thread principal
    // du jeu via SKSE::GetTaskInterface (Activate doit etre sur le main thread).
    inline void RunSequence()
    {
        bool expected = false;
        if (!g_sequenceRunning.compare_exchange_strong(expected, true)) {
            LOG("PuzzleLexicon: sequence deja en cours, skip");
            return;
        }

        LOG("PuzzleLexicon: sequence START (delai pression={}ms, gap={}ms)",
            kButtonDelayMs, kStepGapMs);

        std::thread([]() {
            // Etape 1 : Forward jusqu'a 6x max, stop si Rotate s'ouvre
            // (= Armillaire arrivee a la position 5 = OpenUp triggered).
            PressUntilNextOpens(kButtonForward, kButtonRotate, "Rotate", 6);

            // Etape 2 : Rotate jusqu'a 4x max, stop si Open s'ouvre
            // (= Hub a la position 3 ET Armillaire 5 = ReadyToOpen).
            PressUntilNextOpens(kButtonRotate, kButtonOpen, "Open", 4);

            // Etape 3 : pression finale sur Open, declenche Inscribe() qui
            // remplit le Lexique et fait apparaitre l'Elder Scroll.
            auto* task = SKSE::GetTaskInterface();
            if (task) task->AddTask([]() { ActivateByFormID(kButtonOpen); });

            Speak(TR("Puzzle solved"));
            LOG("PuzzleLexicon: sequence END");
            g_sequenceRunning.store(false);
        }).detach();
    }

    // Appele depuis ActivateListener (plugin.cpp). Si le joueur active le
    // Receptacle ET possede le Lexique vide, on declenche la sequence apres
    // 1.5s (laisse l'anim de pose du Lexique se faire et les 2 leviers de
    // l'Armillaire passer en state 'opened').
    inline bool TryHandleReceptacleActivate(RE::TESObjectREFR* ref)
    {
        if (!ref || ref->GetFormID() != kReceptacleID) return false;

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* blank  = RE::TESForm::LookupByID<RE::TESBoundObject>(kBlankLexicon);
        if (!player || !blank) return false;
        if (player->GetItemCount(blank) <= 0) {
            LOG("PuzzleLexicon: receptacle active mais joueur n'a pas le Lexique vide, skip");
            return false;
        }

        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            RunSequence();
        }).detach();

        LOG("PuzzleLexicon: receptacle active, sequence dans 1.5s");
        return true;
    }

} // namespace PuzzleLexicon
