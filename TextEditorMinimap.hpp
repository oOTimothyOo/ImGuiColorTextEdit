#pragma once

#include "TextEditor.h"
#include "imgui.h"
#include "utilities/imgui_scoped.hpp"
#include "vscode/colors.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

/**
 * @brief Minimap renderer for TextEditor - provides VSCode-style code overview
 *
 * The minimap shows a scaled-down version of the entire document,
 * allowing quick navigation and providing context about code structure.
 */
class TextEditorMinimap
{
public:
    struct Config
    {
        bool enabled = true;
        float width = 120.0f;
        float opacity_background = 0.85f;
        float opacity_foreground = 1.0f;
        float pixels_per_line = 2.0f;
        bool show_viewport_indicator = true;
        bool show_hover_preview = true;
        ImU32 viewport_color = vscode::colors::minimap_viewport;
        ImU32 hover_color = vscode::colors::minimap_hover;
    };

    TextEditorMinimap() : config_() {}
    explicit TextEditorMinimap(Config config) : config_(std::move(config)) {}
    ~TextEditorMinimap() = default;

    // Non-copyable
    TextEditorMinimap(const TextEditorMinimap&) = delete;
    TextEditorMinimap& operator=(const TextEditorMinimap&) = delete;

    // Movable
    TextEditorMinimap(TextEditorMinimap&&) noexcept = default;
    TextEditorMinimap& operator=(TextEditorMinimap&&) noexcept = default;

    /**
     * @brief Render the minimap
     * @param editor The text editor to render minimap for
     * @param available_region The region where minimap should be drawn
     * @return true if minimap was clicked/interacted with
     */
    [[nodiscard]] bool Render(const TextEditor& editor, const ImVec2& available_region);

    /**
     * @brief Handle mouse interactions with minimap
     * @param editor The text editor (for scrolling)
     * @param minimap_min The minimum point of the minimap rect
     * @param minimap_max The maximum point of the minimap rect
     * @return Line number that was clicked, or -1 if no click
     */
    [[nodiscard]] int HandleInput(TextEditor& editor, const ImVec2& minimap_min, const ImVec2& minimap_max);

    // Configuration accessors
    [[nodiscard]] auto& GetConfig() { return config_; }
    [[nodiscard]] const auto& GetConfig() const { return config_; }

    void SetEnabled(bool enabled) { config_.enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return config_.enabled; }

    void SetWidth(float width) { config_.width = width; }
    [[nodiscard]] float GetWidth() const { return config_.width; }

    void SetOpacity(float opacity) { config_.opacity_foreground = opacity; }
    [[nodiscard]] float GetOpacity() const { return config_.opacity_foreground; }

    // Get the line that was clicked/dragged to (returns -1 if none)
    [[nodiscard]] int GetClickedLine() const { return clicked_line_; }
    void ResetClickedLine() { clicked_line_ = -1; }

private:
    Config config_;
    int hovered_line_ = -1;
    int clicked_line_ = -1;
    bool is_dragging_ = false;

    /**
     * @brief Render a single line in the minimap
     * @param draw_list ImGui draw list
     * @param line_text The text of the line
     * @param line_index The line number
     * @param minimap_min The minimum point of the minimap rect
     * @param minimap_max The maximum point of the minimap rect
     * @param colors Syntax highlighting colors
     */
    void RenderLine(ImDrawList* draw_list,
                   const char* line_text,
                   int line_index,
                   const ImVec2& minimap_min,
                   const ImVec2& minimap_max,
                   const void* colors);

    /**
     * @brief Calculate the Y position for a given line in the minimap
     */
    [[nodiscard]] float GetLineYPosition(int line, int total_lines, float minimap_height) const;

    /**
     * @brief Convert a Y position in minimap to a line number
     */
    [[nodiscard]] int GetLineFromYPosition(float y, int total_lines, float minimap_height) const;
};
