#pragma once

#include "common.h"

// =============================================================================
// LootTracker — suivi des conteneurs et cadavres deja ouverts par le joueur.
//
// Inspire du CorpseLootTracker de f4access, adapte pour Skyrim SE/AE.
//
// Fonctionnement :
//   - Quand le joueur ouvre un ContainerMenu (cadavre ou conteneur), on marque
//     le FormID de la ref dans g_looted[formID] = hoursPassed.
//   - Au scan, le scanner consulte IsLooted(formID) pour ajouter ", looted"
//     dans l'annonce.
//   - Quand le moteur respawn une ref (TESResetEvent), on l'efface du set.
//   - Fallback : si >720h (30j) depuis le marquage, on considere que le
//     conteneur a pu respawner meme sans TESResetEvent (safe par defaut).
//   - Pre-filtrage : les conteneurs persistants (kPersistent, safe storage
//     maison, quest containers) ne sont jamais respawnés par le moteur.
//     Pour eux, le flag reste true a vie (comportement correct).
//
// Persistance : SKSE cosave via SerializationInterface. Record type 'LOOT'.
// =============================================================================

class LootTracker {
public:
    static LootTracker* GetSingleton() {
        static LootTracker instance;
        return &instance;
    }

    // Marque le conteneur/cadavre comme deja fouille. Stocke le temps actuel
    // pour le fallback 30j. Thread-safe.
    void MarkLooted(RE::FormID formID) {
        if (formID == 0) return;
        std::lock_guard lk(m_mutex);
        auto* cal = RE::Calendar::GetSingleton();
        float hoursPassed = cal ? cal->GetHoursPassed() : 0.0f;
        m_looted[formID] = hoursPassed;
    }

    // Vrai si le joueur a deja fouille ce conteneur/cadavre. Applique le
    // fallback 30j : si trop de temps s'est ecoule et qu'aucun TESResetEvent
    // n'a nettoye l'entree, on considere que le conteneur a pu respawner.
    // Note : pas const car on nettoie les entrees expirees au passage pour
    // eviter que le map grossisse indefiniment avec des entrees obsoletes.
    bool IsLooted(RE::FormID formID) {
        if (formID == 0) return false;
        std::lock_guard lk(m_mutex);
        auto it = m_looted.find(formID);
        if (it == m_looted.end()) return false;

        // Fallback : si >720h (30 jours de jeu) depuis le marquage, auto-clear.
        // Protege contre le cas ou TESResetEvent n'a pas ete emis (ex: joueur
        // n'est jamais revenu dans la cell avant que le moteur reset).
        auto* cal = RE::Calendar::GetSingleton();
        if (cal) {
            float nowHours = cal->GetHoursPassed();
            float elapsed = nowHours - it->second;
            if (elapsed > 720.0f) {
                m_looted.erase(it);  // nettoyage opportuniste
                return false;
            }
        }
        return true;
    }

    // Efface une entree — appele depuis le hook TESResetEvent quand le moteur
    // respawn la ref.
    void Clear(RE::FormID formID) {
        if (formID == 0) return;
        std::lock_guard lk(m_mutex);
        m_looted.erase(formID);
    }

    // Nettoyage complet — appele au chargement d'une sauvegarde avant de
    // restaurer l'etat persiste.
    void ClearAll() {
        std::lock_guard lk(m_mutex);
        m_looted.clear();
    }

    // Nombre d'entrees (pour logs/debug).
    size_t Size() const {
        std::lock_guard lk(m_mutex);
        return m_looted.size();
    }

    // --- Persistance SKSE cosave ---
    // Record type 'LOOT' version 1. Format :
    //   uint32 count
    //   repeat count times :
    //     uint32 formID (pre-resolved, will be Resolve'd on load)
    //     float  hoursPassed
    static constexpr std::uint32_t kRecordType = 'LOOT';
    static constexpr std::uint32_t kRecordVersion = 1;

    // Ecrit l'etat actuel dans le cosave. Appele sur SKSE::MessagingInterface::kSave.
    void Save(SKSE::SerializationInterface* intf) {
        std::lock_guard lk(m_mutex);
        if (!intf->OpenRecord(kRecordType, kRecordVersion)) {
            LOG("LootTracker: OpenRecord failed");
            return;
        }
        std::uint32_t count = static_cast<std::uint32_t>(m_looted.size());
        intf->WriteRecordData(&count, sizeof(count));
        for (const auto& [formID, hours] : m_looted) {
            intf->WriteRecordData(&formID, sizeof(formID));
            intf->WriteRecordData(&hours, sizeof(hours));
        }
        LOG("LootTracker: saved {} entries to cosave", count);
    }

    // Lit l'etat depuis le cosave. Appele sur SKSE::MessagingInterface::kLoad
    // apres que SerializationInterface ait positionne la tete sur notre record.
    void Load(SKSE::SerializationInterface* intf, std::uint32_t version) {
        if (version != kRecordVersion) {
            LOG("LootTracker: cosave version mismatch (got {}, expected {}), skipping",
                version, kRecordVersion);
            return;
        }
        std::lock_guard lk(m_mutex);
        m_looted.clear();
        std::uint32_t count = 0;
        if (!intf->ReadRecordData(&count, sizeof(count))) {
            LOG("LootTracker: read count failed");
            return;
        }
        std::uint32_t loaded = 0;
        std::uint32_t skipped = 0;
        for (std::uint32_t i = 0; i < count; ++i) {
            RE::FormID rawFormID = 0;
            float hours = 0.0f;
            if (!intf->ReadRecordData(&rawFormID, sizeof(rawFormID))) break;
            if (!intf->ReadRecordData(&hours, sizeof(hours))) break;

            // Resolve le FormID (changements de load order, mods ajoutes/enleves
            // entre deux saves).
            RE::FormID resolvedID = 0;
            if (!intf->ResolveFormID(rawFormID, resolvedID)) {
                // Ref n'existe plus dans le load order courant, on skip.
                skipped++;
                continue;
            }
            m_looted[resolvedID] = hours;
            loaded++;
        }
        LOG("LootTracker: loaded {} entries from cosave ({} skipped, load order changed)",
            loaded, skipped);
    }

private:
    LootTracker() = default;
    LootTracker(const LootTracker&) = delete;
    LootTracker& operator=(const LootTracker&) = delete;

    mutable std::mutex                       m_mutex;
    std::unordered_map<RE::FormID, float>    m_looted;  // formID -> GetHoursPassed() au marquage
};

// =============================================================================
// Pre-filtrage : un conteneur/cadavre peut-il respawner ?
//
// - kPersistent → jamais respawn (safe storage maison, quest containers,
//   player-owned). On ne les exclut PAS du tracking (le joueur veut quand
//   meme savoir qu'il l'a fouille), mais on ne se soucie pas du respawn.
// - !kRespawns ET base !kRespawn → jamais respawn non plus.
// - Les autres → peuvent respawner, le fallback 30j s'applique.
//
// Pour l'instant on tracke TOUT sans filtrage : le flag stocke est utile dans
// tous les cas, meme pour les containers non-respawn (ils restent looted a
// vie, ce qui est le comportement voulu).
// =============================================================================

// Helper : recuperer la ref cible du ContainerMenu actif.
// Strategie preferee : ContainerMenu::GetTargetRefHandle() — accesseur officiel
// expose par CommonLibSSE-NG qui retourne le handle du conteneur/cadavre
// actuellement ouvert. Beaucoup plus fiable que le crosshair (le crosshair est
// desactive pendant que le menu est ouvert).
//
// Fallback crosshair au cas ou (ContainerMenu appele sans setup handle propre,
// cas rare).
inline RE::TESObjectREFR* GetActiveContainerMenuRef() {
    // Methode 1 : API menu officielle. GetTargetRefHandle() retourne un
    // RefHandle (uint32) qu'on resout en NiPointer<TESObjectREFR> via
    // TESObjectREFR::LookupByHandle.
    RE::RefHandle handle = RE::ContainerMenu::GetTargetRefHandle();
    if (handle != 0) {
        RE::NiPointer<RE::TESObjectREFR> refPtr;
        if (RE::TESObjectREFR::LookupByHandle(handle, refPtr) && refPtr) {
            return refPtr.get();
        }
    }

    // Methode 2 (fallback) : crosshair pick data. Moins fiable car le crosshair
    // est desactive pendant que le menu est ouvert, mais au moment precis de
    // l'event d'ouverture il peut encore tenir la derniere valeur.
    auto* crosshair = RE::CrosshairPickData::GetSingleton();
    if (crosshair) {
        auto refPtr = crosshair->target.get();
        if (refPtr) return refPtr.get();
    }
    return nullptr;
}
