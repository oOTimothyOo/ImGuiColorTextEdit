#include "TextEditorBracketMatcher.hpp"
#include <array>
#include <memory_resource>

void TextEditorBracketMatcher::AnalyzeDocument(const TextEditor& editor)
{
    if (!config_.enabled)
        return;

    bracket_pairs_.clear();
    bracket_cache_.clear();

    const int line_count = editor.GetLineCount();
    if (line_count <= 0) {
        return;
    }

    bracket_pairs_.reserve(static_cast<std::size_t>(line_count));
    bracket_cache_.reserve(static_cast<std::size_t>(line_count) * 2U);

    std::array<std::byte, 4096> scratch_storage{};
    std::pmr::monotonic_buffer_resource scratch_resource(scratch_storage.data(), scratch_storage.size());
    std::pmr::vector<BracketPair> stack{&scratch_resource};
    stack.reserve(static_cast<std::size_t>(std::max(8, line_count / 8)));

    const int tab_size = editor.GetTabSize();
    std::string line_text;
    for (int line = 0; line < line_count; ++line) {
        editor.GetLineText(line, line_text);

        int indent_column = 0;
        for (char ch : line_text) {
            if (ch == ' ' || ch == '\t') {
                if (ch == '\t') {
                    const int advance = tab_size - (indent_column % tab_size);
                    indent_column += advance;
                } else {
                    ++indent_column;
                }
            } else {
                break;
            }
        }
        for (int col = 0; col < static_cast<int>(line_text.length()); ++col)
        {
            char ch = line_text[col];

            if (IsOpenBracket(ch))
            {
                // Push opening bracket
                BracketPair pair;
                pair.open_line = line;
                pair.open_column = col;
                pair.open_indent_column = indent_column;
                pair.open_char = ch;
                pair.depth = static_cast<int>(stack.size());

                stack.push_back(pair);
            }
            else if (IsCloseBracket(ch))
            {
                // Pop and match closing bracket
                auto open_match = GetMatchingOpenBracket(ch);

                if (!stack.empty() && open_match && stack.back().open_char == *open_match)
                {
                    BracketPair pair = stack.back();
                    stack.pop_back();

                    pair.close_line = line;
                    pair.close_column = col;
                    pair.close_char = ch;

                    // Store in cache and vector
                    bracket_pairs_.push_back(pair);

                    uint64_t open_key = MakeKey(pair.open_line, pair.open_column);
                    uint64_t close_key = MakeKey(pair.close_line, pair.close_column);

                    bracket_cache_[open_key] = pair;
                    bracket_cache_[close_key] = pair;
                }
                // If stack is empty or brackets don't match, we have a mismatch
                // Could highlight as error in future
            }
        }
    }
}

std::optional<ImU32> TextEditorBracketMatcher::GetBracketColor(int line, int column) const
{
    if (!config_.enabled || !config_.colorize_brackets)
        return std::nullopt;

    uint64_t key = MakeKey(line, column);
    auto it = bracket_cache_.find(key);

    if (it != bracket_cache_.end())
    {
        return GetColorForDepth(it->second.depth);
    }

    return std::nullopt;
}

std::optional<TextEditorBracketMatcher::BracketPair>
TextEditorBracketMatcher::FindMatchingBracket(int cursor_line, int cursor_column) const
{
    if (!config_.enabled || !config_.highlight_matching)
        return std::nullopt;

    uint64_t key = MakeKey(cursor_line, cursor_column);
    auto it = bracket_cache_.find(key);

    if (it != bracket_cache_.end())
    {
        return it->second;
    }

    return std::nullopt;
}

void TextEditorBracketMatcher::RenderBracketGuides(ImDrawList* draw_list,
                                                   const TextEditor& editor,
                                                   float text_start_x,
                                                   float line_height)
{
    if (!config_.enabled || !config_.show_bracket_guides)
        return;

    const int first_visible = editor.GetFirstVisibleLine();
    const int last_visible = editor.GetLastVisibleLine();
    (void)text_start_x;

    // Draw vertical lines for bracket pairs that span multiple lines
    for (const auto& pair : bracket_pairs_)
    {
        // Only draw if the bracket pair spans multiple lines and is visible
        if (pair.close_line <= pair.open_line)
            continue;

        if (pair.open_char != '{' || pair.close_char != '}')
            continue;

        if (pair.close_line < first_visible || pair.open_line > last_visible)
            continue;

        // Calculate Y positions relative to screen coordinates
        int draw_start_line = std::max(pair.open_line, first_visible);
        int draw_end_line = std::min(pair.close_line, last_visible);

        const ImVec2 open_pos = editor.CoordinatesToScreenPos(
            TextEditor::Coordinates{pair.open_line, pair.open_indent_column}
        );
        const ImVec2 start_pos = editor.CoordinatesToScreenPos(
            TextEditor::Coordinates{draw_start_line, 0}
        );
        const ImVec2 end_pos = editor.CoordinatesToScreenPos(
            TextEditor::Coordinates{draw_end_line, 0}
        );

        const float x = open_pos.x;
        const float y_start = start_pos.y;
        const float y_end = end_pos.y + line_height;

        // Draw the guide line with rainbow color based on depth
        ImU32 guide_color = GetColorForDepth(pair.depth);
        // Apply configured alpha while preserving the rainbow color
        ImU32 alpha = (config_.guide_color >> 24) & 0xFF;
        guide_color = (guide_color & 0x00FFFFFF) | (alpha << 24);

        draw_list->AddLine(
            ImVec2(x, y_start),
            ImVec2(x, y_end),
            guide_color,
            1.0f
        );
    }
}

bool TextEditorBracketMatcher::IsOpenBracket(char c) const
{
    for (const auto& [open, close] : config_.bracket_pairs)
    {
        if (c == open)
            return true;
    }
    return false;
}

bool TextEditorBracketMatcher::IsCloseBracket(char c) const
{
    for (const auto& [open, close] : config_.bracket_pairs)
    {
        if (c == close)
            return true;
    }
    return false;
}

std::optional<char> TextEditorBracketMatcher::GetMatchingCloseBracket(char open) const
{
    for (const auto& [open_char, close_char] : config_.bracket_pairs)
    {
        if (open == open_char)
            return close_char;
    }
    return std::nullopt;
}

std::optional<char> TextEditorBracketMatcher::GetMatchingOpenBracket(char close) const
{
    for (const auto& [open_char, close_char] : config_.bracket_pairs)
    {
        if (close == close_char)
            return open_char;
    }
    return std::nullopt;
}

ImU32 TextEditorBracketMatcher::GetColorForDepth(int depth) const
{
    if (depth < 0 || config_.rainbow_colors.empty())
        return IM_COL32(255, 255, 255, 255);

    int color_index = depth % config_.rainbow_colors.size();
    return config_.rainbow_colors[color_index];
}
