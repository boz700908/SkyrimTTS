#pragma once

// TUTORIALMENU - DEBUT

static constexpr const char* TUTO_TITLE   = "_root.Menu_mc.TitleText.text";
static constexpr const char* TUTO_CONTENT = "_root.Menu_mc.HelpText.textField.text";

static void AnnounceTutorialImpl() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::TutorialMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    std::string title, content;
    GetGFxString(movie, TUTO_TITLE, title);
    GetGFxString(movie, TUTO_CONTENT, content);

    std::wstring text;
    if (!title.empty())
        text += StripMarkupForSpeech(Utf8ToWString(title));
    if (!content.empty()) {
        if (!text.empty()) text += L". ";
        text += StripMarkupForSpeech(Utf8ToWString(content));
    }
    if (!text.empty())
        Speak(text);
}

static void QueueTutorialRead() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() { AnnounceTutorialImpl(); });
}

// TUTORIALMENU - FIN
