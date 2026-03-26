#pragma once

// VOCALISATION MENU ENTRAINEMENT - DEBUT

// GFx paths (from TrainingMenu.as):
// Compétence :       TrainingMenuObj.TrainingCard.SkillName.text
// Niveau entraîneur: TrainingMenuObj.TrainingCard.TrainerSkill.text
// Entraînements :    TrainingMenuObj.TrainingCard.TimesTrained.text
// Coût :             TrainingMenuObj.TrainingCard.TrainCost.text
// Or du joueur :     TrainingMenuObj.TrainingCard.CurrentGold.text

static std::atomic_bool g_trainingOpen{false};
static std::string g_lastTrainingCost;

static constexpr const char* TR_SKILL   = "_root.TrainingMenuObj.TrainingCard.SkillName.text";
static constexpr const char* TR_TRAINER = "_root.TrainingMenuObj.TrainingCard.TrainerSkill.text";
static constexpr const char* TR_TIMES   = "_root.TrainingMenuObj.TrainingCard.TimesTrained.text";
static constexpr const char* TR_COST    = "_root.TrainingMenuObj.TrainingCard.TrainCost.text";
static constexpr const char* TR_GOLD    = "_root.TrainingMenuObj.TrainingCard.CurrentGold.text";

// Hook AdvanceMovie pour détecter les changements après entraînement
using TrainingAdvanceMovie_t = void(RE::IMenu*, float, std::uint32_t);
static TrainingAdvanceMovie_t* g_origTrainingAdvanceMovie = nullptr;

static void TrainingAdvanceMovie_Hook(RE::IMenu* a_this, float a_interval, std::uint32_t a_currentTime) {
    if (g_origTrainingAdvanceMovie) g_origTrainingAdvanceMovie(a_this, a_interval, a_currentTime);

    if (!g_trainingOpen.load(std::memory_order_relaxed)) return;

    static int s_frameSkip = 0;
    if (++s_frameSkip < 10) return;
    s_frameSkip = 0;

    RE::GFxMovieView* movie = a_this->uiMovie.get();
    if (!movie) return;

    // Détecter un changement de coût (= un entraînement a eu lieu)
    std::string cost;
    if (GetGFxString(movie, TR_COST, cost) && !cost.empty() && cost != g_lastTrainingCost) {
        bool firstRead = g_lastTrainingCost.empty();
        g_lastTrainingCost = cost;
        if (!firstRead) {
            // Relire toutes les infos après un entraînement
            std::string skill, trainer, times, gold;
            GetGFxString(movie, TR_SKILL, skill);
            GetGFxString(movie, TR_TIMES, times);
            GetGFxString(movie, TR_GOLD, gold);
            std::wstring msg;
            if (!skill.empty()) msg += Utf8ToWString(skill);
            if (!times.empty()) msg += L", trained " + Utf8ToWString(times);
            if (!cost.empty()) msg += L", cost " + Utf8ToWString(cost);
            if (!gold.empty()) msg += L", gold " + Utf8ToWString(gold);
            if (!msg.empty()) Speak(msg);
        }
    }
}

static void AnnounceTrainingOpen(RE::GFxMovieView* movie) {
    std::string skill, trainer, times, cost, gold;
    GetGFxString(movie, TR_SKILL,   skill);
    GetGFxString(movie, TR_TRAINER, trainer);
    GetGFxString(movie, TR_TIMES,   times);
    GetGFxString(movie, TR_COST,    cost);
    GetGFxString(movie, TR_GOLD,    gold);

    g_lastTrainingCost = cost;

    std::wstring msg = L"Training";
    if (!skill.empty()) msg += L", " + Utf8ToWString(skill);
    if (!trainer.empty()) msg += L", trainer level " + Utf8ToWString(trainer);
    if (!times.empty()) msg += L", trained " + Utf8ToWString(times);
    if (!cost.empty()) msg += L", cost " + Utf8ToWString(cost);
    if (!gold.empty()) msg += L", gold " + Utf8ToWString(gold);
    Speak(msg);
}

static bool InstallTrainingAdvanceMovieHook() {
    REL::Relocation<std::uintptr_t> vtbl{RE::VTABLE_TrainingMenu[0]};
    g_origTrainingAdvanceMovie = reinterpret_cast<TrainingAdvanceMovie_t*>(vtbl.write_vfunc(0x05, &TrainingAdvanceMovie_Hook));
    LOG("Training Menu AdvanceMovie hook installed");
    return true;
}

// VOCALISATION MENU ENTRAINEMENT - FIN
