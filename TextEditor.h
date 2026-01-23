#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/regex.hpp>
#include "imgui.h"
#include "utilities/imgui_scoped.hpp"

class IMGUI_API TextEditor
{
public:
	// ------------- Exposed API ------------- //

	TextEditor();
	~TextEditor();

	enum class PaletteId
	{
		Dark, Light, Mariana, RetroBlue
	};
	enum class PaletteIndex
	{
		Default,
		Keyword,
		Number,
		String,
		CharLiteral,
		Punctuation,
		Preprocessor,
		Identifier,
		KnownIdentifier,
		PreprocIdentifier,
		Comment,
		MultiLineComment,
		// Semantic Highlighting - Basic
		Function,
		Type,
		Variable,
		Namespace,
		// Semantic Highlighting - Extended (for LSP modifiers)
		Constant,           // const, constexpr, readonly variables
		Parameter,          // Function parameters
		EnumMember,         // Enum values
		Property,           // Class/struct members
		Method,             // Member functions (distinct from free functions)
		StaticSymbol,       // Static members (variables or methods)
		Deprecated,         // Deprecated symbols (rendered with strikethrough)
		Macro,              // Preprocessor macros
		Label,              // goto labels
		Operator,           // Overloaded operators
		TypeParameter,      // Template parameters
		Concept,            // C++20 concepts

		Background,
		Cursor,
		Selection,
		ErrorMarker,
		ControlCharacter,
		Breakpoint,
		LineNumber,
		CurrentLineFill,
		CurrentLineFillInactive,
		CurrentLineEdge,
		Max
	};
	typedef std::array<ImU32, (unsigned)PaletteIndex::Max> Palette;
	enum class LanguageDefinitionId
	{
		None, Cpp, C, Cs, Python, Lua, Json, Sql, AngelScript, Glsl, Hlsl
	};
	enum class SetViewAtLineMode
	{
		FirstVisibleLine, Centered, LastVisibleLine
	};

	// Represents a character coordinate from the user's point of view,
	// i. e. consider an uniform grid (assuming fixed-width font) on the
	// screen as it is rendered, and each cell has its own coordinate, starting from 0.
	// Tabs are counted as [1..mTabSize] count empty spaces, depending on
	// how many space is necessary to reach the next tab stop.
	// For example, coordinate (1, 5) represents the character 'B' in a line "\tABC", when mTabSize = 4,
	// because it is rendered as "    ABC" on the screen.
	struct Coordinates
	{
		int mLine, mColumn;
		Coordinates() : mLine(0), mColumn(0) {}
		Coordinates(int aLine, int aColumn) : mLine(aLine), mColumn(aColumn)
		{
			assert(aLine >= 0);
			assert(aColumn >= 0);
		}
		static Coordinates Invalid() { static Coordinates invalid(-1, -1); return invalid; }

		bool operator ==(const Coordinates& o) const
		{
			return
				mLine == o.mLine &&
				mColumn == o.mColumn;
		}

		bool operator !=(const Coordinates& o) const
		{
			return
				mLine != o.mLine ||
				mColumn != o.mColumn;
		}

		bool operator <(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine < o.mLine;
			return mColumn < o.mColumn;
		}

		bool operator >(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine > o.mLine;
			return mColumn > o.mColumn;
		}

		bool operator <=(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine < o.mLine;
			return mColumn <= o.mColumn;
		}

		bool operator >=(const Coordinates& o) const
		{
			if (mLine != o.mLine)
				return mLine > o.mLine;
			return mColumn >= o.mColumn;
		}

		Coordinates operator -(const Coordinates& o)
		{
			return Coordinates(mLine - o.mLine, mColumn - o.mColumn);
		}

		Coordinates operator +(const Coordinates& o)
		{
			return Coordinates(mLine + o.mLine, mColumn + o.mColumn);
		}
	};

	inline void SetReadOnlyEnabled(bool aValue) { mReadOnly = aValue; }
	inline bool IsReadOnlyEnabled() const { return mReadOnly; }
	inline void SetAutoIndentEnabled(bool aValue) { mAutoIndent = aValue; }
	inline bool IsAutoIndentEnabled() const { return mAutoIndent; }
	inline void SetShowWhitespacesEnabled(bool aValue) { mShowWhitespaces = aValue; }
	inline bool IsShowWhitespacesEnabled() const { return mShowWhitespaces; }
	inline void SetShowLineNumbersEnabled(bool aValue) { mShowLineNumbers = aValue; }
	inline bool IsShowLineNumbersEnabled() const { return mShowLineNumbers; }
	inline void SetShortTabsEnabled(bool aValue) { mShortTabs = aValue; }
	inline bool IsShortTabsEnabled() const { return mShortTabs; }

	/**
	 * @brief Set whether Ctrl+Click should trigger go-to-definition instead of adding cursors.
	 *
	 * When enabled (default true for IDE mode), Ctrl+Click will not add cursors.
	 * Instead, it is handled externally by the LSP integration for navigation.
	 * Alt+Click can still be used for multi-cursor editing.
	 */
	inline void SetCtrlClickForNavigation(bool aValue) { mCtrlClickForNavigation = aValue; }
	inline bool IsCtrlClickForNavigation() const { return mCtrlClickForNavigation; }

	/**
	 * @brief Set a custom Tab handler.
	 *
	 * If the handler returns true, the default Tab insertion is skipped.
	 */
	void SetTabHandler(std::function<bool(bool)> handler);

	inline int GetLineCount() const { return static_cast<int>(mLines.size()); }
	void SetPalette(PaletteId aValue);
	void SetPalette(const Palette& aValue);
	PaletteId GetPalette() const { return mPaletteId; }
	void SetLanguageDefinition(LanguageDefinitionId aValue);
	LanguageDefinitionId GetLanguageDefinition() const { return mLanguageDefinitionId; };
	const char* GetLanguageDefinitionName() const;
	void SetTabSize(int aValue);
	inline int GetTabSize() const { return mTabSize; }
	void SetLineSpacing(float aValue);
	inline float GetLineSpacing() const { return mLineSpacing;  }

	inline static void SetDefaultPalette(PaletteId aValue) { defaultPalette = aValue; }
	inline static PaletteId GetDefaultPalette() { return defaultPalette; }

	void SelectAll();
	void SelectLine(int aLine);
	void SelectRegion(int aStartLine, int aStartChar, int aEndLine, int aEndChar);
	void SelectNextOccurrenceOf(const char* aText, int aTextSize, bool aCaseSensitive = true);
	void SelectAllOccurrencesOf(const char* aText, int aTextSize, bool aCaseSensitive = true);
	/**
	 * @brief Add a cursor one line above each existing cursor.
	 *
	 * Preserves column position when possible and mirrors single-line selections.
	 */
	void AddCursorAbove();
	/**
	 * @brief Add a cursor one line below each existing cursor.
	 *
	 * Preserves column position when possible and mirrors single-line selections.
	 */
	void AddCursorBelow();
	bool AnyCursorHasSelection() const;
	bool AllCursorsHaveSelection() const;
	void ClearExtraCursors();
	void ClearSelections();
	void SetCursorPosition(int aLine, int aCharIndex);
	inline void GetCursorPosition(int& outLine, int& outColumn) const
	{
		auto coords = GetSanitizedCursorCoordinates();
		outLine = coords.mLine;
		outColumn = coords.mColumn;
	}
	int GetFirstVisibleLine() const;
	int GetLastVisibleLine() const;
	void SetViewAtLine(int aLine, SetViewAtLineMode aMode);

	void Copy();
	void Cut();
	void Paste();
	void Undo(int aSteps = 1);
	void Redo(int aSteps = 1);
	inline bool CanUndo() const { return !mReadOnly && mUndoIndex > 0; };
	inline bool CanRedo() const { return !mReadOnly && mUndoIndex < (int)mUndoBuffer.size(); };
	inline int GetUndoIndex() const { return mUndoIndex; };

	void SetText(const std::string& aText);
	std::string GetText() const;

	void SetTextLines(const std::vector<std::string>& aLines);
	std::vector<std::string> GetTextLines() const;
	std::string GetSelectedText(int aCursor = -1) const;
	/**
	 * @brief Get selection start for a cursor (or the active cursor).
	 */
	inline Coordinates GetSelectionStart(int aCursor = -1) const
	{
		int cursor = (aCursor >= 0) ? aCursor : mState.mCurrentCursor;
		if (cursor < 0 || cursor >= (int)mState.mCursors.size())
			return Coordinates::Invalid();
		return mState.mCursors[cursor].GetSelectionStart();
	}
	/**
	 * @brief Get selection end for a cursor (or the active cursor).
	 */
	inline Coordinates GetSelectionEnd(int aCursor = -1) const
	{
		int cursor = (aCursor >= 0) ? aCursor : mState.mCurrentCursor;
		if (cursor < 0 || cursor >= (int)mState.mCursors.size())
			return Coordinates::Invalid();
		return mState.mCursors[cursor].GetSelectionEnd();
	}
	bool ReplaceRange(int aStartLine, int aStartChar, int aEndLine, int aEndChar, const char* aText, int aCursor = -1);

	struct Highlight
	{
		int mStartLine = 0;
		int mStartCharIndex = 0;
		int mEndLine = 0;
		int mEndCharIndex = 0;
		ImU32 mColor = 0;
	};

	void SetHighlights(const std::vector<Highlight>& aHighlights);
	void ClearHighlights();
	inline const std::vector<Highlight>& GetHighlights() const { return mHighlights; }

	enum class UnderlineStyle
	{
		Solid,
		Wavy
	};

	enum class DiagnosticSeverity
	{
		None = 0,
		Error = 1,
		Warning = 2,
		Information = 3,
		Hint = 4
	};

	struct Underline
	{
		int mStartLine = 0;
		int mStartColumn = 0;
		int mEndLine = 0;
		int mEndColumn = 0;
		ImU32 mColor = 0;
		UnderlineStyle mStyle = UnderlineStyle::Wavy;
		DiagnosticSeverity mSeverity = DiagnosticSeverity::None; // For gutter icons
	};

	struct SemanticToken
	{
		int mLine = 0;
		int mStartChar = 0;
		int mLength = 0;
		std::string mType;
		std::vector<std::string> mModifiers;
	};

	void SetUnderlines(const std::vector<Underline>& aUnderlines);
	void ClearUnderlines();
	inline const std::vector<Underline>& GetUnderlines() const { return mUnderlines; }

	void SetSemanticTokens(const std::vector<SemanticToken>& aTokens);
	void ClearSemanticTokens();
	void ReapplySemanticTokens(); // Re-apply stored tokens (call after colorization)

	// Convert screen position to text coordinates (for hover tooltips, etc.)
	Coordinates ScreenPosToCoordinates(const ImVec2& aPosition, bool* isOverLineNumber = nullptr) const;
	// Convert text coordinates to screen position (for inlay hints, overlays, etc.)
	ImVec2 CoordinatesToScreenPos(const Coordinates& aPosition) const;
	// Line height in pixels for the current font/spacing.
	float GetLineHeight() const;
	/**
	 * @brief Get the pixel offset where text content begins (after the gutter).
	 */
	[[nodiscard]] float GetTextStart() const { return mTextStart; }

	// Convert character index (byte offset) to visual column (accounts for tabs)
	int CharacterIndexToColumn(int aLine, int aCharIndex) const;

	// Convert visual column to character index (reverse of CharacterIndexToColumn)
	int ColumnToCharacterIndex(int aLine, int aColumn) const;

	int GetLineMaxColumn(int aLine, int aLimit = -1) const;

	// Get current scroll position (for detecting scroll changes)
	ImVec2 GetScrollPosition() const { return ImVec2(mScrollX, mScrollY); }

	// =========================================================================
	// Link Hover Support (for Ctrl+Click navigation)
	// =========================================================================

	/**
	 * @brief Represents a clickable link region (e.g., for Ctrl+Click navigation).
	 */
	struct LinkHighlight
	{
		int mLine = 0;
		int mStartCharIndex = 0;
		int mEndCharIndex = 0;
		ImU32 mColor = 0;
		bool mUnderline = true;
	};

	/**
	 * @brief Set the current link highlight (word under cursor when Ctrl is held).
	 *
	 * Pass empty optional to clear the link highlight.
	 */
	void SetLinkHighlight(const std::optional<LinkHighlight>& aLink);

	/**
	 * @brief Clear the link highlight.
	 */
	void ClearLinkHighlight();

	/**
	 * @brief Check if there is an active link highlight.
	 */
	[[nodiscard]] bool HasLinkHighlight() const { return mLinkHighlight.has_value(); }

	/**
	 * @brief Get the current link highlight if any.
	 */
	[[nodiscard]] const std::optional<LinkHighlight>& GetLinkHighlight() const { return mLinkHighlight; }

	/**
	 * @brief Find word boundaries at a given position.
	 *
	 * Returns the start and end character indices of the word at the position,
	 * or {index, index} when the position is not on an identifier.
	 * Useful for highlighting the entire word when Ctrl+hovering for navigation.
	 */
	[[nodiscard]] std::pair<int, int> GetWordBoundaries(int aLine, int aCharIndex) const;

	/**
	 * @brief Callback invoked inside the editor child window after rendering text.
	 */
	using render_callback = std::function<void()>;

	/**
	 * @brief Render the editor inside a child window.
	 *
	 * @param aTitle Child window title/ID.
	 * @param aParentIsFocused True if the parent window is focused.
	 * @param aSize Requested child size (0 = auto).
	 * @param aBorder Whether to draw a border around the child.
	 * @param aCallback Optional callback invoked before EndChild().
	 */
	bool Render(const char* aTitle,
	            bool aParentIsFocused = false,
	            const ImVec2& aSize = ImVec2(),
	            bool aBorder = false,
	            const render_callback& aCallback = {});

	void ImGuiDebugPanel(const std::string& panelName = "Debug");
	void UnitTests();

	void SetSelection(Coordinates aStart, Coordinates aEnd, int aCursor = -1);
	void SetSelection(int aStartLine, int aStartChar, int aEndLine, int aEndChar, int aCursor = -1);


private:
	// ------------- Generic utils ------------- //

	static inline ImVec4 U32ColorToVec4(ImU32 in)
	{
		float s = 1.0f / 255.0f;
		return ImVec4(
			((in >> IM_COL32_A_SHIFT) & 0xFF) * s,
			((in >> IM_COL32_B_SHIFT) & 0xFF) * s,
			((in >> IM_COL32_G_SHIFT) & 0xFF) * s,
			((in >> IM_COL32_R_SHIFT) & 0xFF) * s);
	}
	static inline bool IsUTFSequence(char c)
	{
		return (c & 0xC0) == 0x80;
	}
	static inline float Distance(const ImVec2& a, const ImVec2& b)
	{
		float x = a.x - b.x;
		float y = a.y - b.y;
		return sqrt(x * x + y * y);
	}
	template<typename T>
	static inline T Max(T a, T b) { return a > b ? a : b; }
	template<typename T>
	static inline T Min(T a, T b) { return a < b ? a : b; }

	// ------------- Internal ------------- //

	// Coordinates struct moved to public section

	struct Cursor
	{
		Coordinates mInteractiveStart = { 0, 0 };
		Coordinates mInteractiveEnd = { 0, 0 };
		[[nodiscard]] auto GetSelectionStart() const -> Coordinates { return mInteractiveStart < mInteractiveEnd ? mInteractiveStart : mInteractiveEnd; }
		[[nodiscard]] auto GetSelectionEnd() const -> Coordinates { return mInteractiveStart > mInteractiveEnd ? mInteractiveStart : mInteractiveEnd; }
		[[nodiscard]] auto HasSelection() const -> bool { return mInteractiveStart != mInteractiveEnd; }
	};

	struct EditorState // state to be restored with undo/redo
	{
		int mCurrentCursor = 0;
		int mLastAddedCursor = 0;
		std::vector<Cursor> mCursors = { {{0,0}} };
		void AddCursor();
		auto GetLastAddedCursorIndex() -> int;
		void SortCursorsFromTopToBottom();
	};

	struct Identifier
	{
		Coordinates mLocation;
		std::string mDeclaration;
	};

	using Identifiers = std::unordered_map<std::string, Identifier>;
	struct Glyph
	{
		char mChar;
		PaletteIndex mColorIndex = PaletteIndex::Default;
		bool mComment : 1;
		bool mMultiLineComment : 1;
		bool mPreprocessor : 1;
		// Style flags for semantic highlighting (from LSP modifiers)
		bool mItalic : 1;        // For abstract, virtual, parameter
		bool mBold : 1;          // For declaration, definition
		bool mUnderline : 1;     // For static symbols
		bool mStrikethrough : 1; // For deprecated symbols

		Glyph(char aChar, PaletteIndex aColorIndex) : mChar(aChar), mColorIndex(aColorIndex),
			mComment(false), mMultiLineComment(false), mPreprocessor(false),
			mItalic(false), mBold(false), mUnderline(false), mStrikethrough(false) {}
	};

	using Line = std::vector<Glyph>;

	struct LanguageDefinition
	{
		using TokenRegexString = std::pair<std::string, PaletteIndex>;
		using TokenizeCallback = bool(*)(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end, PaletteIndex& paletteIndex);

		std::string mName;
		std::unordered_set<std::string> mKeywords;
		Identifiers mIdentifiers;
		Identifiers mPreprocIdentifiers;
		std::string mCommentStart, mCommentEnd, mSingleLineComment;
		char mPreprocChar = '#';
		TokenizeCallback mTokenize = nullptr;
		std::vector<TokenRegexString> mTokenRegexStrings;
		bool mCaseSensitive = true;

		static const LanguageDefinition& Cpp();
		static const LanguageDefinition& Hlsl();
		static const LanguageDefinition& Glsl();
		static const LanguageDefinition& Python();
		static const LanguageDefinition& C();
		static const LanguageDefinition& Sql();
		static const LanguageDefinition& AngelScript();
		static const LanguageDefinition& Lua();
		static const LanguageDefinition& Cs();
		static const LanguageDefinition& Json();
	};

	enum class UndoOperationType { Add, Delete };
	struct UndoOperation
	{
		std::string mText;
		TextEditor::Coordinates mStart;
		TextEditor::Coordinates mEnd;
		UndoOperationType mType;
	};


	class UndoRecord
	{
	public:
		UndoRecord() {}
		~UndoRecord() {}

		UndoRecord(
			const std::vector<UndoOperation>& aOperations,
			TextEditor::EditorState& aBefore,
			TextEditor::EditorState& aAfter);

		void Undo(TextEditor* aEditor);
		void Redo(TextEditor* aEditor);

		std::vector<UndoOperation> mOperations;

		EditorState mBefore;
		EditorState mAfter;
	};

	[[nodiscard]] auto GetText(const Coordinates& aStart, const Coordinates& aEnd) const -> std::string;
	[[nodiscard]] auto GetClipboardText() const -> std::string;

	void SetCursorPosition(const Coordinates& aPosition, int aCursor = -1, bool aClearSelection = true);

	int InsertTextAt(Coordinates& aWhere, const char* aValue);
	void InsertTextAtCursor(const char* aValue, int aCursor = -1);

	enum class MoveDirection : std::uint8_t { Right = 0, Left = 1, Up = 2, Down = 3 };
	bool Move(int& aLine, int& aCharIndex, bool aLeft = false, bool aLockLine = false) const;
	void MoveCharIndexAndColumn(int aLine, int& aCharIndex, int& aColumn) const;
	void MoveCoords(Coordinates& aCoords, MoveDirection aDirection, bool aWordMode = false, int aLineCount = 1) const;

	void MoveUp(int aAmount = 1, bool aSelect = false);
	void MoveDown(int aAmount = 1, bool aSelect = false);
	void MoveLeft(bool aSelect = false, bool aWordMode = false);
	void MoveRight(bool aSelect = false, bool aWordMode = false);
	void MoveTop(bool aSelect = false);
	void MoveBottom(bool aSelect = false);
	void MoveHome(bool aSelect = false);
	void MoveEnd(bool aSelect = false);
	void EnterCharacter(ImWchar aChar, bool aShift);
	void Backspace(bool aWordMode = false);
	void Delete(bool aWordMode = false, const EditorState* aEditorState = nullptr);

	void SelectNextOccurrenceOf(const char* aText, int aTextSize, int aCursor = -1, bool aCaseSensitive = true);
	void AddCursorForNextOccurrence(bool aCaseSensitive = true);
	void AddCursorsWithLineOffset(int aLineOffset);
	bool FindNextOccurrence(const char* aText, int aTextSize, const Coordinates& aFrom, Coordinates& outStart, Coordinates& outEnd, bool aCaseSensitive = true);
	bool FindMatchingBracket(int aLine, int aCharIndex, Coordinates& out);
	void ChangeCurrentLinesIndentation(bool aIncrease);
	void MoveUpCurrentLines();
	void MoveDownCurrentLines();
	void ToggleLineComment();
	void RemoveCurrentLines();

	float TextDistanceToLineStart(const Coordinates& aFrom, bool aSanitizeCoords = true) const;
	void EnsureCursorVisible(int aCursor = -1, bool aStartToo = false);

	Coordinates SanitizeCoordinates(const Coordinates& aValue) const;
	Coordinates GetSanitizedCursorCoordinates(int aCursor = -1, bool aStart = false) const;
	// ScreenPosToCoordinates moved to public section
	Coordinates FindWordStart(const Coordinates& aFrom) const;
	Coordinates FindWordEnd(const Coordinates& aFrom) const;
	int GetCharacterIndexFromColumn(const Coordinates& aCoordinates, bool aLeftLean) const;
	int GetCharacterIndexL(const Coordinates& aCoordinates) const;
	int GetCharacterIndexR(const Coordinates& aCoordinates) const;
	int GetCharacterColumn(int aLine, int aIndex) const;
	int GetFirstVisibleCharacterIndex(int aLine) const;

	Line& InsertLine(int aIndex);
	void RemoveLine(int aIndex, const std::unordered_set<int>* aHandledCursors = nullptr);
	void RemoveLines(int aStart, int aEnd);
	void DeleteRange(const Coordinates& aStart, const Coordinates& aEnd);
	void DeleteSelection(int aCursor = -1);

	void RemoveGlyphsFromLine(int aLine, int aStartChar, int aEndChar = -1);
	void AddGlyphsToLine(int aLine, int aTargetIndex, Line::iterator aSourceStart, Line::iterator aSourceEnd);
	void AddGlyphToLine(int aLine, int aTargetIndex, Glyph aGlyph);
	ImU32 GetGlyphColor(const Glyph& aGlyph) const;

	void HandleKeyboardInputs(bool aParentIsFocused = false);
	void HandleMouseInputs();
	void UpdateViewVariables(float aScrollX, float aScrollY);
	void Render(bool aParentIsFocused = false);

	void OnCursorPositionChanged();
	void OnLineChanged(bool aBeforeChange, int aLine, int aColumn, int aCharCount, bool aDeleted);
	void MergeCursorsIfPossible();

	void AddUndo(UndoRecord& aValue);

	void Colorize(int aFromLine = 0, int aCount = -1);
	void ColorizeRange(int aFromLine = 0, int aToLine = 0);
	void ColorizeInternal();

	std::vector<Line> mLines;
	EditorState mState;
	std::vector<UndoRecord> mUndoBuffer;
	int mUndoIndex = 0;

	int mTabSize = 4;
	float mLineSpacing = 1.0f;
	bool mReadOnly = false;
	bool mAutoIndent = true;
	bool mShowWhitespaces = true;
	bool mShowLineNumbers = true;
	bool mShortTabs = false;
	bool mCtrlClickForNavigation = true; // When true, Ctrl+Click does not add cursors (for LSP navigation)

	int mSetViewAtLine = -1;
	SetViewAtLineMode mSetViewAtLineMode;
	int mEnsureCursorVisible = -1;
	bool mEnsureCursorVisibleStartToo = false;
	bool mScrollToTop = false;

	float mTextStart = 20.0f; // position (in pixels) where a code line starts relative to the left of the TextEditor.
	int mLeftMargin = 10;
	ImVec2 mCharAdvance;
	float mCurrentSpaceHeight = 20.0f;
	float mCurrentSpaceWidth = 20.0f;
	float mLastClickTime = -1.0f;
	ImVec2 mLastClickPos;
	int mFirstVisibleLine = 0;
	int mLastVisibleLine = 0;
	int mVisibleLineCount = 0;
	int mFirstVisibleColumn = 0;
	int mLastVisibleColumn = 0;
	int mVisibleColumnCount = 0;
	float mContentWidth = 0.0f;
	float mContentHeight = 0.0f;
	float mScrollX = 0.0f;
	float mScrollY = 0.0f;
	ImVec2 mEditorScreenPos = ImVec2(0.0f, 0.0f);  // Cached screen position of content origin (scroll neutral)
	bool mPanning = false;
	bool mDraggingSelection = false;
	ImVec2 mLastMousePos;
	bool mCursorPositionChanged = false;
	bool mCursorOnBracket = false;
	Coordinates mMatchingBracketCoords;

	int mColorRangeMin = 0;
	int mColorRangeMax = 0;
	bool mCheckComments = true;
	PaletteId mPaletteId;
	Palette mPalette;
	LanguageDefinitionId mLanguageDefinitionId;
	const LanguageDefinition* mLanguageDefinition = nullptr;
	std::vector<Highlight> mHighlights;
	std::vector<Underline> mUnderlines;
	std::vector<SemanticToken> mSemanticTokens; // Stored for re-application after colorization
	std::optional<LinkHighlight> mLinkHighlight; // Ctrl+hover link highlight

	inline bool IsHorizontalScrollbarVisible() const { return mCurrentSpaceWidth > mContentWidth; }
	inline bool IsVerticalScrollbarVisible() const { return mCurrentSpaceHeight > mContentHeight; }
	inline int TabSizeAtColumn(int aColumn) const { return mTabSize - (aColumn % mTabSize); }

	static const Palette& GetDarkPalette();
	static const Palette& GetMarianaPalette();
	static const Palette& GetLightPalette();
	static const Palette& GetRetroBluePalette();
	static const std::unordered_map<char, char> OPEN_TO_CLOSE_CHAR;
	static const std::unordered_map<char, char> CLOSE_TO_OPEN_CHAR;
	static PaletteId defaultPalette;

private:
    struct RegexList;
    std::shared_ptr<RegexList> mRegexList;
};
