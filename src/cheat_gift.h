// cheat_gift.h
// =============================================================================
// Donne des objets au joueur au prochain chargement de save, en contournant
// la console Papyrus (cassee dans certains modpacks ou les hooks Papyrus
// "OnItemAdded" sont mal resolus -> "script not found" sur tout additem).
//
// Cette voie utilise l'API native CommonLibSSE-NG :
//   RE::PlayerCharacter -> AddObjectToContainer(boundObject, nullptr, count, nullptr)
//
// AddObjectToContainer est une virtuelle du moteur, elle ne passe PAS par
// Papyrus, donc elle marche meme si tous les hooks Papyrus inventaire sont
// casses.
//
// Marquage anti-doublon : on note la 1ere execution dans une variable static.
// Effet : un seul "don" par session de jeu (= un seul par lancement du
// processus SkyrimSE.exe). Si on relance le jeu, on redonne. Pour l'usage
// rapide "j'ai besoin de stuff pour cette session de craft", c'est suffisant
// et evite de spammer si on enchaine load/save/load.
//
// Pour activer/desactiver : changer kEnabled ci-dessous. Apres la session
// de craft, remettre a false et rebuild pour ne plus donner d'objets.
// =============================================================================
#pragma once

#include "common.h"

#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>

namespace CheatGift
{
    // -------------------------------------------------------------------------
    // CONFIGURATION
    // -------------------------------------------------------------------------
    // Mettre a true pour activer le don au prochain load de save.
    // IMPORTANT : remettre a false apres usage pour ne plus donner d'objets
    // automatiquement a chaque load.
    constexpr bool kEnabled = false;

    // Liste des objets a donner. FormID hexa + quantite + nom pour le log/NVDA.
    // Tous les FormID ci-dessous sont VANILLA (Skyrim.esm), donc le prefixe
    // 00 est correct quel que soit le load order.
    struct Gift
    {
        RE::FormID   formID;
        std::int32_t count;
        const char*  label;   // pour le log + annonce NVDA
    };

    inline constexpr Gift kGifts[] = {
        { 0x0005ACE5, 500, "lingots d'acier" },      // SteelIngot (Skyrim.esm) - verifie 2026
        { 0x000800E4, 500, "bandes de cuir" },       // LeatherStrips (Skyrim.esm) - verifie 2026
        // Ajouts faciles si besoin plus tard :
        //   { 0x0005ACE4, 100, "lingots de fer" },           // IronIngot
        //   { 0x000DB5D2,  50, "lingots d'ebonite" },        // EbonyIngot
        //   { 0x0003AD68,  50, "peaux de loup" },            // WolfPelt
        //   { 0x0003AD9C, 100, "cuir tanne" },               // Leather
    };

    // -------------------------------------------------------------------------
    // ETAT INTERNE
    // -------------------------------------------------------------------------
    // Drapeau : true une fois que le don a ete fait dans cette session.
    // Reset automatique a chaque lancement du processus.
    inline std::atomic_bool g_alreadyGiven{ false };

    // -------------------------------------------------------------------------
    // FONCTION CORE : donne un objet a l'inventaire du joueur
    // -------------------------------------------------------------------------
    // Retourne true si le don a reussi, false sinon (form introuvable, joueur
    // null, etc.). N'utilise PAS Papyrus, contourne donc tous les mods qui
    // hookent OnItemAdded.
    //
    // Doit etre appele depuis le thread principal du jeu (via AddTask).
    inline bool GiveItemToPlayer(RE::FormID formID, std::int32_t count)
    {
        if (count <= 0) {
            return false;
        }

        // Resoudre le FormID en TESBoundObject.
        // LookupByID<T> est une variante template qui fait le cast en interne
        // et retourne nullptr si le type ne correspond pas (securite).
        // Cf RE/T/TESForm.h:220.
        auto* boundObj = RE::TESForm::LookupByID<RE::TESBoundObject>(formID);
        if (!boundObj) {
            LOG("CheatGift: FormID 0x{:08X} introuvable ou pas un TESBoundObject",
                formID);
            return false;
        }

        // Recuperer le joueur.
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            LOG("CheatGift: PlayerCharacter singleton null");
            return false;
        }

        // Ajouter au conteneur du joueur.
        // - a_extraList = nullptr : pas d'enchant/charge/proprietaire custom.
        // - a_fromRefr  = nullptr : spawn ex nihilo (pas un transfert).
        // Cf RE/T/TESObjectREFR.h:282.
        player->AddObjectToContainer(boundObj, nullptr, count, nullptr);
        return true;
    }

    // -------------------------------------------------------------------------
    // ENTRY POINT : appele depuis kPostLoadGame dans plugin.cpp
    // -------------------------------------------------------------------------
    // Donne tous les objets de kGifts au joueur, une seule fois par session.
    // Annonce le resultat via NVDA.
    inline void OnPostLoadGame()
    {
        if (!kEnabled) {
            return;
        }

        // Anti-doublon : une seule execution par session de jeu.
        bool expected = false;
        if (!g_alreadyGiven.compare_exchange_strong(expected, true)) {
            // Deja fait dans cette session, on skip silencieusement.
            return;
        }

        // L'appel a AddObjectToContainer doit etre fait sur le thread principal
        // (touche directement la structure d'inventaire du joueur, lue par le
        // moteur a chaque tick). On planifie via SKSE TaskInterface.
        SKSE::GetTaskInterface()->AddTask([]() {
            int  successCount  = 0;
            int  failureCount  = 0;
            std::string summary;

            for (const auto& gift : kGifts) {
                if (GiveItemToPlayer(gift.formID, gift.count)) {
                    ++successCount;
                    LOG("CheatGift: donne {} {} (FormID 0x{:08X})",
                        gift.count, gift.label, gift.formID);
                    if (!summary.empty()) summary += ", ";
                    summary += std::to_string(gift.count) + " " + gift.label;
                } else {
                    ++failureCount;
                    LOG("CheatGift: ECHEC pour FormID 0x{:08X} ({})",
                        gift.formID, gift.label);
                }
            }

            // Annonce NVDA du resultat.
            if (successCount > 0) {
                const std::wstring msg = L"Objets ajoutes : " +
                    Utf8ToWString(summary);
                Speak(msg);
            }
            if (failureCount > 0) {
                Speak(L"Certains objets n'ont pas pu etre ajoutes, voir le log");
            }
        });
    }

} // namespace CheatGift
