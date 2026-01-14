#include "TextEditorMinimap.hpp"

// Smooth lerp helper for animations
static float SmoothLerp(float current, float target, float speed)
{
    return current + (target - current) * std::min(1.0f, speed);
}

// Apply alpha to color
static ImU32 ApplyAlpha(ImU32 color, float alpha)
{
    ImVec4 value = vscode::colors::to_vec4(color);
    value.w = std::clamp(value.w * alpha, 0.0f, 1.0f);
    return ImGui::ColorConvertFloat4ToU32(value);
}

bool TextEditorMinimap::Render(const TextEditor& editor, const ImVec2& available_region)
{
    if (!config_.enabled)
        return false;

    // Smooth hover animation (persists across frames)
    static float hover_anim = 0.0f;

    imgui::scoped::StyleVar const minimap_vars{
        {ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f)},
        {ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)}
    };
    imgui::scoped::StyleColor const minimap_colors{
        {ImGuiCol_Border, vscode::colors::transparent},
        {ImGuiCol_ChildBg, vscode::colors::transparent}
    };

    // Create minimap child window
    ImVec2 minimap_size(config_.width, available_region.y);

    imgui::scoped::Child const minimap_child(
        "##minimap",
        minimap_size,
        ImGuiChildFlags_None,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
    );

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 minimap_pos = ImGui::GetWindowPos();
    ImVec2 minimap_content_size = ImGui::GetWindowSize();
    ImVec2 minimap_min = ImVec2(minimap_pos.x, minimap_pos.y);
    ImVec2 minimap_max = ImVec2(minimap_pos.x + minimap_content_size.x,
                                minimap_pos.y + minimap_content_size.y);

    float content_height = minimap_max.y - minimap_min.y;

    ImU32 bg_color = ApplyAlpha(vscode::colors::minimap_bg, config_.opacity_background);
    draw_list->AddRectFilled(minimap_min, minimap_max, bg_color);

    ImU32 border_accent = ApplyAlpha(vscode::colors::minimap_slider, 0.25f);
    draw_list->AddRectFilled(minimap_min, ImVec2(minimap_min.x + 1.5f, minimap_max.y), border_accent);

    // Get editor info
    int total_lines = editor.GetLineCount();
    int first_visible = const_cast<TextEditor&>(editor).GetFirstVisibleLine();
    int last_visible = const_cast<TextEditor&>(editor).GetLastVisibleLine();

    // Determine if window is hovered for hover effects
    bool is_hovered = ImGui::IsWindowHovered();

    // Animate hover state
    float target_hover_anim = is_hovered ? 1.0f : 0.0f;
    hover_anim = SmoothLerp(hover_anim, target_hover_anim, 0.15f);

    if (total_lines > 0)
    {
        // Draw code representation with syntax-aware coloring
        auto text_lines = editor.GetTextLines();
        float line_height = content_height / static_cast<float>(total_lines);
        float actual_line_h = std::max(1.0f, line_height * 0.75f);

        // Syntax colors (modern palette)
        ImU32 color_default  = ApplyAlpha(vscode::colors::minimap_text, config_.opacity_foreground);
        ImU32 color_string   = ApplyAlpha(vscode::colors::minimap_string, config_.opacity_foreground);
        ImU32 color_comment  = ApplyAlpha(vscode::colors::minimap_comment, config_.opacity_foreground);
        ImU32 color_number   = ApplyAlpha(vscode::colors::syntax_number, config_.opacity_foreground);
        ImU32 color_bracket  = ApplyAlpha(vscode::colors::minimap_bracket, config_.opacity_foreground);
        ImU32 color_type     = ApplyAlpha(vscode::colors::syntax_type, config_.opacity_foreground);

        for (int i = 0; i < total_lines && i < static_cast<int>(text_lines.size()); ++i)
        {
            const auto& line = text_lines[i];
            if (line.empty())
                continue;

            float y = minimap_min.y + (static_cast<float>(i) / static_cast<float>(total_lines)) * content_height;
            float x = minimap_min.x + 4.0f;
            float max_x = minimap_max.x - 4.0f;

            // Determine indentation
            size_t indent = 0;
            for (char c : line)
            {
                if (c == ' ') indent++;
                else if (c == '\t') indent += 4;
                else break;
            }
            x += static_cast<float>(indent) * 0.8f;

            // Simple syntax detection state
            bool in_string = false;
            bool in_comment = false;
            size_t char_idx = indent;

            while (char_idx < line.size() && x < max_x)
            {
                char c = line[char_idx];

                // Detect comments
                if (!in_string && char_idx + 1 < line.size() && line[char_idx] == '/' && line[char_idx + 1] == '/')
                {
                    in_comment = true;
                }

                // Detect strings
                if (!in_comment && (c == '"' || c == '\''))
                {
                    in_string = !in_string;
                }

                // Choose color based on context
                ImU32 char_color = color_default;
                if (in_comment)
                {
                    char_color = color_comment;
                }
                else if (in_string)
                {
                    char_color = color_string;
                }
                else if (std::isdigit(static_cast<unsigned char>(c)))
                {
                    char_color = color_number;
                }
                else if (c == '(' || c == ')' || c == '{' || c == '}' ||
                         c == '[' || c == ']' || c == '<' || c == '>')
                {
                    char_color = color_bracket;
                }
                else if (std::isupper(static_cast<unsigned char>(c)) && char_idx + 1 < line.size() &&
                         std::islower(static_cast<unsigned char>(line[char_idx + 1])))
                {
                    // Likely a type name (CamelCase)
                    char_color = color_type;
                }

                // Skip whitespace
                if (c != ' ' && c != '\t')
                {
                    float char_w = 1.2f;
                    draw_list->AddRectFilled(
                        ImVec2(x, y),
                        ImVec2(x + char_w, y + actual_line_h),
                        char_color
                    );
                }

                x += 1.0f;
                ++char_idx;
            }
        }

        // Draw hover highlight when hovering
        if (is_hovered && hovered_line_ >= 0 && hovered_line_ < total_lines)
        {
            float hover_y = minimap_min.y + (static_cast<float>(hovered_line_) / static_cast<float>(total_lines)) * content_height;

            // Animated hover indicator
            ImU32 hover_fill = ApplyAlpha(config_.hover_color, 0.2f * hover_anim);
            ImU32 hover_line_color = ApplyAlpha(vscode::colors::minimap_hover, 0.6f * hover_anim);

            // Highlight hovered line with glow
            float hover_h = std::max(3.0f, line_height);
            draw_list->AddRectFilled(
                ImVec2(minimap_min.x, hover_y - 1.0f),
                ImVec2(minimap_max.x, hover_y + hover_h + 1.0f),
                hover_fill
            );

            // Left accent line
            draw_list->AddRectFilled(
                ImVec2(minimap_min.x, hover_y),
                ImVec2(minimap_min.x + 2.0f, hover_y + hover_h),
                hover_line_color
            );
        }

        // Draw viewport indicator with modern styling
        if (config_.show_viewport_indicator && total_lines > 0)
        {
            float viewport_start_y = minimap_min.y + (static_cast<float>(first_visible) / static_cast<float>(total_lines)) * content_height;
            float viewport_end_y = minimap_min.y + (static_cast<float>(last_visible + 1) / static_cast<float>(total_lines)) * content_height;

            // Ensure minimum visible height
            float min_viewport_h = 20.0f;
            if (viewport_end_y - viewport_start_y < min_viewport_h)
            {
                viewport_end_y = viewport_start_y + min_viewport_h;
            }

            // Brighten viewport when hovered
            float viewport_opacity = is_hovered ? 0.25f : 0.15f;
            ImU32 viewport_fill = ApplyAlpha(config_.viewport_color, viewport_opacity);

            draw_list->AddRectFilled(
                ImVec2(minimap_min.x, viewport_start_y),
                ImVec2(minimap_max.x, viewport_end_y),
                viewport_fill,
                2.0f
            );

            // Subtle border with glow effect when hovered
            ImU32 viewport_border = ApplyAlpha(config_.viewport_color, is_hovered ? 0.7f : 0.4f);
            ImU32 viewport_accent = ApplyAlpha(config_.viewport_color, is_hovered ? 0.9f : 0.6f);

            // Left accent bar (VS Code style)
            draw_list->AddRectFilled(
                ImVec2(minimap_min.x, viewport_start_y),
                ImVec2(minimap_min.x + 2.5f, viewport_end_y),
                viewport_accent,
                1.0f
            );

            // Optional: subtle top/bottom edges
            draw_list->AddRectFilled(
                ImVec2(minimap_min.x + 2.0f, viewport_start_y),
                ImVec2(minimap_max.x, viewport_start_y + 1.0f),
                ApplyAlpha(viewport_border, 0.5f)
            );
            draw_list->AddRectFilled(
                ImVec2(minimap_min.x + 2.0f, viewport_end_y - 1.0f),
                ImVec2(minimap_max.x, viewport_end_y),
                ApplyAlpha(viewport_border, 0.5f)
            );
        }
    }

    // Handle input with better interaction
    bool clicked = false;
    if (is_hovered)
    {
        ImVec2 mouse_pos = ImGui::GetMousePos();
        float relative_y = mouse_pos.y - minimap_min.y;
        hovered_line_ = GetLineFromYPosition(relative_y, total_lines, content_height);

        // Change cursor to indicate clickable area
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            clicked = true;
            is_dragging_ = true;
            clicked_line_ = hovered_line_;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && is_dragging_)
        {
            clicked = true;
            clicked_line_ = hovered_line_;
        }
    }
    else
    {
        hovered_line_ = -1;
    }

    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
    {
        is_dragging_ = false;
    }

    return clicked;
}

int TextEditorMinimap::HandleInput([[maybe_unused]] TextEditor& editor,
                                   [[maybe_unused]] const ImVec2& minimap_min,
                                   [[maybe_unused]] const ImVec2& minimap_max)
{
    if (!config_.enabled)
        return -1;

    if (is_dragging_ && hovered_line_ >= 0)
    {
        return hovered_line_;
    }

    return -1;
}

float TextEditorMinimap::GetLineYPosition(int line, int total_lines, float minimap_height) const
{
    if (total_lines <= 0)
        return 0.0f;

    return (static_cast<float>(line) / static_cast<float>(total_lines)) * minimap_height;
}

int TextEditorMinimap::GetLineFromYPosition(float y, int total_lines, float minimap_height) const
{
    if (minimap_height <= 0.0f || total_lines <= 0)
        return -1;

    int line = static_cast<int>((y / minimap_height) * static_cast<float>(total_lines));
    return std::clamp(line, 0, total_lines - 1);
}
