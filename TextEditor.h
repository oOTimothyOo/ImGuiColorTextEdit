#pragma once

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <map>
#include <boost/regex.hpp>
#include "imgui.h"

class IMGUI_API TextEditor
{
public:
	// ------------- Exposed API ------------- //

	TextEditor();
	~TextEditor();

	enum class PaletteId
	{
		Dark,
		Light,
		Mariana,
		RetroBlue
	};
	enum class LanguageDefinitionId
	{
		None, Cpp, C, Cs, Python, Lua, Json, Sql, AngelScript, Glsl, Hlsl
	};

	inline void SetReadOnlyEnabled(bool aValue) { mReadOnly = aValue; }
	inline bool IsReadOnlyEnabled() const { return mReadOnly; }
	inline void SetAutoIndentEnabled(bool aValue) { mAutoIndent = aValue; }
	inline bool IsAutoIndentEnabled() const { return mAutoIndent; }
	inline void SetShowWhitespacesEnabled(bool aValue) { mShowWhitespaces = aValue; }
	inline bool IsShowWhitespacesEnabled() const { return mShowWhitespaces; }
	inline int GetLineCount() const { return mLines.size(); }
	inline bool IsOverwriteEnabled() const { return mOverwrite; }
	void SetPalette(PaletteId aValue);
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
	bool AnyCursorHasSelection() const;
	bool AllCursorsHaveSelection() const;
	void ClearExtraCursors();
	void ClearSelections();
	void SetCursorPosition(int aLine, int aCharIndex);
	inline void GetCursorPosition(int& outLine, int& outColumn) const
	{
		auto coords = GetActualCursorCoordinates();
		outLine = coords.mLine;
		outColumn = coords.mColumn;
	}

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

	bool Render(const char* aTitle, bool aParentIsFocused = false, const ImVec2& aSize = ImVec2(), bool aBorder = false);

	void ImGuiDebugPanel(const std::string& panelName = "Debug");
	void UnitTests();

private:
	// ------------- Generic utils ------------- //

	inline ImVec4 U32ColorToVec4(ImU32 in)
	{
		float s = 1.0f / 255.0f;
		return ImVec4(
			((in >> IM_COL32_A_SHIFT) & 0xFF) * s,
			((in >> IM_COL32_B_SHIFT) & 0xFF) * s,
			((in >> IM_COL32_G_SHIFT) & 0xFF) * s,
			((in >> IM_COL32_R_SHIFT) & 0xFF) * s);
	}
	inline bool IsUTFSequence(char c) const
	{
		return (c & 0xC0) == 0x80;
	}
	template<typename T>
	inline T Max(T a, T b) { return a > b ? a : b; }
	template<typename T>
	inline T Min(T a, T b) { return a < b ? a : b; }

	// ------------- Internal ------------- //

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

	struct Cursor
	{
		Coordinates mInteractiveStart = { 0, 0 };
		Coordinates mInteractiveEnd = { 0, 0 };
		inline Coordinates GetSelectionStart() const { return mInteractiveStart < mInteractiveEnd ? mInteractiveStart : mInteractiveEnd; }
		inline Coordinates GetSelectionEnd() const { return mInteractiveStart > mInteractiveEnd ? mInteractiveStart : mInteractiveEnd; }
		inline bool HasSelection() const { return mInteractiveStart != mInteractiveEnd; }
	};

	struct EditorState
	{
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
		bool mPanning = false;
		bool mDraggingSelection = false;
		ImVec2 mLastMousePos;
		int mCurrentCursor = 0;
		int mLastAddedCursor = 0;
		bool mCursorPositionChanged = false;
		std::vector<Cursor> mCursors = { {{0,0}} };
		void AddCursor()
		{
			// vector is never resized to smaller size, mCurrentCursor points to last available cursor in vector
			mCurrentCursor++;
			mCursors.resize(mCurrentCursor + 1);
			mLastAddedCursor = mCurrentCursor;
		}
		int GetLastAddedCursorIndex()
		{
			return mLastAddedCursor > mCurrentCursor ? 0 : mLastAddedCursor;
		}
		void SortCursorsFromTopToBottom()
		{
			Coordinates lastAddedCursorPos = mCursors[GetLastAddedCursorIndex()].mInteractiveEnd;
			std::sort(mCursors.begin(), mCursors.begin() + (mCurrentCursor + 1), [](const Cursor& a, const Cursor& b) -> bool
				{
					return a.GetSelectionStart() < b.GetSelectionStart();
				});
			// update last added cursor index to be valid after sort
			for (int c = mCurrentCursor; c > -1; c--)
				if (mCursors[c].mInteractiveEnd == lastAddedCursorPos)
					mLastAddedCursor = c;
		}
	};

	struct Identifier
	{
		Coordinates mLocation;
		std::string mDeclaration;
	};

	typedef std::unordered_map<std::string, Identifier> Identifiers;
	typedef std::array<ImU32, (unsigned)PaletteIndex::Max> Palette;

	struct Glyph
	{
		char mChar;
		PaletteIndex mColorIndex = PaletteIndex::Default;
		bool mComment : 1;
		bool mMultiLineComment : 1;
		bool mPreprocessor : 1;

		Glyph(char aChar, PaletteIndex aColorIndex) : mChar(aChar), mColorIndex(aColorIndex),
			mComment(false), mMultiLineComment(false), mPreprocessor(false) {}
	};

	typedef std::vector<Glyph> Line;

	struct LanguageDefinition
	{
		typedef std::pair<std::string, PaletteIndex> TokenRegexString;
		typedef bool(*TokenizeCallback)(const char* in_begin, const char* in_end, const char*& out_begin, const char*& out_end, PaletteIndex& paletteIndex);

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

	typedef std::vector<std::pair<boost::regex, PaletteIndex>> RegexList;

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

	std::string GetClipboardText() const;
	std::string GetSelectedText(int aCursor = -1) const;
	std::string GetCurrentLineText()const;

	void OnCursorPositionChanged();

	void SetCursorPosition(const Coordinates& aPosition, int aCursor = -1, bool aClearSelection = true);

	void InsertText(const std::string& aValue, int aCursor = -1);
	void InsertText(const char* aValue, int aCursor = -1);

	enum class MoveDirection { Right = 0, Left = 1, Up = 2, Down = 3 };
	bool Move(int& aLine, int& aCharIndex, bool aLeft = false, bool aLockLine = false) const;
	void MoveCoords(Coordinates& aCoords, MoveDirection aDirection, bool aWordMode = false, int aLineCount = 1) const;

	void MoveUp(int aAmount = 1, bool aSelect = false);
	void MoveDown(int aAmount = 1, bool aSelect = false);
	void MoveLeft(bool aSelect = false, bool aWordMode = false);
	void MoveRight(bool aSelect = false, bool aWordMode = false);
	void MoveTop(bool aSelect = false);
	void MoveBottom(bool aSelect = false);
	void MoveHome(bool aSelect = false);
	void MoveEnd(bool aSelect = false);

	void SetSelection(Coordinates aStart, Coordinates aEnd, int aCursor = -1);
	void SetSelection(int aStartLine, int aStartChar, int aEndLine, int aEndChar, int aCursor = -1);

	void SelectNextOccurrenceOf(const char* aText, int aTextSize, int aCursor = -1, bool aCaseSensitive = true);
	void AddCursorForNextOccurrence(bool aCaseSensitive = true);

	void MergeCursorsIfPossible();


	void Colorize(int aFromLine = 0, int aCount = -1);
	void ColorizeRange(int aFromLine = 0, int aToLine = 0);
	void ColorizeInternal();
	float TextDistanceToLineStart(const Coordinates& aFrom) const;
	void EnsureCursorVisible(int aCursor = -1);
	std::string GetText(const Coordinates& aStart, const Coordinates& aEnd) const;
	Coordinates GetActualCursorCoordinates(int aCursor = -1) const;
	Coordinates SanitizeCoordinates(const Coordinates& aValue) const;
	void Delete(bool aWordMode = false, const EditorState* aEditorState = nullptr);
	void DeleteRange(const Coordinates& aStart, const Coordinates& aEnd);
	int InsertTextAt(Coordinates& aWhere, const char* aValue);
	void AddUndo(UndoRecord& aValue);
	Coordinates ScreenPosToCoordinates(const ImVec2& aPosition, bool aInsertionMode = false, bool* isOverLineNumber = nullptr) const;
	Coordinates FindWordStart(const Coordinates& aFrom) const;
	Coordinates FindWordEnd(const Coordinates& aFrom) const;
	int GetCharacterIndexL(const Coordinates& aCoordinates) const;
	int GetCharacterIndexR(const Coordinates& aCoordinates) const;
	int GetCharacterColumn(int aLine, int aIndex) const;
	int GetLineMaxColumn(int aLine) const;

	Line& InsertLine(int aIndex);
	void RemoveLine(int aIndex, const std::unordered_set<int>* aHandledCursors = nullptr);
	void RemoveLines(int aStart, int aEnd);
	
	void RemoveCurrentLines();
	void OnLineChanged(bool aBeforeChange, int aLine, int aColumn, int aCharCount, bool aDeleted);
	void RemoveGlyphsFromLine(int aLine, int aStartChar, int aEndChar = -1);
	void AddGlyphsToLine(int aLine, int aTargetIndex, Line::iterator aSourceStart, Line::iterator aSourceEnd);
	void AddGlyphToLine(int aLine, int aTargetIndex, Glyph aGlyph);

	void ChangeCurrentLinesIndentation(bool aIncrease);
	void MoveUpCurrentLines();
	void MoveDownCurrentLines();
	void ToggleLineComment();

	void EnterCharacter(ImWchar aChar, bool aShift);
	void Backspace(bool aWordMode = false);
	void DeleteSelection(int aCursor = -1);
	std::string GetWordUnderCursor() const;
	std::string GetWordAt(const Coordinates& aCoords) const;
	ImU32 GetGlyphColor(const Glyph& aGlyph) const;

	void HandleKeyboardInputs(bool aParentIsFocused = false);
	void HandleMouseInputs();
	void Render(bool aParentIsFocused = false);

	bool FindNextOccurrence(const char* aText, int aTextSize, const Coordinates& aFrom, Coordinates& outStart, Coordinates& outEnd, bool aCaseSensitive = true);
	bool FindMatchingBracket(int aLine, int aCharIndex, Coordinates& out);

	std::vector<Line> mLines;
	EditorState mState;
	std::vector<UndoRecord> mUndoBuffer;
	int mUndoIndex = 0;

	int mTabSize = 4;
	float mLineSpacing = 1.0f;
	bool mOverwrite = false;
	bool mReadOnly = false;
	bool mAutoIndent = true;
	bool mShowWhitespaces = true;

	int mEnsureCursorVisible = -1;
	bool mScrollToTop = false;

	float mTextStart = 20.0f; // position (in pixels) where a code line starts relative to the left of the TextEditor.
	int mLeftMargin = 10;
	ImVec2 mCharAdvance;
	float mLongestLineLength = 20.0f;
	float mLastClick = -1.0f;

	int mColorRangeMin = 0;
	int mColorRangeMax = 0;
	bool mCheckComments = true;
	PaletteId mPaletteId;
	Palette mPalette;
	LanguageDefinitionId mLanguageDefinitionId;
	const LanguageDefinition* mLanguageDefinition = nullptr;
	RegexList mRegexList;
	std::string mLineBuffer;

	static const Palette& GetMarianaPalette();
	static const Palette& GetDarkPalette();
	static const Palette& GetLightPalette();
	static const Palette& GetRetroBluePalette();
	static const std::unordered_map<char, char> OPEN_TO_CLOSE_CHAR;
	static const std::unordered_map<char, char> CLOSE_TO_OPEN_CHAR;
	static bool IsGlyphWordChar(const Glyph& aGlyph);
	static PaletteId defaultPalette;
};
