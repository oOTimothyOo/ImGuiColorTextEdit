#pragma once

#include "imgui.h"
#include <vector>
#include <unordered_map>
#include <optional>
#include <string>

// Forward declaration
class TextEditor;

/**
 * @brief Code folding system for TextEditor
 *
 * Provides VSCode-style code folding:
 * - Automatic detection of foldable regions (braces, indentation)
 * - Manual fold/unfold with gutter icons
 * - Fold all / unfold all commands
 * - Persistent fold state
 * - Keyboard shortcuts
 */
class TextEditorCodeFolding
{
public:
    struct FoldRegion
    {
        int start_line = 0;
        int end_line = 0;
        bool is_folded = false;
        int indent_level = 0;

        bool IsValid() const { return end_line > start_line; }
        bool Contains(int line) const { return line >= start_line && line <= end_line; }
    };

    enum class DetectionMode
    {
        Braces,        // Detect by matching braces {}
        Indentation,   // Detect by indentation level
        Both           // Use both methods
    };

    struct Config
    {
        bool enabled = true;
        DetectionMode detection_mode = DetectionMode::Both;
        int min_lines_to_fold = 2;   // Minimum lines before region is foldable
        bool show_fold_icons = true;
        bool fold_on_goto = false;    // Unfold when navigating to a line

        // Visual config
        ImU32 fold_icon_color = IM_COL32(150, 150, 150, 255);
        ImU32 fold_icon_hover_color = IM_COL32(255, 255, 255, 255);
        float icon_size = 12.0f;

        // Placeholder text for folded regions
        const char* fold_placeholder = " ... ";
        ImU32 placeholder_color = IM_COL32(100, 100, 100, 255);
    };

    TextEditorCodeFolding() : config_() {}
    explicit TextEditorCodeFolding(Config config) : config_(std::move(config)) {}
    ~TextEditorCodeFolding() = default;

    // Non-copyable
    TextEditorCodeFolding(const TextEditorCodeFolding&) = delete;
    TextEditorCodeFolding& operator=(const TextEditorCodeFolding&) = delete;

    // Movable
    TextEditorCodeFolding(TextEditorCodeFolding&&) noexcept = default;
    TextEditorCodeFolding& operator=(TextEditorCodeFolding&&) noexcept = default;

    /**
     * @brief Analyze document and detect foldable regions
     * @param editor The text editor to analyze
     */
    void AnalyzeDocument(const TextEditor& editor);

    /**
     * @brief Toggle fold state for a region at given line
     * @param line Line number
     * @return true if fold state changed
     */
    [[nodiscard]] bool ToggleFold(int line);

    /**
     * @brief Fold a specific region
     * @param line Line number within the region
     * @return true if folded
     */
    [[nodiscard]] bool Fold(int line);

    /**
     * @brief Unfold a specific region
     * @param line Line number within the region
     * @return true if unfolded
     */
    [[nodiscard]] bool Unfold(int line);

    /**
     * @brief Fold all regions in the document
     */
    void FoldAll();

    /**
     * @brief Unfold all regions in the document
     */
    void UnfoldAll();

    /**
     * @brief Check if a line is hidden due to folding
     * @param line Line number
     * @return true if line should not be rendered
     */
    [[nodiscard]] bool IsLineHidden(int line) const;

    /**
     * @brief Get the fold region that contains this line
     * @param line Line number
     * @return Fold region, or nullopt if none
     */
    [[nodiscard]] std::optional<FoldRegion> GetRegionAtLine(int line) const;

    /**
     * @brief Get fold region that starts at this line
     * @param line Line number
     * @return Fold region, or nullopt if none starts here
     */
    [[nodiscard]] std::optional<FoldRegion> GetRegionStartingAtLine(int line) const;

    /**
     * @brief Render fold icons in the gutter
     * @param draw_list ImGui draw list
     * @param line Line number
     * @param icon_pos Position to draw icon
     * @param line_height Height of a line
     * @return true if icon was clicked
     */
    [[nodiscard]] bool RenderFoldIcon(ImDrawList* draw_list, int line,
                                     const ImVec2& icon_pos, float line_height);

    /**
     * @brief Render placeholder for folded region
     * @param draw_list ImGui draw list
     * @param region The folded region
     * @param start_pos Position to start drawing
     * @return Width of placeholder
     */
    [[nodiscard]] float RenderFoldPlaceholder(ImDrawList* draw_list,
                                             const FoldRegion& region,
                                             const ImVec2& start_pos);

    /**
     * @brief Map visual line to actual line (accounting for folds)
     * @param visual_line Visual line number (what user sees)
     * @return Actual line number in document
     */
    [[nodiscard]] int VisualLineToActualLine(int visual_line) const;

    /**
     * @brief Map actual line to visual line (accounting for folds)
     * @param actual_line Actual line number in document
     * @return Visual line number, or -1 if hidden
     */
    [[nodiscard]] int ActualLineToVisualLine(int actual_line) const;

    /**
     * @brief Get all fold regions
     */
    [[nodiscard]] const auto& GetRegions() const { return regions_; }

    /**
     * @brief Set fold regions from LSP provider (bypasses internal detection)
     * @param regions Vector of fold regions from LSP
     */
    void SetFoldRegions(std::vector<FoldRegion> regions) {
        regions_ = std::move(regions);
        RebuildCache();
    }

    // Configuration accessors
    [[nodiscard]] auto& GetConfig() { return config_; }
    [[nodiscard]] const auto& GetConfig() const { return config_; }

    void SetEnabled(bool enabled) { config_.enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return config_.enabled; }

private:
    Config config_;
    std::vector<FoldRegion> regions_;

    // Cache: line -> region index
    std::unordered_map<int, size_t> line_to_region_;

    /**
     * @brief Detect fold regions using brace matching
     */
    std::vector<FoldRegion> DetectBraceRegions(const std::vector<std::string>& lines);

    /**
     * @brief Detect fold regions using indentation
     */
    std::vector<FoldRegion> DetectIndentationRegions(const std::vector<std::string>& lines);

    /**
     * @brief Get indentation level of a line
     */
    [[nodiscard]] int GetIndentLevel(const std::string& line) const;

    /**
     * @brief Check if line contains only opening brace
     */
    [[nodiscard]] bool IsOpeningBraceLine(const std::string& line) const;

    /**
     * @brief Rebuild the line-to-region cache
     */
    void RebuildCache();
};
