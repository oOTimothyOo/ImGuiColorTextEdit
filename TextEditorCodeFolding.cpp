#include "TextEditorCodeFolding.hpp"
#include "TextEditor.h"
#include <algorithm>
#include <stack>
#include <cctype>

void TextEditorCodeFolding::AnalyzeDocument(const TextEditor& editor)
{
    if (!config_.enabled)
        return;

    auto lines = editor.GetTextLines();

    regions_.clear();
    line_to_region_.clear();

    std::vector<FoldRegion> detected_regions;

    if (config_.detection_mode == DetectionMode::Braces ||
        config_.detection_mode == DetectionMode::Both)
    {
        auto brace_regions = DetectBraceRegions(lines);
        detected_regions.insert(detected_regions.end(),
                              brace_regions.begin(), brace_regions.end());
    }

    if (config_.detection_mode == DetectionMode::Indentation ||
        config_.detection_mode == DetectionMode::Both)
    {
        auto indent_regions = DetectIndentationRegions(lines);
        detected_regions.insert(detected_regions.end(),
                              indent_regions.begin(), indent_regions.end());
    }

    // Filter by minimum lines
    for (const auto& region : detected_regions)
    {
        if ((region.end_line - region.start_line) >= config_.min_lines_to_fold)
        {
            regions_.push_back(region);
        }
    }

    // Sort by start line
    std::sort(regions_.begin(), regions_.end(),
             [](const FoldRegion& a, const FoldRegion& b) {
                 return a.start_line < b.start_line;
             });

    RebuildCache();
}

bool TextEditorCodeFolding::ToggleFold(int line)
{
    auto it = line_to_region_.find(line);
    if (it != line_to_region_.end() && it->second < regions_.size())
    {
        regions_[it->second].is_folded = !regions_[it->second].is_folded;
        return true;
    }
    return false;
}

bool TextEditorCodeFolding::Fold(int line)
{
    auto it = line_to_region_.find(line);
    if (it != line_to_region_.end() && it->second < regions_.size())
    {
        if (!regions_[it->second].is_folded)
        {
            regions_[it->second].is_folded = true;
            return true;
        }
    }
    return false;
}

bool TextEditorCodeFolding::Unfold(int line)
{
    auto it = line_to_region_.find(line);
    if (it != line_to_region_.end() && it->second < regions_.size())
    {
        if (regions_[it->second].is_folded)
        {
            regions_[it->second].is_folded = false;
            return true;
        }
    }
    return false;
}

void TextEditorCodeFolding::FoldAll()
{
    for (auto& region : regions_)
    {
        region.is_folded = true;
    }
}

void TextEditorCodeFolding::UnfoldAll()
{
    for (auto& region : regions_)
    {
        region.is_folded = false;
    }
}

bool TextEditorCodeFolding::IsLineHidden(int line) const
{
    for (const auto& region : regions_)
    {
        if (region.is_folded &&
            line > region.start_line &&
            line <= region.end_line)
        {
            return true;
        }
    }
    return false;
}

std::optional<TextEditorCodeFolding::FoldRegion>
TextEditorCodeFolding::GetRegionAtLine(int line) const
{
    auto it = line_to_region_.find(line);
    if (it != line_to_region_.end() && it->second < regions_.size())
    {
        return regions_[it->second];
    }
    return std::nullopt;
}

std::optional<TextEditorCodeFolding::FoldRegion>
TextEditorCodeFolding::GetRegionStartingAtLine(int line) const
{
    for (const auto& region : regions_)
    {
        if (region.start_line == line)
        {
            return region;
        }
    }
    return std::nullopt;
}

bool TextEditorCodeFolding::RenderFoldIcon(ImDrawList* draw_list, int line,
                                          const ImVec2& icon_pos, [[maybe_unused]] float line_height)
{
    if (!config_.enabled || !config_.show_fold_icons)
        return false;

    auto region_opt = GetRegionStartingAtLine(line);
    if (!region_opt)
        return false;

    const auto& region = *region_opt;

    // Icon rect
    ImVec2 icon_min = icon_pos;
    ImVec2 icon_max = ImVec2(icon_pos.x + config_.icon_size,
                             icon_pos.y + config_.icon_size);

    bool is_hovered = ImGui::IsMouseHoveringRect(icon_min, icon_max);
    bool is_clicked = is_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    ImU32 color = is_hovered ? config_.fold_icon_hover_color : config_.fold_icon_color;

    // Draw triangle (pointing right if folded, down if unfolded)
    ImVec2 center = ImVec2(icon_pos.x + config_.icon_size * 0.5f,
                          icon_pos.y + config_.icon_size * 0.5f);
    float size = config_.icon_size * 0.35f;

    if (region.is_folded)
    {
        // Right-pointing triangle
        draw_list->AddTriangleFilled(
            ImVec2(center.x - size * 0.5f, center.y - size),
            ImVec2(center.x - size * 0.5f, center.y + size),
            ImVec2(center.x + size, center.y),
            color
        );
    }
    else
    {
        // Down-pointing triangle
        draw_list->AddTriangleFilled(
            ImVec2(center.x - size, center.y - size * 0.5f),
            ImVec2(center.x + size, center.y - size * 0.5f),
            ImVec2(center.x, center.y + size),
            color
        );
    }

    return is_clicked;
}

float TextEditorCodeFolding::RenderFoldPlaceholder(ImDrawList* draw_list,
                                                   const FoldRegion& region,
                                                   const ImVec2& start_pos)
{
    if (!region.is_folded)
        return 0.0f;

    ImVec2 text_size = ImGui::CalcTextSize(config_.fold_placeholder);

    // Background
    draw_list->AddRectFilled(
        start_pos,
        ImVec2(start_pos.x + text_size.x + 4, start_pos.y + text_size.y),
        config_.placeholder_color
    );

    // Text
    draw_list->AddText(
        ImVec2(start_pos.x + 2, start_pos.y),
        ImGui::GetColorU32(ImGuiCol_Text),
        config_.fold_placeholder
    );

    return text_size.x + 4;
}

int TextEditorCodeFolding::VisualLineToActualLine(int visual_line) const
{
    int actual_line = visual_line;
    int hidden_lines = 0;

    for (const auto& region : regions_)
    {
        if (region.is_folded && actual_line >= region.start_line)
        {
            int region_hidden = region.end_line - region.start_line;
            if (actual_line <= region.start_line)
            {
                // Visual line is before this fold
                break;
            }
            hidden_lines += region_hidden;
            actual_line = visual_line + hidden_lines;
        }
    }

    return actual_line;
}

int TextEditorCodeFolding::ActualLineToVisualLine(int actual_line) const
{
    // Check if line is hidden
    if (IsLineHidden(actual_line))
        return -1;

    int visual_line = actual_line;

    for (const auto& region : regions_)
    {
        if (region.is_folded && actual_line > region.end_line)
        {
            visual_line -= (region.end_line - region.start_line);
        }
    }

    return visual_line;
}

std::vector<TextEditorCodeFolding::FoldRegion>
TextEditorCodeFolding::DetectBraceRegions(const std::vector<std::string>& lines)
{
    std::vector<FoldRegion> regions;
    std::stack<int> brace_stack;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
    {
        const auto& line = lines[i];

        for (char ch : line)
        {
            if (ch == '{')
            {
                brace_stack.push(i);
            }
            else if (ch == '}')
            {
                if (!brace_stack.empty())
                {
                    int start_line = brace_stack.top();
                    brace_stack.pop();

                    FoldRegion region;
                    region.start_line = start_line;
                    region.end_line = i;
                    region.indent_level = GetIndentLevel(lines[start_line]);
                    regions.push_back(region);
                }
            }
        }
    }

    return regions;
}

std::vector<TextEditorCodeFolding::FoldRegion>
TextEditorCodeFolding::DetectIndentationRegions(const std::vector<std::string>& lines)
{
    std::vector<FoldRegion> regions;

    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
    {
        int current_indent = GetIndentLevel(lines[i]);

        // Look ahead for lines with deeper indentation
        int end_line = i;
        for (int j = i + 1; j < static_cast<int>(lines.size()); ++j)
        {
            // Skip empty lines
            if (lines[j].empty() || std::all_of(lines[j].begin(), lines[j].end(), ::isspace))
                continue;

            int next_indent = GetIndentLevel(lines[j]);

            if (next_indent > current_indent)
            {
                end_line = j;
            }
            else
            {
                break;
            }
        }

        if (end_line > i)
        {
            FoldRegion region;
            region.start_line = i;
            region.end_line = end_line;
            region.indent_level = current_indent;
            regions.push_back(region);
        }
    }

    return regions;
}

int TextEditorCodeFolding::GetIndentLevel(const std::string& line) const
{
    int indent = 0;
    for (char ch : line)
    {
        if (ch == ' ')
            indent++;
        else if (ch == '\t')
            indent += 4;  // Assume tab = 4 spaces
        else
            break;
    }
    return indent;
}

bool TextEditorCodeFolding::IsOpeningBraceLine(const std::string& line) const
{
    // Simple heuristic: line ends with '{'
    auto it = std::find_if(line.rbegin(), line.rend(),
                          [](char ch) { return !std::isspace(ch); });

    return it != line.rend() && *it == '{';
}

void TextEditorCodeFolding::RebuildCache()
{
    line_to_region_.clear();

    for (size_t i = 0; i < regions_.size(); ++i)
    {
        line_to_region_[regions_[i].start_line] = i;
    }
}
