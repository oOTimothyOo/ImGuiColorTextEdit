#include "TextEditorBracketMatcher.hpp"
#include "TextEditor.h"
#include <stack>
#include <algorithm>

void TextEditorBracketMatcher::AnalyzeDocument(const TextEditor& editor)
{
    if (!config_.enabled)
        return;

    bracket_pairs_.clear();
    bracket_cache_.clear();

    auto lines = editor.GetTextLines();
    std::stack<BracketPair> stack;

    for (int line = 0; line < static_cast<int>(lines.size()); ++line)
    {
        const auto& line_text = lines[line];

        for (int col = 0; col < static_cast<int>(line_text.length()); ++col)
        {
            char ch = line_text[col];

            if (IsOpenBracket(ch))
            {
                // Push opening bracket
                BracketPair pair;
                pair.open_line = line;
                pair.open_column = col;
                pair.open_char = ch;
                pair.depth = static_cast<int>(stack.size());

                stack.push(pair);
            }
            else if (IsCloseBracket(ch))
            {
                // Pop and match closing bracket
                auto open_match = GetMatchingOpenBracket(ch);

                if (!stack.empty() && open_match && stack.top().open_char == *open_match)
                {
                    BracketPair pair = stack.top();
                    stack.pop();

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

    int first_visible = const_cast<TextEditor&>(editor).GetFirstVisibleLine();
    int last_visible = const_cast<TextEditor&>(editor).GetLastVisibleLine();

    // Draw vertical lines for bracket pairs that span multiple lines
    for (const auto& pair : bracket_pairs_)
    {
        // Only draw if the bracket pair spans multiple lines and is visible
        if (pair.close_line <= pair.open_line)
            continue;

        if (pair.close_line < first_visible || pair.open_line > last_visible)
            continue;

        // Calculate X position based on indentation (column of opening bracket)
        float x = text_start_x + pair.open_column * ImGui::GetFontSize() * 0.5f;

        // Calculate Y positions
        float y_start = (pair.open_line - first_visible) * line_height;
        float y_end = (pair.close_line - first_visible + 1) * line_height;

        // Draw the guide line
        ImU32 guide_color = GetColorForDepth(pair.depth);
        guide_color = (guide_color & 0x00FFFFFF) | (config_.guide_color & 0xFF000000);  // Use guide alpha

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
