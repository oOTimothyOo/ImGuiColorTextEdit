#pragma once

#include "TextEditor.h"
#include "imgui.h"
#include "foundation/imgui/imgui_scoped.hpp"
#include "ui_kit/colors.hpp"
#include <algorithm>
#include <cctype>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

/**
 * @brief Autocomplete/IntelliSense system for TextEditor
 *
 * Provides VSCode-style intelligent code completion:
 * - Trigger on specific characters (., ::, ->, etc.)
 * - Multiple completion providers
 * - Fuzzy matching
 * - Documentation preview
 * - Icon/kind display
 * - Keyboard and mouse navigation
 */
class TextEditorAutocomplete
{
public:
    enum class CompletionItemKind
    {
        Text,
        Method,
        Function,
        Constructor,
        Field,
        Variable,
        Class,
        Interface,
        Module,
        Property,
        Unit,
        Value,
        Enum,
        Keyword,
        Snippet,
        Color,
        File,
        Reference,
        Folder,
        EnumMember,
        Constant,
        Struct,
        Event,
        Operator,
        TypeParameter
    };

    struct CompletionItem
    {
        std::string label;                    // Display text
        std::string insert_text;              // Text to insert (may differ from label)
        std::string detail;                   // Short description
        std::string documentation;            // Full documentation
        CompletionItemKind kind = CompletionItemKind::Text;
        int priority = 0;                     // For sorting (higher = better)
        std::string filter_text;              // Text used for filtering (default: label)
        std::string sort_text;                // LSP sortText: server-side ranking (lower = higher rank, lex compare)
        int replace_start_char = -1;          // Replacement range start on trigger line
        int replace_end_char = -1;            // Replacement range end on trigger line

        CompletionItem() = default;
        explicit CompletionItem(std::string lbl)
            : label(std::move(lbl)), insert_text(label), filter_text(label) {}
    };

    /// Pure ranking score for a (item, filter) pair. Higher is better.
    /// Combines case-sensitive/insensitive prefix match, length penalty for
    /// long labels with the same prefix match, and kind bias (functions and
    /// methods slightly above variables, which sit above plain text / snippets).
    /// Exposed for unit testing.
    [[nodiscard]] static auto LocalityScore(const CompletionItem& item, std::string_view filter) -> int;

    enum class AsyncStatusKind
    {
        Idle,
        Loading,
        Empty,
        Error
    };

    struct AsyncStatus
    {
        AsyncStatusKind kind = AsyncStatusKind::Idle;
        std::string message;
    };

    /**
     * @brief Interface for completion providers
     *
     * Implement this to provide custom completions (keywords, APIs, etc.)
     */
    class ICompletionProvider
    {
    public:
        virtual ~ICompletionProvider() = default;

        /**
         * @brief Get completions for a given position
         * @param editor The text editor
         * @param line Line number
         * @param column Column number
         * @param trigger_char Character that triggered completion (or '\0' for manual trigger)
         * @return List of completion items
         */
        [[nodiscard]] virtual std::vector<CompletionItem> GetCompletions(
            const TextEditor& editor,
            int line,
            int column,
            char trigger_char) = 0;

        /**
         * @brief Get trigger characters for this provider
         * @return Characters that should trigger completion
         */
        [[nodiscard]] virtual std::vector<char> GetTriggerCharacters() const = 0;

        [[nodiscard]] virtual AsyncStatus GetAsyncStatus() const { return {}; }

        virtual void CancelAsyncRequests() {}
    };

    struct Config
    {
        bool enabled = true;
        bool auto_trigger = true;              // Auto-trigger on trigger chars
        bool fuzzy_matching = true;            // Allow fuzzy matching
        int max_items = 20;                    // Max items to show
        int min_chars_to_trigger = 1;          // Min characters before showing
        float popup_width = 400.0f;
        float popup_max_height = 300.0f;
        bool show_documentation = true;
        bool show_icons = true;

        // Trigger characters (in addition to provider-specific ones)
        std::vector<char> global_trigger_chars = {'.', ':', '>'};

        // Commit characters: typing any of these while the popup is open with
        // a valid selection commits the selected item AND lets the character
        // type into the editor afterwards. Empty list disables the behavior.
        // Defaults are conservative C-like token enders that rarely appear
        // mid-identifier; identifier chars (letters, digits, _) are NEVER
        // commit chars regardless of this list.
        std::vector<char> commit_chars = {'.', ';', '(', ',', '=', ':', '>', '<'};
    };

    TextEditorAutocomplete() : config_() {}
    explicit TextEditorAutocomplete(Config config) : config_(std::move(config)) {}
    ~TextEditorAutocomplete() = default;

    // Non-copyable
    TextEditorAutocomplete(const TextEditorAutocomplete&) = delete;
    TextEditorAutocomplete& operator=(const TextEditorAutocomplete&) = delete;

    // Movable
    TextEditorAutocomplete(TextEditorAutocomplete&&) noexcept = default;
    TextEditorAutocomplete& operator=(TextEditorAutocomplete&&) noexcept = default;

    /**
     * @brief Register a completion provider
     */
    void RegisterProvider(std::unique_ptr<ICompletionProvider> provider);

    /**
     * @brief Remove all registered completion providers.
     */
    void ClearProviders();

    /**
     * @brief Trigger autocomplete at current cursor position
     * @param editor The text editor
     * @param trigger_char Character that triggered (or '\0' for manual)
     */
    void Trigger(const TextEditor& editor, char trigger_char = '\0');

    /**
     * @brief Render the autocomplete popup
     * @param editor The text editor
     * @return true if an item was selected
     */
    [[nodiscard]] bool Render(TextEditor& editor);

    /**
     * @brief Handle keyboard input for autocomplete
     * @return true if input was handled
     */
    [[nodiscard]] bool HandleKeyboard();

    /**
     * @brief If a commit character is queued for the editor and an item is
     * selected, accept the item but DO NOT consume the character — the caller
     * should return false from its keyboard interceptor so the character flows
     * through to the editor normally (and gets typed right after the inserted
     * completion). Returns true if a commit happened.
     */
    [[nodiscard]] bool TryCommitOnCharacter(TextEditor& editor);

    /**
     * @brief Decide whether the head of @p input_chars should trigger a
     * commit. Pure helper, exposed for unit testing. Returns true iff:
     *   1. is_active && there's a valid selection,
     *   2. AND scanning the input queue front-to-back: an identifier char
     *      (alnum/_) seen before any commit char vetoes the commit (those
     *      identifier chars should keep filtering the popup),
     *   3. AND a char in @p commit_chars appears in the queue.
     */
    [[nodiscard]] static auto ShouldCommitFromInputQueue(
        bool is_active,
        bool has_selection,
        std::span<const ImWchar> input_chars,
        std::span<const char> commit_chars) -> bool;

    /**
     * @brief Check if autocomplete is currently active
     */
    [[nodiscard]] bool IsActive() const { return is_active_; }

    [[nodiscard]] AsyncStatus GetCurrentAsyncStatus() const { return async_status_; }

    /**
     * @brief Close the autocomplete popup
     */
    void Close();

    void CancelAsyncRequests();

    /**
     * @brief Accept the currently selected completion
     * @param editor The text editor
     * @return The accepted completion item, or nullopt if none
     */
    [[nodiscard]] std::optional<CompletionItem> AcceptSelected(TextEditor& editor);

    // Configuration accessors
    [[nodiscard]] auto& GetConfig() { return config_; }
    [[nodiscard]] const auto& GetConfig() const { return config_; }

    void SetEnabled(bool enabled) { config_.enabled = enabled; }
    [[nodiscard]] bool IsEnabled() const { return config_.enabled; }

private:
    Config config_;
    std::vector<std::unique_ptr<ICompletionProvider>> providers_;

    bool is_active_ = false;
    std::vector<CompletionItem> current_items_;
    std::vector<CompletionItem> filtered_items_;
    AsyncStatus async_status_{};
    int selected_index_ = 0;
    std::string filter_text_;

    int trigger_line_ = -1;
    int trigger_column_ = -1;

    /**
     * @brief Filter completion items based on current input
     */
    void FilterCompletions(const std::string& filter);

    /**
     * @brief Get fuzzy match score for a completion item
     * @return Score (higher is better), or -1 if no match
     */
    [[nodiscard]] int GetFuzzyMatchScore(const CompletionItem& item, const std::string& filter) const;

    /**
     * @brief Get icon for completion item kind
     */
    [[nodiscard]] const char* GetIconForKind(CompletionItemKind kind) const;

    /**
     * @brief Get color for completion item kind
     */
    [[nodiscard]] ImU32 GetColorForKind(CompletionItemKind kind) const;

    /**
     * @brief Render a single completion item
     */
    void RenderCompletionItem(const CompletionItem& item, bool is_selected);
};

/**
 * @brief Simple keyword-based completion provider
 */
class KeywordCompletionProvider : public TextEditorAutocomplete::ICompletionProvider
{
public:
    explicit KeywordCompletionProvider(std::vector<std::string> keywords);

    [[nodiscard]] std::vector<TextEditorAutocomplete::CompletionItem> GetCompletions(
        const TextEditor& editor,
        int line,
        int column,
        char trigger_char) override;

    [[nodiscard]] std::vector<char> GetTriggerCharacters() const override;

private:
    std::vector<std::string> keywords_;
};
