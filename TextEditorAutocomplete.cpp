#include "TextEditorAutocomplete.hpp"

void TextEditorAutocomplete::RegisterProvider(std::unique_ptr<ICompletionProvider> provider)
{
    if (provider)
    {
        providers_.push_back(std::move(provider));
    }
}

void TextEditorAutocomplete::Trigger(const TextEditor& editor, char trigger_char)
{
    if (!config_.enabled)
        return;

    int line, column;
    editor.GetCursorPosition(line, column);

    trigger_line_ = line;
    trigger_column_ = column;

    // Collect completions from all providers
    current_items_.clear();

    for (const auto& provider : providers_)
    {
        auto items = provider->GetCompletions(editor, line, column, trigger_char);
        current_items_.insert(current_items_.end(),
                            std::make_move_iterator(items.begin()),
                            std::make_move_iterator(items.end()));
    }

    if (!current_items_.empty())
    {
        // Sort by priority
        std::sort(current_items_.begin(), current_items_.end(),
                 [](const CompletionItem& a, const CompletionItem& b) {
                     return a.priority > b.priority;
                 });

        filter_text_.clear();
        FilterCompletions(filter_text_);
        selected_index_ = 0;
        is_active_ = true;
    }
}

bool TextEditorAutocomplete::Render([[maybe_unused]] TextEditor& editor)
{
    if (!is_active_ || filtered_items_.empty())
        return false;

    ImGui::SetNextWindowSize(ImVec2(config_.popup_width, 0), ImGuiCond_Always);

    // Position popup near cursor
    ImVec2 cursor_screen_pos = ImGui::GetCursorScreenPos();  // This should be calculated from editor
    ImGui::SetNextWindowPos(cursor_screen_pos, ImGuiCond_Always);

    bool item_selected = false;

    ImGuiWindowFlags const window_flags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (auto window = imgui::scoped::Window("##autocomplete", nullptr, window_flags)) {
        // Render list of items
        ImVec2 const list_size(0.0f, std::min(config_.popup_max_height,
                         static_cast<float>(filtered_items_.size()) * ImGui::GetTextLineHeightWithSpacing()));
        imgui::scoped::Child const items_child("##items", list_size, ImGuiChildFlags_None, 0);

        for (int i = 0; i < static_cast<int>(filtered_items_.size()) && i < config_.max_items; ++i)
        {
            bool is_selected = (i == selected_index_);
            RenderCompletionItem(filtered_items_[i], is_selected);

            if (is_selected && ImGui::IsItemHovered())
            {
                if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                {
                    item_selected = true;
                }
            }
        }

        // Auto-scroll to selected item
        if (selected_index_ >= 0 && selected_index_ < static_cast<int>(filtered_items_.size()))
        {
            float item_pos_y = selected_index_ * ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetScrollFromPosY(item_pos_y - ImGui::GetScrollY());
        }

        // Documentation panel
        if (config_.show_documentation && selected_index_ >= 0 &&
            selected_index_ < static_cast<int>(filtered_items_.size()))
        {
            const auto& item = filtered_items_[selected_index_];
            if (!item.documentation.empty())
            {
                ImGui::Separator();
                ImGui::TextWrapped("%s", item.documentation.c_str());
            }
        }
    }

    return item_selected;
}

bool TextEditorAutocomplete::HandleKeyboard()
{
    if (!is_active_)
        return false;

    if (ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        Close();
        return true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_Tab))
    {
        // Item will be accepted by caller
        return true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
    {
        selected_index_ = std::max(0, selected_index_ - 1);
        return true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
    {
        selected_index_ = std::min(static_cast<int>(filtered_items_.size()) - 1,
                                   selected_index_ + 1);
        return true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_PageUp))
    {
        selected_index_ = std::max(0, selected_index_ - 10);
        return true;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_PageDown))
    {
        selected_index_ = std::min(static_cast<int>(filtered_items_.size()) - 1,
                                   selected_index_ + 10);
        return true;
    }

    return false;
}

void TextEditorAutocomplete::Close()
{
    is_active_ = false;
    current_items_.clear();
    filtered_items_.clear();
    selected_index_ = 0;
    filter_text_.clear();
}

std::optional<TextEditorAutocomplete::CompletionItem>
TextEditorAutocomplete::AcceptSelected([[maybe_unused]] TextEditor& editor)
{
    if (!is_active_ || selected_index_ < 0 ||
        selected_index_ >= static_cast<int>(filtered_items_.size()))
    {
        return std::nullopt;
    }

    auto item = filtered_items_[selected_index_];
    Close();
    return item;
}

void TextEditorAutocomplete::FilterCompletions(const std::string& filter)
{
    filtered_items_.clear();

    if (filter.empty())
    {
        filtered_items_ = current_items_;
        return;
    }

    for (const auto& item : current_items_)
    {
        int score = GetFuzzyMatchScore(item, filter);
        if (score >= 0)
        {
            filtered_items_.push_back(item);
        }
    }

    // Sort by match score
    std::sort(filtered_items_.begin(), filtered_items_.end(),
             [this, &filter](const CompletionItem& a, const CompletionItem& b) {
                 int score_a = GetFuzzyMatchScore(a, filter);
                 int score_b = GetFuzzyMatchScore(b, filter);
                 if (score_a != score_b)
                     return score_a > score_b;
                 return a.priority > b.priority;
             });

    selected_index_ = 0;
}

int TextEditorAutocomplete::GetFuzzyMatchScore(const CompletionItem& item,
                                               const std::string& filter) const
{
    const std::string& text = item.filter_text.empty() ? item.label : item.filter_text;

    if (!config_.fuzzy_matching)
    {
        // Simple prefix match
        if (text.substr(0, filter.length()) == filter)
            return 100;
        return -1;
    }

    // Fuzzy matching: score based on how well filter matches text
    int score = 0;
    size_t filter_idx = 0;
    size_t text_idx = 0;
    bool consecutive = true;

    while (filter_idx < filter.length() && text_idx < text.length())
    {
        if (std::tolower(filter[filter_idx]) == std::tolower(text[text_idx]))
        {
            score += consecutive ? 10 : 5;  // Bonus for consecutive matches
            filter_idx++;
            consecutive = true;
        }
        else
        {
            consecutive = false;
        }
        text_idx++;
    }

    // Did we match all filter characters?
    if (filter_idx < filter.length())
        return -1;  // Not all characters matched

    // Bonus for shorter matches
    score += 100 / static_cast<int>(text.length() + 1);

    return score;
}

const char* TextEditorAutocomplete::GetIconForKind(CompletionItemKind kind) const
{
    switch (kind)
    {
        case CompletionItemKind::Method:
        case CompletionItemKind::Function:      return "f";
        case CompletionItemKind::Constructor:   return "c";
        case CompletionItemKind::Field:
        case CompletionItemKind::Variable:      return "v";
        case CompletionItemKind::Class:         return "C";
        case CompletionItemKind::Interface:     return "I";
        case CompletionItemKind::Module:        return "M";
        case CompletionItemKind::Property:      return "p";
        case CompletionItemKind::Enum:          return "E";
        case CompletionItemKind::Keyword:       return "k";
        case CompletionItemKind::Snippet:       return "s";
        case CompletionItemKind::Constant:      return "K";
        case CompletionItemKind::Struct:        return "S";
        default:                                return "?";
    }
}

ImU32 TextEditorAutocomplete::GetColorForKind(CompletionItemKind kind) const
{
    switch (kind)
    {
        case CompletionItemKind::Method:        return vscode::colors::syntax_method;
        case CompletionItemKind::Function:      return vscode::colors::syntax_function;
        case CompletionItemKind::Constructor:   return vscode::colors::syntax_method;
        case CompletionItemKind::Class:
        case CompletionItemKind::Struct:        return vscode::colors::syntax_type;
        case CompletionItemKind::Variable:
        case CompletionItemKind::Field:         return vscode::colors::syntax_variable;
        case CompletionItemKind::Keyword:       return vscode::colors::keyword_color;
        case CompletionItemKind::Constant:      return vscode::colors::syntax_enum_member;
        case CompletionItemKind::Enum:
        case CompletionItemKind::EnumMember:    return vscode::colors::syntax_enum_member;
        case CompletionItemKind::Property:      return vscode::colors::syntax_property;
        case CompletionItemKind::Module:        return vscode::colors::syntax_namespace;
        default:                                return vscode::colors::foreground;
    }
}

void TextEditorAutocomplete::RenderCompletionItem(const CompletionItem& item, bool is_selected)
{
    std::optional<imgui::scoped::StyleColor> selected_bg{};
    if (is_selected) {
        selected_bg.emplace(ImGuiCol_Header, vscode::colors::list_selection_bg);
    }

    if (ImGui::Selectable(("##item" + item.label).c_str(), is_selected,
                         ImGuiSelectableFlags_AllowOverlap))
    {
        // Handled by caller
    }

    ImGui::SameLine();

    // Icon
    if (config_.show_icons)
    {
        ImU32 icon_color = GetColorForKind(item.kind);
        ImGui::TextColored(vscode::colors::to_vec4(icon_color),
                          "%s", GetIconForKind(item.kind));
        ImGui::SameLine();
    }

    // Label
    ImGui::Text("%s", item.label.c_str());

    // Detail (right-aligned)
    if (!item.detail.empty())
    {
        ImGui::SameLine();
        float detail_width = ImGui::CalcTextSize(item.detail.c_str()).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                            ImGui::GetContentRegionAvail().x - detail_width);
        ImGui::TextDisabled("%s", item.detail.c_str());
    }

}

// KeywordCompletionProvider implementation

KeywordCompletionProvider::KeywordCompletionProvider(std::vector<std::string> keywords)
    : keywords_(std::move(keywords))
{
}

std::vector<TextEditorAutocomplete::CompletionItem>
KeywordCompletionProvider::GetCompletions([[maybe_unused]] const TextEditor& editor,
                                         [[maybe_unused]] int line,
                                         [[maybe_unused]] int column,
                                         [[maybe_unused]] char trigger_char)
{
    std::vector<TextEditorAutocomplete::CompletionItem> items;

    for (const auto& keyword : keywords_)
    {
        TextEditorAutocomplete::CompletionItem item;
        item.label = keyword;
        item.insert_text = keyword;
        item.kind = TextEditorAutocomplete::CompletionItemKind::Keyword;
        item.priority = 50;
        items.push_back(std::move(item));
    }

    return items;
}

std::vector<char> KeywordCompletionProvider::GetTriggerCharacters() const
{
    return {};  // Keywords don't need special trigger characters
}
