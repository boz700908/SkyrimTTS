#pragma once

// TUTORIALMENU - DEBUT

static constexpr const char* TUTO_TITLE        = "_root.Menu_mc.TitleText.text";
static constexpr const char* TUTO_CONTENT_TEXT  = "_root.Menu_mc.HelpText.textField.text";
static constexpr const char* TUTO_CONTENT_HTML  = "_root.Menu_mc.HelpText.textField.htmlText";

// ReplaceImgTagsWithKeyNames is in common.h

static void AnnounceTutorialImpl() {
    auto ui = RE::UI::GetSingleton();
    if (!ui) return;
    auto menu = ui->GetMenu(RE::TutorialMenu::MENU_NAME);
    if (!menu) return;
    RE::GFxMovieView* movie = menu->uiMovie.get();
    if (!movie) return;

    std::string title, content;
    GetGFxString(movie, TUTO_TITLE, title);

    // Try htmlText first (contains <img src='KeyName.png'> for keybinds)
    GetGFxString(movie, TUTO_CONTENT_HTML, content);
    bool fromHtml = !content.empty() && content.find("<img") != std::string::npos;
    if (!fromHtml) {
        content.clear();
        GetGFxString(movie, TUTO_CONTENT_TEXT, content);
    }

    std::wstring text;
    if (!title.empty())
        text += StripMarkupForSpeech(Utf8ToWString(title));
    if (!content.empty()) {
        if (!text.empty()) text += L". ";
        std::wstring contentW = Utf8ToWString(content);
        if (fromHtml)
            contentW = ReplaceImgTagsWithKeyNames(contentW);
        text += StripMarkupForSpeech(contentW);
    }
    if (!text.empty()) {
        LOG("TUTO speech='{}' (fromHtml={})", WStringToUtf8(text), fromHtml);
        Speak(text);
    }
}

static void QueueTutorialRead() {
    auto* task = SKSE::GetTaskInterface();
    if (!task) return;
    task->AddUITask([]() { AnnounceTutorialImpl(); });
}

// TUTORIALMENU - FIN
