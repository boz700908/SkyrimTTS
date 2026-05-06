#pragma once

// VOCALISATION MENU LIVRE - DEBUT

// Le contenu du livre est récupéré depuis TESObjectBOOK (TESDescription + TESFullName)
// Le SWF ne contient que la barre du bas (Take/Turn Page)

static std::atomic_bool g_bookOpen{false};

static void AnnounceBookContent() {
    auto* book = RE::BookMenu::GetTargetForm();
    if (!book) return;

    // Titre du livre
    const char* rawName = book->GetFullName();
    std::wstring title = rawName ? Utf8ToWString(rawName) : L"";

    // Contenu du livre
    RE::BSString desc;
    book->GetDescription(desc, book);
    std::wstring content = Utf8ToWString(desc.c_str());

    // Nettoyer le HTML du contenu (les livres contiennent du markup Scaleform)
    content = StripMarkupForSpeech(content);

    // Annoncer
    if (!title.empty()) {
        Speak(title);
    }
    if (!content.empty()) {
        SpeakQueue(content);
    }

    // Info supplémentaire : enseigne une compétence ou un sort ?
    if (book->TeachesSkill()) {
        auto skill = book->GetSkill();
        if (skill != RE::ActorValue::kNone) {
            SpeakQueue(TR("This book teaches a skill"));
        }
    } else if (book->TeachesSpell()) {
        auto* spell = book->GetSpell();
        if (spell) {
            const char* spellName = spell->GetFullName();
            if (spellName && spellName[0]) {
                SpeakQueue(TR("Teaches spell") + L": " + Utf8ToWString(spellName));
            }
        }
    }
}

// VOCALISATION MENU LIVRE - FIN
