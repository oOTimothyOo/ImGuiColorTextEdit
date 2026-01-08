#pragma once

#include "TextEditor.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <stack>
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief Bracket matching and colorization for TextEditor
 *
 * Provides VSCode-style features:
 * - Rainbow bracket colorization by nesting depth
 * - Matching bracket highlighting when cursor is on a bracket
 * - Bracket pair guides (vertical lines)
 * - Configurable bracket pairs per language
 */
class TextEditorBracketMatcher
{
public:
    struct BracketPair
    {
        int open_line = 0;
        int open_column = 0;
        int open_indent_column = 0;
        int close_line = 0;
        int close_column = 0;
        int depth = 0;
        char open_char = '\0';
        char close_char = '\0';
    };

    struct Config
    {
        bool enabled = true;
        bool colorize_brackets = true;
        bool highlight_matching = true;
        bool show_bracket_guides = false;  // Vertical lines connecting brackets
        int max_depth = 6;  // Number of colors to cycle through

        // Rainbow colors for different nesting depths
        std::array<ImU32, 6> rainbow_colors = {
            IM_COL32(255, 215, 0,   255),  // Gold
            IM_COL32(218, 112, 214, 255),  // Orchid
            IM_COL32(135, 206, 250, 255),  // Light Sky Blue
            IM_COL32(144, 238, 144, 255),  // Light Green
            IM_COL32(255, 182, 193, 255),  // Light Pink
            IM_COL32(255, 160, 122, 255),  // Light Salmon
        };

        ImU32 matching_highlight_color = IM_COL32(255, 255, 255, 100);
        ImU32 guide_color = IM_COL32(100, 100, 100, 80);

        // Supported bracket pairs
        std::vector<std::pair<char, char>> bracket_pairs = {
            {'(', ')'},
            {'{', '}'},
            {'[', ']'},
            {'<', '>'}  // Optional, can be disabled for some languages
        };
    };

    TextEditorBracketMatcher() : config_() {}
    explicit TextEditorBracketMatcher(Config config) : config_(std::move(config)) {}
    ~TextEditorBracketMatcher() = default;

    // Non-copyable
    TextEditorBracketMatcher(const TextEditorBracketMatcher&) = delete;
    TextEditorBracketMatcher& operator=(const TextEditorBracketMatcher&) = delete;

    // Movable
    TextEditorBracketMatcher(TextEditorBracketMatcher&&) noexcept = default;
    TextEditorBracketMatcher& operator=(TextEditorBracketMatcher&&) noexcept = default;

    /**
     * @brief Analyze the document and find all bracket pairs
     * @param editor The text editor to analyze
     */
    void AnalyzeDocument(const TextEditor& editor);

    /**
     * @brief Get the color for a bracket at given position
     * @param line Line number
     * @param column Column number
     * @return Color to use, or nullopt if not a bracket
     */
    [[nodiscard]] std::optional<ImU32> GetBracketColor(int line, int column) const;

    /**
     * @brief Find matching bracket for cursor position
     * @param cursor_line Current cursor line
     * @param cursor_column Current cursor column
     * @return Matching bracket position, or nullopt if none
     */
    [[nodiscard]] std::optional<BracketPair> FindMatchingBracket(int cursor_line, int cursor_column) const;

    /**
     * @brief Render bracket guides (vertical lines)
     * @param draw_list ImGui draw list
     * @param editor The text editor
     * @param text_start_x X position where text starts
     * @param line_height Height of a line
     */
    void RenderBracketGuides(ImDrawList* draw_list,
                            const TextEditor& editor,
                            float text_start_x,
                            float line_height);

    // Configuration accessors
    [[nodiscard]] auto& GetConfig() { return config_; }
    [[nodiscard]] const auto& GetConfig() const { return config_; }

    void SetEnabled(bool enabled) { config_.enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return config_.enabled; }

    [[nodiscard]] const auto& GetBracketPairs() const { return bracket_pairs_; }

private:
    Config config_;
    std::vector<BracketPair> bracket_pairs_;

    // Cache: position -> bracket info
    std::unordered_map<uint64_t, BracketPair> bracket_cache_;

    /**
     * @brief Convert line/column to a hash key for fast lookup
     */
    [[nodiscard]] static constexpr uint64_t MakeKey(int line, int column)
    {
        return (static_cast<uint64_t>(line) << 32) | static_cast<uint64_t>(column);
    }

    /**
     * @brief Check if a character is an opening bracket
     */
    [[nodiscard]] bool IsOpenBracket(char c) const;

    /**
     * @brief Check if a character is a closing bracket
     */
    [[nodiscard]] bool IsCloseBracket(char c) const;

    /**
     * @brief Get the closing bracket for an opening bracket
     */
    [[nodiscard]] std::optional<char> GetMatchingCloseBracket(char open) const;

    /**
     * @brief Get the opening bracket for a closing bracket
     */
    [[nodiscard]] std::optional<char> GetMatchingOpenBracket(char close) const;

    /**
     * @brief Get color for a given depth
     */
    [[nodiscard]] ImU32 GetColorForDepth(int depth) const;
};
