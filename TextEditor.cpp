#define IMGUI_DEFINE_MATH_OPERATORS
#include "TextEditor.h"

#define IMGUI_SCROLLBAR_WIDTH 14.0f


struct TextEditor::RegexList {
    std::vector<std::pair<boost::regex, TextEditor::PaletteIndex>> mValue;
};

[[nodiscard]] constexpr auto matching_open_bracket(const char close_bracket) -> std::optional<char>
{
	switch (close_bracket)
	{
	case '}':
		return '{';
	case ')':
		return '(';
	case ']':
		return '[';
	default:
		return std::nullopt;
	}
}

[[nodiscard]] constexpr auto matching_close_bracket(const char open_bracket) -> std::optional<char>
{
	switch (open_bracket)
	{
	case '{':
		return '}';
	case '(':
		return ')';
	case '[':
		return ']';
	default:
		return std::nullopt;
	}
}


// --------------------------------------- //
// ------------- Exposed API ------------- //

TextEditor::TextEditor()
    : mRegexList(std::make_shared<RegexList>())
{
	SetPalette(defaultPalette);
	mLines.push_back(Line());
}

TextEditor::~TextEditor()
{
}

void TextEditor::SetPalette(PaletteId aValue)
{
	mPaletteId = aValue;
	const Palette* palletteBase = &(GetDarkPalette());
	switch (mPaletteId)
	{
	case PaletteId::Dark:
		palletteBase = &(GetDarkPalette());
		break;
	case PaletteId::Light:
		palletteBase = &(GetLightPalette());
		break;
	case PaletteId::Mariana:
		palletteBase = &(GetMarianaPalette());
		break;
	case PaletteId::RetroBlue:
		palletteBase = &(GetRetroBluePalette());
		break;
	default:
		palletteBase = &(GetDarkPalette());
		break;
	}
	/* Update palette with the current alpha from style */
	for (int i = 0; i < (int)PaletteIndex::Max; ++i)
	{
		ImVec4 color = U32ColorToVec4((*palletteBase)[i]);
		color.w *= ImGui::GetStyle().Alpha;
		mPalette[i] = ImGui::ColorConvertFloat4ToU32(color);
	}
}

void TextEditor::SetPalette(const Palette& aValue)
{
	for (int i = 0; i < (int)PaletteIndex::Max; ++i)
	{
		ImVec4 color = U32ColorToVec4(aValue[i]);
		color.w *= ImGui::GetStyle().Alpha;
		mPalette[i] = ImGui::ColorConvertFloat4ToU32(color);
	}
}

void TextEditor::SetLanguageDefinition(LanguageDefinitionId aValue)
{
	mLanguageDefinitionId = aValue;
	switch (mLanguageDefinitionId)
	{
	case LanguageDefinitionId::None:
		mLanguageDefinition = nullptr;
		return;
	case LanguageDefinitionId::Cpp:
		mLanguageDefinition = &(LanguageDefinition::Cpp());
		break;
	case LanguageDefinitionId::C:
		mLanguageDefinition = &(LanguageDefinition::C());
		break;
	case LanguageDefinitionId::Cs:
		mLanguageDefinition = &(LanguageDefinition::Cs());
		break;
	case LanguageDefinitionId::Python:
		mLanguageDefinition = &(LanguageDefinition::Python());
		break;
	case LanguageDefinitionId::Lua:
		mLanguageDefinition = &(LanguageDefinition::Lua());
		break;
	case LanguageDefinitionId::Json:
		mLanguageDefinition = &(LanguageDefinition::Json());
		break;
	case LanguageDefinitionId::Sql:
		mLanguageDefinition = &(LanguageDefinition::Sql());
		break;
	case LanguageDefinitionId::AngelScript:
		mLanguageDefinition = &(LanguageDefinition::AngelScript());
		break;
	case LanguageDefinitionId::Glsl:
		mLanguageDefinition = &(LanguageDefinition::Glsl());
		break;
	case LanguageDefinitionId::Hlsl:
		mLanguageDefinition = &(LanguageDefinition::Hlsl());
		break;
	}

    mRegexList->mValue.clear();
	for (const auto& r : mLanguageDefinition->mTokenRegexStrings)
        mRegexList->mValue.push_back(std::make_pair(boost::regex(r.first, boost::regex_constants::optimize), r.second));

	Colorize();
}

const char* TextEditor::GetLanguageDefinitionName() const
{
	return mLanguageDefinition != nullptr ? mLanguageDefinition->mName.c_str() : "None";
}

void TextEditor::SetTabSize(int aValue)
{
	mTabSize = Max(1, Min(8, aValue));
}

void TextEditor::SetLineSpacing(float aValue)
{
	mLineSpacing = Max(1.0f, Min(2.0f, aValue));
}

void TextEditor::SetZoomLevel(float aValue)
{
	mZoomLevel = std::clamp(aValue, 0.5f, 3.0f);
}

void TextEditor::SelectAll()
{
	ClearSelections();
	ClearExtraCursors();
	MoveTop();
	MoveBottom(true);
}

void TextEditor::SelectLine(int aLine)
{
	ClearSelections();
	ClearExtraCursors();
	SetSelection({ aLine, 0 }, { aLine, GetLineMaxColumn(aLine) });
}

void TextEditor::SelectRegion(int aStartLine, int aStartChar, int aEndLine, int aEndChar)
{
	ClearSelections();
	ClearExtraCursors();
	SetSelection(aStartLine, aStartChar, aEndLine, aEndChar);
}

void TextEditor::SelectNextOccurrenceOf(const char* aText, int aTextSize, bool aCaseSensitive)
{
	ClearSelections();
	ClearExtraCursors();
	SelectNextOccurrenceOf(aText, aTextSize, -1, aCaseSensitive);
}

void TextEditor::SelectAllOccurrencesOf(const char* aText, int aTextSize, bool aCaseSensitive)
{
	ClearSelections();
	ClearExtraCursors();
	SelectNextOccurrenceOf(aText, aTextSize, -1, aCaseSensitive);
	Coordinates startPos = mState.mCursors[mState.GetLastAddedCursorIndex()].mInteractiveEnd;
	while (true)
	{
		AddCursorForNextOccurrence(aCaseSensitive);
		Coordinates lastAddedPos = mState.mCursors[mState.GetLastAddedCursorIndex()].mInteractiveEnd;
		if (lastAddedPos == startPos)
			break;
	}
}

void TextEditor::AddCursorAbove()
{
	AddCursorsWithLineOffset(-1);
}

void TextEditor::AddCursorBelow()
{
	AddCursorsWithLineOffset(1);
}

bool TextEditor::AnyCursorHasSelection() const
{
	for (int c = 0; c <= mState.mCurrentCursor; c++)
		if (mState.mCursors[c].HasSelection())
			return true;
	return false;
}

bool TextEditor::AllCursorsHaveSelection() const
{
	for (int c = 0; c <= mState.mCurrentCursor; c++)
		if (!mState.mCursors[c].HasSelection())
			return false;
	return true;
}

void TextEditor::ClearExtraCursors()
{
	mState.mCurrentCursor = 0;
}

void TextEditor::ClearSelections()
{
	for (int c = mState.mCurrentCursor; c > -1; c--)
		mState.mCursors[c].mInteractiveEnd =
		mState.mCursors[c].mInteractiveStart =
		mState.mCursors[c].GetSelectionEnd();
}

void TextEditor::SetCursorPosition(int aLine, int aCharIndex)
{
	SetCursorPosition({ aLine, GetCharacterColumn(aLine, aCharIndex) }, -1, true);
}

int TextEditor::GetFirstVisibleLine() const
{
	return GetDocumentLineForVisualLine(mFirstVisibleLine);
}

int TextEditor::GetLastVisibleLine() const
{
	return GetDocumentLineForVisualLine(mLastVisibleLine);
}

void TextEditor::SetViewAtLine(int aLine, SetViewAtLineMode aMode)
{
	mSetViewAtLine = aLine;
	mSetViewAtLineMode = aMode;
}

void TextEditor::Copy()
{
	if (AnyCursorHasSelection())
	{
		std::string clipboardText = GetClipboardText();
		ImGui::SetClipboardText(clipboardText.c_str());
	}
	else
	{
		if (!mLines.empty())
		{
			std::string str;
			auto& line = mLines[GetSanitizedCursorCoordinates().mLine];
			for (auto& g : line)
				str.push_back(g.mChar);
			ImGui::SetClipboardText(str.c_str());
		}
	}
}

void TextEditor::Cut()
{
	if (mReadOnly)
	{
		Copy();
	}
	else
	{
		if (AnyCursorHasSelection())
		{
			UndoRecord u;
			u.mBefore = mState;

			Copy();
			for (int c = mState.mCurrentCursor; c > -1; c--)
			{
				u.mOperations.push_back({ GetSelectedText(c), mState.mCursors[c].GetSelectionStart(), mState.mCursors[c].GetSelectionEnd(), UndoOperationType::Delete });
				DeleteSelection(c);
			}

			u.mAfter = mState;
			AddUndo(u);
		}
	}
}

void TextEditor::Paste()
{
	if (mReadOnly)
		return;

	if (ImGui::GetClipboardText() == nullptr)
		return; // something other than text in the clipboard

	// check if we should do multicursor paste
	std::string clipText = ImGui::GetClipboardText();
	bool canPasteToMultipleCursors = false;
	std::vector<std::pair<int, int>> clipTextLines;
	if (mState.mCurrentCursor > 0)
	{
		clipTextLines.push_back({ 0,0 });
		for (size_t i = 0; i < clipText.length(); i++)
		{
			if (clipText[i] == '\n')
			{
				clipTextLines.back().second = static_cast<int>(i);
				clipTextLines.push_back({ static_cast<int>(i) + 1, 0 });
			}
		}
		clipTextLines.back().second = static_cast<int>(clipText.length());
		canPasteToMultipleCursors = static_cast<int>(clipTextLines.size()) == mState.mCurrentCursor + 1;
	}

	if (clipText.length() > 0)
	{
		UndoRecord u;
		u.mBefore = mState;

		if (AnyCursorHasSelection())
		{
			for (int c = mState.mCurrentCursor; c > -1; c--)
			{
				u.mOperations.push_back({ GetSelectedText(c), mState.mCursors[c].GetSelectionStart(), mState.mCursors[c].GetSelectionEnd(), UndoOperationType::Delete });
				DeleteSelection(c);
			}
		}

		for (int c = mState.mCurrentCursor; c > -1; c--)
		{
			Coordinates start = GetSanitizedCursorCoordinates(c);
			if (canPasteToMultipleCursors)
			{
				std::string clipSubText = clipText.substr(clipTextLines[c].first, clipTextLines[c].second - clipTextLines[c].first);
				InsertTextAtCursor(clipSubText.c_str(), c);
				u.mOperations.push_back({ clipSubText, start, GetSanitizedCursorCoordinates(c), UndoOperationType::Add });
			}
			else
			{
				InsertTextAtCursor(clipText.c_str(), c);
				u.mOperations.push_back({ clipText, start, GetSanitizedCursorCoordinates(c), UndoOperationType::Add });
			}
		}

		u.mAfter = mState;
		AddUndo(u);
	}
}

void TextEditor::Undo(int aSteps)
{
	while (CanUndo() && aSteps-- > 0)
		mUndoBuffer[--mUndoIndex].Undo(this);
}

void TextEditor::Redo(int aSteps)
{
	while (CanRedo() && aSteps-- > 0)
		mUndoBuffer[mUndoIndex++].Redo(this);
}

void TextEditor::SetText(const std::string& aText)
{
	mLines.clear();
	mLines.emplace_back(Line());
	for (auto chr : aText)
	{
		if (chr == '\r')
			continue;

		if (chr == '\n')
			mLines.emplace_back(Line());
		else
		{
			mLines.back().emplace_back(Glyph(chr, PaletteIndex::Default));
		}
	}

	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

std::string TextEditor::GetText() const
{
	auto lastLine = (int)mLines.size() - 1;
	auto lastLineLength = GetLineMaxColumn(lastLine);
	Coordinates startCoords = Coordinates();
	Coordinates endCoords = Coordinates(lastLine, lastLineLength);
	return startCoords < endCoords ? GetText(startCoords, endCoords) : "";
}

void TextEditor::SetTextLines(const std::vector<std::string>& aLines)
{
	mLines.clear();

	if (aLines.empty())
		mLines.emplace_back(Line());
	else
	{
		mLines.resize(aLines.size());

		for (size_t i = 0; i < aLines.size(); ++i)
		{
			const std::string& aLine = aLines[i];

			mLines[i].reserve(aLine.size());
			for (size_t j = 0; j < aLine.size(); ++j)
				mLines[i].emplace_back(Glyph(aLine[j], PaletteIndex::Default));
		}
	}

	mScrollToTop = true;

	mUndoBuffer.clear();
	mUndoIndex = 0;

	Colorize();
}

std::vector<std::string> TextEditor::GetTextLines() const
{
	std::vector<std::string> result;

	result.reserve(mLines.size());

	for (auto& line : mLines)
	{
		std::string text;

		text.resize(line.size());

		for (size_t i = 0; i < line.size(); ++i)
			text[i] = line[i].mChar;

		result.emplace_back(std::move(text));
	}

	return result;
}

void TextEditor::GetLineText(int aLine, std::string& outText) const
{
	outText.clear();
	if (aLine < 0 || aLine >= static_cast<int>(mLines.size()))
		return;

	const auto& line = mLines[static_cast<std::size_t>(aLine)];
	outText.reserve(line.size());
	for (const auto& glyph : line)
		outText.push_back(glyph.mChar);
}

auto TextEditor::GetLineText(int aLine) const -> std::string
{
	std::string line_text;
	GetLineText(aLine, line_text);
	return line_text;
}

auto TextEditor::GetLineLength(int aLine) const -> int
{
	if (aLine < 0 || aLine >= static_cast<int>(mLines.size()))
		return 0;
	return static_cast<int>(mLines[static_cast<std::size_t>(aLine)].size());
}

bool TextEditor::Render(const char* aTitle,
                        bool aParentIsFocused,
                        const ImVec2& aSize,
                        bool aBorder,
                        const render_callback& aCallback)
{
	if (mCursorPositionChanged)
		OnCursorPositionChanged();
	mCursorPositionChanged = false;

	imgui::scoped::StyleColor const child_bg(ImGuiCol_ChildBg, mPalette[(int)PaletteIndex::Background]);
	imgui::scoped::StyleVar const item_spacing(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGuiChildFlags const child_flags = aBorder ? ImGuiChildFlags_Borders : ImGuiChildFlags_None;
	const ImGuiWindowFlags child_window_flags =
		(mWordWrapEnabled ? ImGuiWindowFlags_None : ImGuiWindowFlags_HorizontalScrollbar) |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoNavInputs;
	imgui::scoped::Child const child(
		aTitle,
		aSize,
		child_flags,
		child_window_flags
	);

	bool isFocused = ImGui::IsWindowFocused();
	HandleKeyboardInputs(aParentIsFocused);
	HandleMouseInputs();
	ColorizeInternal();
	Render(aParentIsFocused);

	if (aCallback)
	{
		aCallback();
	}

	return isFocused;
}

// ------------------------------------ //
// ---------- Generic utils ----------- //

// https://en.wikipedia.org/wiki/UTF-8
// We assume that the char is a standalone character (<128) or a leading byte of an UTF-8 code sequence (non-10xxxxxx code)
static int UTF8CharLength(char c)
{
	if ((c & 0xFE) == 0xFC)
		return 6;
	if ((c & 0xFC) == 0xF8)
		return 5;
	if ((c & 0xF8) == 0xF0)
		return 4;
	else if ((c & 0xF0) == 0xE0)
		return 3;
	else if ((c & 0xE0) == 0xC0)
		return 2;
	return 1;
}

// "Borrowed" from ImGui source
static inline int ImTextCharToUtf8(char* buf, int buf_size, unsigned int c)
{
	if (c < 0x80)
	{
		buf[0] = (char)c;
		return 1;
	}
	if (c < 0x800)
	{
		if (buf_size < 2) return 0;
		buf[0] = (char)(0xc0 + (c >> 6));
		buf[1] = (char)(0x80 + (c & 0x3f));
		return 2;
	}
	if (c >= 0xdc00 && c < 0xe000)
	{
		return 0;
	}
	if (c >= 0xd800 && c < 0xdc00)
	{
		if (buf_size < 4) return 0;
		buf[0] = (char)(0xf0 + (c >> 18));
		buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
		buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[3] = (char)(0x80 + ((c) & 0x3f));
		return 4;
	}
	//else if (c < 0x10000)
	{
		if (buf_size < 3) return 0;
		buf[0] = (char)(0xe0 + (c >> 12));
		buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
		buf[2] = (char)(0x80 + ((c) & 0x3f));
		return 3;
	}
}

static inline bool CharIsWordChar(char ch)
{
	int sizeInBytes = UTF8CharLength(ch);
	return sizeInBytes > 1 ||
		(ch >= 'a' && ch <= 'z') ||
		(ch >= 'A' && ch <= 'Z') ||
		(ch >= '0' && ch <= '9') ||
		ch == '_';
}

// ------------------------------------ //
// ------------- Internal ------------- //


// ---------- Editor state functions --------- //

void TextEditor::EditorState::AddCursor()
{
	// vector is never resized to smaller size, mCurrentCursor points to last available cursor in vector
	mCurrentCursor++;
	mCursors.resize(mCurrentCursor + 1);
	mLastAddedCursor = mCurrentCursor;
}

int TextEditor::EditorState::GetLastAddedCursorIndex()
{
	return mLastAddedCursor > mCurrentCursor ? 0 : mLastAddedCursor;
}

void TextEditor::EditorState::SortCursorsFromTopToBottom()
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

// ---------- Undo record functions --------- //

TextEditor::UndoRecord::UndoRecord(const std::vector<UndoOperation>& aOperations,
	TextEditor::EditorState& aBefore, TextEditor::EditorState& aAfter)
{
	mOperations = aOperations;
	mBefore = aBefore;
	mAfter = aAfter;
#ifndef NDEBUG
	for (const UndoOperation& o : mOperations)
		assert(o.mStart <= o.mEnd);
#endif
}

void TextEditor::UndoRecord::Undo(TextEditor* aEditor)
{
	for (int i = static_cast<int>(mOperations.size()) - 1; i > -1; i--)
	{
		const UndoOperation& operation = mOperations[i];
		if (!operation.mText.empty())
		{
			switch (operation.mType)
			{
			case UndoOperationType::Delete:
			{
				auto start = operation.mStart;
				aEditor->InsertTextAt(start, operation.mText.c_str());
				aEditor->Colorize(operation.mStart.mLine - 1, operation.mEnd.mLine - operation.mStart.mLine + 2);
				break;
			}
			case UndoOperationType::Add:
			{
				aEditor->DeleteRange(operation.mStart, operation.mEnd);
				aEditor->Colorize(operation.mStart.mLine - 1, operation.mEnd.mLine - operation.mStart.mLine + 2);
				break;
			}
			}
		}
	}

	aEditor->mState = mBefore;
	aEditor->EnsureCursorVisible();
}

void TextEditor::UndoRecord::Redo(TextEditor* aEditor)
{
	for (size_t i = 0; i < mOperations.size(); i++)
	{
		const UndoOperation& operation = mOperations[i];
		if (!operation.mText.empty())
		{
			switch (operation.mType)
			{
			case UndoOperationType::Delete:
			{
				aEditor->DeleteRange(operation.mStart, operation.mEnd);
				aEditor->Colorize(operation.mStart.mLine - 1, operation.mEnd.mLine - operation.mStart.mLine + 1);
				break;
			}
			case UndoOperationType::Add:
			{
				auto start = operation.mStart;
				aEditor->InsertTextAt(start, operation.mText.c_str());
				aEditor->Colorize(operation.mStart.mLine - 1, operation.mEnd.mLine - operation.mStart.mLine + 1);
				break;
			}
			}
		}
	}

	aEditor->mState = mAfter;
	aEditor->EnsureCursorVisible();
}

// ---------- Text editor internal functions --------- //

std::string TextEditor::GetText(const Coordinates& aStart, const Coordinates& aEnd) const
{
	assert(aStart < aEnd);

	std::string result;
	auto lstart = aStart.mLine;
	auto lend = aEnd.mLine;
	auto istart = GetCharacterIndexR(aStart);
	auto iend = GetCharacterIndexR(aEnd);
	size_t s = 0;

	for (int i = lstart; i < lend; i++)
		s += mLines[i].size();

	result.reserve(s + s / 8);

	while (istart < iend || lstart < lend)
	{
		if (lstart >= (int)mLines.size())
			break;

		auto& line = mLines[lstart];
		if (istart < (int)line.size())
		{
			result += line[istart].mChar;
			istart++;
		}
		else
		{
			istart = 0;
			++lstart;
			result += '\n';
		}
	}

	return result;
}

std::string TextEditor::GetClipboardText() const
{
	std::string result;
	for (int c = 0; c <= mState.mCurrentCursor; c++)
	{
		if (mState.mCursors[c].GetSelectionStart() < mState.mCursors[c].GetSelectionEnd())
		{
			if (result.length() != 0)
				result += '\n';
			result += GetText(mState.mCursors[c].GetSelectionStart(), mState.mCursors[c].GetSelectionEnd());
		}
	}
	return result;
}

std::string TextEditor::GetSelectedText(int aCursor) const
{
	if (aCursor == -1)
		aCursor = mState.mCurrentCursor;

	return GetText(mState.mCursors[aCursor].GetSelectionStart(), mState.mCursors[aCursor].GetSelectionEnd());
}

void TextEditor::SetHighlights(const std::vector<Highlight>& aHighlights)
{
	mHighlights = aHighlights;
}

void TextEditor::ClearHighlights()
{
	mHighlights.clear();
}

void TextEditor::SetGhostLines(const std::vector<GhostLine>& aLines)
{
	mGhostLines = aLines;
	++mGhostLinesRevision;
}

void TextEditor::SetGhostLines(std::vector<GhostLine>&& aLines)
{
	mGhostLines = std::move(aLines);
	++mGhostLinesRevision;
}

void TextEditor::ClearGhostLines()
{
	if (!mGhostLines.empty())
	{
		mGhostLines.clear();
		++mGhostLinesRevision;
	}
}

void TextEditor::SetHiddenLineRanges(std::vector<LineRange> ranges)
{
	if (ranges.empty())
	{
		ClearHiddenLineRanges();
		return;
	}

	for (auto& range : ranges)
	{
		if (range.mStartLine > range.mEndLine)
			std::swap(range.mStartLine, range.mEndLine);
	}

	std::sort(ranges.begin(), ranges.end(), [](const LineRange& a, const LineRange& b) {
		return a.mStartLine < b.mStartLine;
	});

	std::vector<LineRange> merged;
	merged.reserve(ranges.size());
	for (const auto& range : ranges)
	{
		if (merged.empty())
		{
			merged.push_back(range);
			continue;
		}
		auto& last = merged.back();
		if (range.mStartLine <= last.mEndLine + 1)
		{
			last.mEndLine = std::max(last.mEndLine, range.mEndLine);
		}
		else
		{
			merged.push_back(range);
		}
	}

	mHiddenLineRanges = std::move(merged);
	++mHiddenRangesRevision;
}

void TextEditor::ClearHiddenLineRanges()
{
	if (!mHiddenLineRanges.empty())
	{
		mHiddenLineRanges.clear();
		++mHiddenRangesRevision;
	}
}

void TextEditor::EnsureVisualLines() const
{
	const int line_count = static_cast<int>(mLines.size());
	const int effective_wrap_column = mWordWrapEnabled ? Max(1, mWrapColumn) : 0;
	if (mCachedLineCount == line_count &&
	    mCachedGhostRevision == mGhostLinesRevision &&
	    mCachedHiddenRevision == mHiddenRangesRevision &&
	    mCachedLinesRevision == mLinesRevision &&
	    mCachedWordWrapEnabled == mWordWrapEnabled &&
	    mCachedWrapColumn == effective_wrap_column)
		return;

	mVisualLines.clear();
	mDocumentToVisual.clear();
	mDocumentToVisual.resize(static_cast<std::size_t>(std::max(0, line_count)), -1);

	std::vector<std::vector<int>> ghost_buckets;
	ghost_buckets.resize(static_cast<std::size_t>(std::max(0, line_count) + 1), {});

	for (std::size_t i = 0; i < mGhostLines.size(); ++i)
	{
		int anchor = mGhostLines[i].mAnchorLine;
		if (anchor < 0)
			anchor = 0;
		if (anchor > line_count)
			anchor = line_count;
		ghost_buckets[static_cast<std::size_t>(anchor)].push_back(static_cast<int>(i));
	}

	auto append_document_visual_line = [&](int doc_line, int start_column, int end_column, int& visual_index) {
		if (doc_line < 0 || doc_line >= line_count)
			return;
		if (doc_line >= 0 && doc_line < static_cast<int>(mDocumentToVisual.size()) &&
		    mDocumentToVisual[static_cast<std::size_t>(doc_line)] < 0)
		{
			mDocumentToVisual[static_cast<std::size_t>(doc_line)] = visual_index;
		}

		VisualLine entry{};
		entry.mDocumentLine = doc_line;
		entry.mWrapStartColumn = Max(0, start_column);
		entry.mWrapEndColumn = Max(entry.mWrapStartColumn, end_column);
		mVisualLines.push_back(entry);
		++visual_index;
	};

	auto append_wrapped_document_lines = [&](int doc_line, int& visual_index) {
		const int line_max_column = GetLineMaxColumn(doc_line);
		if (line_max_column <= effective_wrap_column || effective_wrap_column <= 0)
		{
			append_document_visual_line(doc_line, 0, line_max_column, visual_index);
			return;
		}

		const auto& line = mLines[static_cast<std::size_t>(doc_line)];
		const int line_size = static_cast<int>(line.size());
		int segment_start_index = 0;
		int segment_start_column = 0;
		int char_index = 0;
		int column = 0;
		int last_break_index = -1;
		int last_break_column = -1;

		while (char_index < line_size)
		{
			const int glyph_index = char_index;
			const int glyph_column_start = column;
			MoveCharIndexAndColumn(doc_line, char_index, column);

			const bool can_break_after =
				std::isspace(static_cast<unsigned char>(line[static_cast<std::size_t>(glyph_index)].mChar)) != 0;
			if (can_break_after)
			{
				last_break_index = char_index;
				last_break_column = column;
			}

			if (column - segment_start_column <= effective_wrap_column)
				continue;

			int break_index = char_index;
			int break_column = column;
			if (last_break_index > segment_start_index && last_break_column > segment_start_column)
			{
				break_index = last_break_index;
				break_column = last_break_column;
			}
			else if (glyph_column_start > segment_start_column)
			{
				break_index = glyph_index;
				break_column = glyph_column_start;
			}

			if (break_index <= segment_start_index || break_column <= segment_start_column)
			{
				break_index = char_index;
				break_column = column;
			}

			append_document_visual_line(doc_line, segment_start_column, break_column, visual_index);

			segment_start_index = break_index;
			segment_start_column = break_column;
			char_index = break_index;
			column = break_column;
			last_break_index = -1;
			last_break_column = -1;
		}

		if (line_max_column == 0 || segment_start_column < line_max_column)
		{
			append_document_visual_line(doc_line, segment_start_column, line_max_column, visual_index);
		}
	};

	int visual_index = 0;
	std::size_t hidden_index = 0;
	for (int doc_line = 0; doc_line < line_count; ++doc_line)
	{
		for (int ghost_index : ghost_buckets[static_cast<std::size_t>(doc_line)])
		{
			VisualLine entry{};
			entry.mIsGhost = true;
			entry.mGhostIndex = ghost_index;
			mVisualLines.push_back(entry);
			++visual_index;
		}

		while (hidden_index < mHiddenLineRanges.size() &&
		       mHiddenLineRanges[hidden_index].mEndLine < doc_line)
		{
			++hidden_index;
		}

		const bool is_hidden = hidden_index < mHiddenLineRanges.size() &&
			doc_line >= mHiddenLineRanges[hidden_index].mStartLine &&
			doc_line <= mHiddenLineRanges[hidden_index].mEndLine;

		if (is_hidden)
		{
			continue;
		}

		if (mWordWrapEnabled)
		{
			append_wrapped_document_lines(doc_line, visual_index);
		}
		else
		{
			append_document_visual_line(doc_line, 0, GetLineMaxColumn(doc_line), visual_index);
		}
	}

	for (int ghost_index : ghost_buckets[static_cast<std::size_t>(line_count)])
	{
		VisualLine entry{};
		entry.mIsGhost = true;
		entry.mGhostIndex = ghost_index;
		mVisualLines.push_back(entry);
		++visual_index;
	}

	mCachedLineCount = line_count;
	mCachedGhostRevision = mGhostLinesRevision;
	mCachedHiddenRevision = mHiddenRangesRevision;
	mCachedLinesRevision = mLinesRevision;
	mCachedWordWrapEnabled = mWordWrapEnabled;
	mCachedWrapColumn = effective_wrap_column;
}

int TextEditor::GetVisualLineCount() const
{
	EnsureVisualLines();
	return static_cast<int>(mVisualLines.size());
}

int TextEditor::GetVisualLineForDocumentLine(int aLine) const
{
	EnsureVisualLines();
	const int line_count = static_cast<int>(mLines.size());
	if (line_count <= 0)
		return 0;
	if (aLine < 0)
		return 0;
	if (aLine >= line_count)
		return static_cast<int>(std::max<std::size_t>(mVisualLines.size(), 1u)) - 1;
	if (aLine >= 0 && aLine < static_cast<int>(mDocumentToVisual.size()))
	{
		int mapped = mDocumentToVisual[static_cast<std::size_t>(aLine)];
		if (mapped >= 0)
			return mapped;

		for (int line = aLine + 1; line < line_count; ++line)
		{
			int next = mDocumentToVisual[static_cast<std::size_t>(line)];
			if (next >= 0)
				return next;
		}
		for (int line = aLine - 1; line >= 0; --line)
		{
			int prev = mDocumentToVisual[static_cast<std::size_t>(line)];
			if (prev >= 0)
				return prev;
		}
	}
	return 0;
}

int TextEditor::GetVisualLineForCoordinates(const Coordinates& aCoords) const
{
	EnsureVisualLines();
	const Coordinates coords = SanitizeCoordinates(aCoords);
	const int visual = GetVisualLineForDocumentLine(coords.mLine);
	if (!mWordWrapEnabled)
		return visual;
	if (visual < 0 || visual >= static_cast<int>(mVisualLines.size()))
		return visual;

	int best_visual = visual;
	for (int i = visual; i < static_cast<int>(mVisualLines.size()); ++i)
	{
		const auto& entry = mVisualLines[static_cast<std::size_t>(i)];
		if (entry.mIsGhost || entry.mDocumentLine != coords.mLine)
			break;
		best_visual = i;
		if (coords.mColumn < entry.mWrapEndColumn)
			return i;
	}
	return best_visual;
}

int TextEditor::GetDocumentLineForVisualLine(int aLine) const
{
	EnsureVisualLines();
	const int line_count = static_cast<int>(mLines.size());
	if (line_count <= 0)
		return 0;
	if (mVisualLines.empty())
		return std::clamp(aLine, 0, line_count - 1);

	int visual = aLine;
	if (visual < 0)
		visual = 0;
	if (visual >= static_cast<int>(mVisualLines.size()))
		visual = static_cast<int>(mVisualLines.size()) - 1;

	const auto& entry = mVisualLines[static_cast<std::size_t>(visual)];
	if (!entry.mIsGhost)
		return std::clamp(entry.mDocumentLine, 0, line_count - 1);

	if (entry.mGhostIndex < 0 || entry.mGhostIndex >= static_cast<int>(mGhostLines.size()))
		return 0;

	int anchor = mGhostLines[static_cast<std::size_t>(entry.mGhostIndex)].mAnchorLine;
	if (anchor < 0)
		anchor = 0;
	if (anchor >= line_count)
		return line_count - 1;
	return anchor;
}

int TextEditor::GetVisualLineStartColumn(int aLine) const
{
	EnsureVisualLines();
	if (aLine < 0 || aLine >= static_cast<int>(mVisualLines.size()))
		return 0;
	const auto& entry = mVisualLines[static_cast<std::size_t>(aLine)];
	if (entry.mIsGhost || entry.mDocumentLine < 0 || entry.mDocumentLine >= static_cast<int>(mLines.size()))
		return 0;
	return Max(0, entry.mWrapStartColumn);
}

int TextEditor::GetVisualLineEndColumn(int aLine) const
{
	EnsureVisualLines();
	if (aLine < 0 || aLine >= static_cast<int>(mVisualLines.size()))
		return 0;
	const auto& entry = mVisualLines[static_cast<std::size_t>(aLine)];
	if (entry.mIsGhost || entry.mDocumentLine < 0 || entry.mDocumentLine >= static_cast<int>(mLines.size()))
		return 0;
	const int line_max_column = GetLineMaxColumn(entry.mDocumentLine);
	const int end_column = Max(entry.mWrapStartColumn, entry.mWrapEndColumn);
	return Min(line_max_column, end_column);
}

const TextEditor::GhostLine* TextEditor::GetGhostLineForVisualLine(int aLine) const
{
	EnsureVisualLines();
	if (aLine < 0 || aLine >= static_cast<int>(mVisualLines.size()))
		return nullptr;
	const auto& entry = mVisualLines[static_cast<std::size_t>(aLine)];
	if (!entry.mIsGhost)
		return nullptr;
	if (entry.mGhostIndex < 0 || entry.mGhostIndex >= static_cast<int>(mGhostLines.size()))
		return nullptr;
	return &mGhostLines[static_cast<std::size_t>(entry.mGhostIndex)];
}

int TextEditor::GetMaxLineNumber() const
{
	int max_line = static_cast<int>(mLines.size());
	for (const auto& ghost : mGhostLines)
	{
		if (ghost.mLineNumber > max_line)
			max_line = ghost.mLineNumber;
	}
	return max_line;
}

void TextEditor::SetLinkHighlight(const std::optional<LinkHighlight>& aLink)
{
	mLinkHighlight = aLink;
}

void TextEditor::ClearLinkHighlight()
{
	mLinkHighlight.reset();
}

std::pair<int, int> TextEditor::GetWordBoundaries(int aLine, int aCharIndex) const
{
	if (aLine < 0 || aLine >= static_cast<int>(mLines.size()) || aCharIndex < 0)
		return {0, 0};

	const auto& line = mLines[static_cast<std::size_t>(aLine)];
	if (line.empty() || aCharIndex >= static_cast<int>(line.size()))
		return {0, 0};

	// Check if we're on a word character
	auto isWordChar = [](char c) -> bool {
		return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
	};

	// Get the character at the position - need to handle UTF-8 properly
	// For now, treat each byte as a character (this works for ASCII identifiers)
	char c = line[static_cast<std::size_t>(aCharIndex)].mChar;
	if (!isWordChar(c))
		return {aCharIndex, aCharIndex}; // Not on a word

	// Find start of word
	int start = aCharIndex;
	while (start > 0) {
		char prev = line[static_cast<std::size_t>(start - 1)].mChar;
		if (!isWordChar(prev))
			break;
		--start;
	}

	// Find end of word
	int end = aCharIndex;
	while (end < static_cast<int>(line.size())) {
		char next = line[static_cast<std::size_t>(end)].mChar;
		if (!isWordChar(next))
			break;
		++end;
	}

	return {start, end};
}

bool TextEditor::ReplaceRange(int aStartLine, int aStartChar, int aEndLine, int aEndChar, const char* aText, int aCursor)
{
	if (mReadOnly || mLines.empty())
		return false;

	if (aCursor == -1)
		aCursor = mState.mCurrentCursor;

	if (aText == nullptr)
		aText = "";

	UndoRecord u;
	u.mBefore = mState;

	SetSelection(aStartLine, aStartChar, aEndLine, aEndChar, aCursor);
	Coordinates selectionStart = mState.mCursors[aCursor].GetSelectionStart();
	Coordinates selectionEnd = mState.mCursors[aCursor].GetSelectionEnd();

	if (selectionEnd > selectionStart)
	{
		u.mOperations.push_back({ GetText(selectionStart, selectionEnd), selectionStart, selectionEnd, UndoOperationType::Delete });
		DeleteSelection(aCursor);
	}

	Coordinates insertStart = GetSanitizedCursorCoordinates(aCursor);
	if (aText[0] != '\0')
	{
		InsertTextAtCursor(aText, aCursor);
		Coordinates insertEnd = GetSanitizedCursorCoordinates(aCursor);
		u.mOperations.push_back({ aText, insertStart, insertEnd, UndoOperationType::Add });
	}

	u.mAfter = mState;
	AddUndo(u);
	return true;
}

void TextEditor::SetUnderlines(const std::vector<Underline>& aUnderlines)
{
	mUnderlines = aUnderlines;
}

void TextEditor::ClearUnderlines()
{
	mUnderlines.clear();
}

void TextEditor::SetCursorPosition(const Coordinates& aPosition, int aCursor, bool aClearSelection)
{
	if (aCursor == -1)
		aCursor = mState.mCurrentCursor;

	mCursorPositionChanged = true;
	if (aClearSelection)
		mState.mCursors[aCursor].mInteractiveStart = aPosition;
	if (mState.mCursors[aCursor].mInteractiveEnd != aPosition)
	{
		mState.mCursors[aCursor].mInteractiveEnd = aPosition;
		EnsureCursorVisible();
	}
}

int TextEditor::InsertTextAt(Coordinates& /* inout */ aWhere, const char* aValue)
{
	assert(!mReadOnly);

	int cindex = GetCharacterIndexR(aWhere);
	int totalLines = 0;
	while (*aValue != '\0')
	{
		assert(!mLines.empty());

		if (*aValue == '\r')
		{
			// skip
			++aValue;
		}
		else if (*aValue == '\n')
		{
			if (cindex < (int)mLines[aWhere.mLine].size())
			{
				InsertLine(aWhere.mLine + 1);
				AddGlyphsToLine(aWhere.mLine + 1, 0, mLines[aWhere.mLine].begin() + cindex, mLines[aWhere.mLine].end());
				RemoveGlyphsFromLine(aWhere.mLine, cindex);
			}
			else
			{
				InsertLine(aWhere.mLine + 1);
			}
			++aWhere.mLine;
			aWhere.mColumn = 0;
			cindex = 0;
			++totalLines;
			++aValue;
		}
		else
		{
			auto d = UTF8CharLength(*aValue);
			while (d-- > 0 && *aValue != '\0')
				AddGlyphToLine(aWhere.mLine, cindex++, Glyph(*aValue++, PaletteIndex::Default));
			aWhere.mColumn = GetCharacterColumn(aWhere.mLine, cindex);
		}
	}

	return totalLines;
}

void TextEditor::InsertTextAtCursor(const char* aValue, int aCursor)
{
	if (aValue == nullptr)
		return;
	if (aCursor == -1)
		aCursor = mState.mCurrentCursor;

	auto pos = GetSanitizedCursorCoordinates(aCursor);
	auto start = std::min(pos, mState.mCursors[aCursor].GetSelectionStart());
	int totalLines = pos.mLine - start.mLine;

	totalLines += InsertTextAt(pos, aValue);

	SetCursorPosition(pos, aCursor);
	Colorize(start.mLine - 1, totalLines + 2);
}

bool TextEditor::Move(int& aLine, int& aCharIndex, bool aLeft, bool aLockLine) const
{
	// assumes given char index is not in the middle of utf8 sequence
	// char index can be line.length()

	// invalid line
	if (aLine >= static_cast<int>(mLines.size()))
		return false;

	if (aLeft)
	{
		if (aCharIndex == 0)
		{
			if (aLockLine || aLine == 0)
				return false;
			aLine--;
			aCharIndex = static_cast<int>(mLines[aLine].size());
		}
		else
		{
			aCharIndex--;
			while (aCharIndex > 0 && IsUTFSequence(mLines[aLine][aCharIndex].mChar))
				aCharIndex--;
		}
	}
	else // right
	{
		if (aCharIndex == static_cast<int>(mLines[aLine].size()))
		{
			if (aLockLine || aLine == static_cast<int>(mLines.size()) - 1)
				return false;
			aLine++;
			aCharIndex = 0;
		}
		else
		{
			int seqLength = UTF8CharLength(mLines[aLine][aCharIndex].mChar);
			aCharIndex = std::min(aCharIndex + seqLength, (int)mLines[aLine].size());
		}
	}
	return true;
}

void TextEditor::MoveCharIndexAndColumn(int aLine, int& aCharIndex, int& aColumn) const
{
	assert(aLine < static_cast<int>(mLines.size()));
	assert(aCharIndex < static_cast<int>(mLines[aLine].size()));
	char c = mLines[aLine][aCharIndex].mChar;
	aCharIndex += UTF8CharLength(c);
	if (c == '\t')
		aColumn = (aColumn / mTabSize) * mTabSize + mTabSize;
	else
		aColumn++;
}

void TextEditor::MoveCoords(Coordinates& aCoords, MoveDirection aDirection, bool aWordMode, int aLineCount) const
{
	int charIndex = GetCharacterIndexR(aCoords);
	int lineIndex = aCoords.mLine;
	switch (aDirection)
	{
	case MoveDirection::Right:
		if (charIndex >= static_cast<int>(mLines[lineIndex].size()))
		{
			if (lineIndex < static_cast<int>(mLines.size()) - 1)
			{
				aCoords.mLine = std::max(0, std::min((int)mLines.size() - 1, lineIndex + 1));
				aCoords.mColumn = 0;
			}
		}
		else
		{
			Move(lineIndex, charIndex);
			int oneStepRightColumn = GetCharacterColumn(lineIndex, charIndex);
			if (aWordMode)
			{
				aCoords = FindWordEnd(aCoords);
				aCoords.mColumn = std::max(aCoords.mColumn, oneStepRightColumn);
			}
			else
				aCoords.mColumn = oneStepRightColumn;
		}
		break;
	case MoveDirection::Left:
		if (charIndex == 0)
		{
			if (lineIndex > 0)
			{
				aCoords.mLine = lineIndex - 1;
				aCoords.mColumn = GetLineMaxColumn(aCoords.mLine);
			}
		}
		else
		{
			Move(lineIndex, charIndex, true);
			aCoords.mColumn = GetCharacterColumn(lineIndex, charIndex);
			if (aWordMode)
				aCoords = FindWordStart(aCoords);
		}
		break;
	case MoveDirection::Up:
		aCoords.mLine = std::max(0, lineIndex - aLineCount);
		break;
	case MoveDirection::Down:
		aCoords.mLine = std::max(0, std::min((int)mLines.size() - 1, lineIndex + aLineCount));
		break;
	}
}

void TextEditor::MoveUp(int aAmount, bool aSelect)
{
	for (int c = 0; c <= mState.mCurrentCursor; c++)
	{
		Coordinates newCoords = mState.mCursors[c].mInteractiveEnd;
		MoveCoords(newCoords, MoveDirection::Up, false, aAmount);
		SetCursorPosition(newCoords, c, !aSelect);
	}
	EnsureCursorVisible();
}

void TextEditor::MoveDown(int aAmount, bool aSelect)
{
	for (int c = 0; c <= mState.mCurrentCursor; c++)
	{
		assert(mState.mCursors[c].mInteractiveEnd.mColumn >= 0);
		Coordinates newCoords = mState.mCursors[c].mInteractiveEnd;
		MoveCoords(newCoords, MoveDirection::Down, false, aAmount);
		SetCursorPosition(newCoords, c, !aSelect);
	}
	EnsureCursorVisible();
}

void TextEditor::MoveLeft(bool aSelect, bool aWordMode)
{
	if (mLines.empty())
		return;

	if (AnyCursorHasSelection() && !aSelect && !aWordMode)
	{
		for (int c = 0; c <= mState.mCurrentCursor; c++)
			SetCursorPosition(mState.mCursors[c].GetSelectionStart(), c);
	}
	else
	{
		for (int c = 0; c <= mState.mCurrentCursor; c++)
		{
			Coordinates newCoords = mState.mCursors[c].mInteractiveEnd;
			MoveCoords(newCoords, MoveDirection::Left, aWordMode);
			SetCursorPosition(newCoords, c, !aSelect);
		}
	}
	EnsureCursorVisible();
}

void TextEditor::MoveRight(bool aSelect, bool aWordMode)
{
	if (mLines.empty())
		return;

	if (AnyCursorHasSelection() && !aSelect && !aWordMode)
	{
		for (int c = 0; c <= mState.mCurrentCursor; c++)
			SetCursorPosition(mState.mCursors[c].GetSelectionEnd(), c);
	}
	else
	{
		for (int c = 0; c <= mState.mCurrentCursor; c++)
		{
			Coordinates newCoords = mState.mCursors[c].mInteractiveEnd;
			MoveCoords(newCoords, MoveDirection::Right, aWordMode);
			SetCursorPosition(newCoords, c, !aSelect);
		}
	}
	EnsureCursorVisible();
}

void TextEditor::MoveTop(bool aSelect)
{
	SetCursorPosition(Coordinates(0, 0), mState.mCurrentCursor, !aSelect);
}

void TextEditor::TextEditor::MoveBottom(bool aSelect)
{
	int maxLine = (int)mLines.size() - 1;
	Coordinates newPos = Coordinates(maxLine, GetLineMaxColumn(maxLine));
	SetCursorPosition(newPos, mState.mCurrentCursor, !aSelect);
}

void TextEditor::MoveHome(bool aSelect)
{
	for (int c = 0; c <= mState.mCurrentCursor; c++)
		SetCursorPosition(Coordinates(mState.mCursors[c].mInteractiveEnd.mLine, 0), c, !aSelect);
}

void TextEditor::MoveEnd(bool aSelect)
{
	for (int c = 0; c <= mState.mCurrentCursor; c++)
	{
		int lindex = mState.mCursors[c].mInteractiveEnd.mLine;
		SetCursorPosition(Coordinates(lindex, GetLineMaxColumn(lindex)), c, !aSelect);
	}
}

void TextEditor::EnterCharacter(ImWchar aChar, bool aShift)
{
	assert(!mReadOnly);

	bool hasSelection = AnyCursorHasSelection();
	bool anyCursorHasMultilineSelection = false;
	for (int c = mState.mCurrentCursor; c > -1; c--)
		if (mState.mCursors[c].GetSelectionStart().mLine != mState.mCursors[c].GetSelectionEnd().mLine)
		{
			anyCursorHasMultilineSelection = true;
			break;
		}
	bool isIndentOperation = hasSelection && anyCursorHasMultilineSelection && aChar == '\t';
	if (isIndentOperation)
	{
		ChangeCurrentLinesIndentation(!aShift);
		return;
	}

	UndoRecord u;
	u.mBefore = mState;

	if (hasSelection)
	{
		for (int c = mState.mCurrentCursor; c > -1; c--)
		{
			u.mOperations.push_back({ GetSelectedText(c), mState.mCursors[c].GetSelectionStart(), mState.mCursors[c].GetSelectionEnd(), UndoOperationType::Delete });
			DeleteSelection(c);
		}
	}

	std::vector<Coordinates> coords;
	for (int c = mState.mCurrentCursor; c > -1; c--) // order important here for typing \n in the same line at the same time
	{
		auto coord = GetSanitizedCursorCoordinates(c);
		coords.push_back(coord);
		UndoOperation added;
		added.mType = UndoOperationType::Add;
		added.mStart = coord;

		assert(!mLines.empty());

		if (aChar == '\n')
		{
			InsertLine(coord.mLine + 1);
			auto& line = mLines[coord.mLine];
			auto& newLine = mLines[coord.mLine + 1];

			added.mText = "";
			added.mText += (char)aChar;
			if (mAutoIndent)
				for (size_t i = 0; i < line.size() && isascii(line[i].mChar) && isblank(line[i].mChar); ++i)
				{
					newLine.push_back(line[i]);
					added.mText += line[i].mChar;
				}

			const size_t whitespaceSize = newLine.size();
			auto cindex = GetCharacterIndexR(coord);
			AddGlyphsToLine(coord.mLine + 1, static_cast<int>(newLine.size()), line.begin() + cindex, line.end());
			RemoveGlyphsFromLine(coord.mLine, cindex);
			SetCursorPosition(Coordinates(coord.mLine + 1, GetCharacterColumn(coord.mLine + 1, (int)whitespaceSize)), c);
		}
		else
		{
			char buf[7];
			int e = ImTextCharToUtf8(buf, 7, aChar);
			if (e > 0)
			{
				buf[e] = '\0';
				auto cindex = GetCharacterIndexR(coord);

				for (auto p = buf; *p != '\0'; p++, ++cindex)
					AddGlyphToLine(coord.mLine, cindex, Glyph(*p, PaletteIndex::Default));
				added.mText = buf;

				SetCursorPosition(Coordinates(coord.mLine, GetCharacterColumn(coord.mLine, cindex)), c);
			}
			else
				continue;
		}

		added.mEnd = GetSanitizedCursorCoordinates(c);
		u.mOperations.push_back(added);
	}

	u.mAfter = mState;
	AddUndo(u);

	for (const auto& coord : coords)
		Colorize(coord.mLine - 1, 3);
	EnsureCursorVisible();
}

void TextEditor::Backspace(bool aWordMode)
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;

	if (AnyCursorHasSelection())
		Delete(aWordMode);
	else
	{
		EditorState stateBeforeDeleting = mState;
		MoveLeft(true, aWordMode);
		if (!AllCursorsHaveSelection()) // can't do backspace if any cursor at {0,0}
		{
			if (AnyCursorHasSelection())
				MoveRight();
			return;
		}

		OnCursorPositionChanged(); // might combine cursors
		Delete(aWordMode, &stateBeforeDeleting);
	}
}

void TextEditor::Delete(bool aWordMode, const EditorState* aEditorState)
{
	assert(!mReadOnly);

	if (mLines.empty())
		return;

	if (AnyCursorHasSelection())
	{
		UndoRecord u;
		u.mBefore = aEditorState == nullptr ? mState : *aEditorState;
		for (int c = mState.mCurrentCursor; c > -1; c--)
		{
			if (!mState.mCursors[c].HasSelection())
				continue;
			u.mOperations.push_back({ GetSelectedText(c), mState.mCursors[c].GetSelectionStart(), mState.mCursors[c].GetSelectionEnd(), UndoOperationType::Delete });
			DeleteSelection(c);
		}
		u.mAfter = mState;
		AddUndo(u);
	}
	else
	{
		EditorState stateBeforeDeleting = mState;
		MoveRight(true, aWordMode);
		if (!AllCursorsHaveSelection()) // can't do delete if any cursor at end of last line
		{
			if (AnyCursorHasSelection())
				MoveLeft();
			return;
		}

		OnCursorPositionChanged(); // might combine cursors
		Delete(aWordMode, &stateBeforeDeleting);
	}
}

void TextEditor::SetSelection(Coordinates aStart, Coordinates aEnd, int aCursor)
{
	if (aCursor == -1)
		aCursor = mState.mCurrentCursor;

	Coordinates minCoords = Coordinates(0, 0);
	int maxLine = (int)mLines.size() - 1;
	Coordinates maxCoords = Coordinates(maxLine, GetLineMaxColumn(maxLine));
	if (aStart < minCoords)
		aStart = minCoords;
	else if (aStart > maxCoords)
		aStart = maxCoords;
	if (aEnd < minCoords)
		aEnd = minCoords;
	else if (aEnd > maxCoords)
		aEnd = maxCoords;

	mState.mCursors[aCursor].mInteractiveStart = aStart;
	SetCursorPosition(aEnd, aCursor, false);
}

void TextEditor::SetSelection(int aStartLine, int aStartChar, int aEndLine, int aEndChar, int aCursor)
{
	Coordinates startCoords = { aStartLine, GetCharacterColumn(aStartLine, aStartChar) };
	Coordinates endCoords = { aEndLine, GetCharacterColumn(aEndLine, aEndChar) };
	SetSelection(startCoords, endCoords, aCursor);
}

void TextEditor::SelectNextOccurrenceOf(const char* aText, int aTextSize, int aCursor, bool aCaseSensitive)
{
	if (aCursor == -1)
		aCursor = mState.mCurrentCursor;
	Coordinates nextStart, nextEnd;
	FindNextOccurrence(aText, aTextSize, mState.mCursors[aCursor].mInteractiveEnd, nextStart, nextEnd, aCaseSensitive);
	SetSelection(nextStart, nextEnd, aCursor);
	EnsureCursorVisible(aCursor, true);
}

void TextEditor::AddCursorForNextOccurrence(bool aCaseSensitive)
{
	const Cursor& currentCursor = mState.mCursors[mState.GetLastAddedCursorIndex()];
	if (currentCursor.GetSelectionStart() == currentCursor.GetSelectionEnd())
		return;

	std::string selectionText = GetText(currentCursor.GetSelectionStart(), currentCursor.GetSelectionEnd());
	Coordinates nextStart, nextEnd;
	if (!FindNextOccurrence(selectionText.c_str(), static_cast<int>(selectionText.size()), currentCursor.GetSelectionEnd(), nextStart, nextEnd, aCaseSensitive))
		return;

	mState.AddCursor();
	SetSelection(nextStart, nextEnd, mState.mCurrentCursor);
	mState.SortCursorsFromTopToBottom();
	MergeCursorsIfPossible();
	EnsureCursorVisible(-1, true);
}

void TextEditor::AddCursorsWithLineOffset(int aLineOffset)
{
	if (mLines.empty() || aLineOffset == 0)
		return;

	std::vector<std::pair<Coordinates, Coordinates>> newSelections;
	newSelections.reserve(static_cast<std::size_t>(mState.mCurrentCursor + 1));

	for (int c = 0; c <= mState.mCurrentCursor; ++c)
	{
		const Cursor& cursor = mState.mCursors[c];
		const Coordinates anchor = SanitizeCoordinates(cursor.mInteractiveEnd);
		const int targetLine = anchor.mLine + aLineOffset;
		if (targetLine < 0 || targetLine >= GetLineCount())
			continue;

		const int maxColumn = GetLineMaxColumn(targetLine);
		const auto clampColumn = [maxColumn](int column) -> int {
			return Max(0, Min(column, maxColumn));
		};

		if (cursor.HasSelection() && cursor.mInteractiveStart.mLine == cursor.mInteractiveEnd.mLine)
		{
			const int startColumn = clampColumn(cursor.mInteractiveStart.mColumn);
			const int endColumn = clampColumn(cursor.mInteractiveEnd.mColumn);
			newSelections.emplace_back(Coordinates{ targetLine, startColumn }, Coordinates{ targetLine, endColumn });
		}
		else
		{
			const int targetColumn = clampColumn(anchor.mColumn);
			Coordinates target{ targetLine, targetColumn };
			newSelections.emplace_back(target, target);
		}
	}

	if (newSelections.empty())
		return;

	for (const auto& selection : newSelections)
	{
		mState.AddCursor();
		if (selection.first == selection.second)
			SetCursorPosition(selection.first, mState.mCurrentCursor, true);
		else
			SetSelection(selection.first, selection.second, mState.mCurrentCursor);
	}

	mState.SortCursorsFromTopToBottom();
	MergeCursorsIfPossible();
	EnsureCursorVisible(-1, true);
}

bool TextEditor::FindNextOccurrence(const char* aText, int aTextSize, const Coordinates& aFrom, Coordinates& outStart, Coordinates& outEnd, bool aCaseSensitive)
{
	assert(aTextSize > 0);
	int fline, ifline;
	int findex, ifindex;

	ifline = fline = aFrom.mLine;
	ifindex = findex = GetCharacterIndexR(aFrom);

	while (true)
	{
		bool matches;
		{ // match function
			int lineOffset = 0;
			int currentCharIndex = findex;
			int i = 0;
			for (; i < aTextSize; i++)
			{
				if (currentCharIndex == static_cast<int>(mLines[fline + lineOffset].size()))
				{
					if (aText[i] == '\n' && fline + lineOffset + 1 < static_cast<int>(mLines.size()))
					{
						currentCharIndex = 0;
						lineOffset++;
					}
					else
						break;
				}
				else
				{
					char toCompareA = mLines[fline + lineOffset][currentCharIndex].mChar;
					char toCompareB = aText[i];
					toCompareA = (!aCaseSensitive && toCompareA >= 'A' && toCompareA <= 'Z') ? toCompareA - 'A' + 'a' : toCompareA;
					toCompareB = (!aCaseSensitive && toCompareB >= 'A' && toCompareB <= 'Z') ? toCompareB - 'A' + 'a' : toCompareB;
					if (toCompareA != toCompareB)
						break;
					else
						currentCharIndex++;
				}
			}
			matches = i == aTextSize;
			if (matches)
			{
				outStart = { fline, GetCharacterColumn(fline, findex) };
				outEnd = { fline + lineOffset, GetCharacterColumn(fline + lineOffset, currentCharIndex) };
				return true;
			}
		}

		// move forward
		if (findex == static_cast<int>(mLines[fline].size())) // need to consider line breaks
		{
			if (fline == static_cast<int>(mLines.size()) - 1)
			{
				fline = 0;
				findex = 0;
			}
			else
			{
				fline++;
				findex = 0;
			}
		}
		else
			findex++;

		// detect complete scan
		if (findex == ifindex && fline == ifline)
			return false;
	}

	return false;
}

bool TextEditor::FindMatchingBracket(int aLine, int aCharIndex, Coordinates& out)
{
	if (aLine > static_cast<int>(mLines.size()) - 1)
		return false;
	int maxCharIndex = static_cast<int>(mLines[aLine].size()) - 1;
	if (aCharIndex > maxCharIndex)
		return false;

	int currentLine = aLine;
	int currentCharIndex = aCharIndex;
	int counter = 1;
	const char anchor_char = mLines[aLine][aCharIndex].mChar;
	if (const auto open_char = matching_open_bracket(anchor_char); open_char.has_value())
	{
		const char closeChar = anchor_char;
		const char openChar = *open_char;
		while (Move(currentLine, currentCharIndex, true))
		{
			if (currentCharIndex < static_cast<int>(mLines[currentLine].size()))
			{
				char currentChar = mLines[currentLine][currentCharIndex].mChar;
				if (currentChar == openChar)
				{
					counter--;
					if (counter == 0)
					{
						out = { currentLine, GetCharacterColumn(currentLine, currentCharIndex) };
						return true;
					}
				}
				else if (currentChar == closeChar)
					counter++;
			}
		}
	}
	else if (const auto close_char = matching_close_bracket(anchor_char); close_char.has_value())
	{
		const char openChar = anchor_char;
		const char closeChar = *close_char;
		while (Move(currentLine, currentCharIndex))
		{
			if (currentCharIndex < static_cast<int>(mLines[currentLine].size()))
			{
				char currentChar = mLines[currentLine][currentCharIndex].mChar;
				if (currentChar == closeChar)
				{
					counter--;
					if (counter == 0)
					{
						out = { currentLine, GetCharacterColumn(currentLine, currentCharIndex) };
						return true;
					}
				}
				else if (currentChar == openChar)
					counter++;
			}
		}
	}
	return false;
}

void TextEditor::ChangeCurrentLinesIndentation(bool aIncrease)
{
	assert(!mReadOnly);

	UndoRecord u;
	u.mBefore = mState;

	for (int c = mState.mCurrentCursor; c > -1; c--)
	{
		for (int currentLine = mState.mCursors[c].GetSelectionEnd().mLine; currentLine >= mState.mCursors[c].GetSelectionStart().mLine; currentLine--)
		{
			if (Coordinates{ currentLine, 0 } == mState.mCursors[c].GetSelectionEnd() && mState.mCursors[c].GetSelectionEnd() != mState.mCursors[c].GetSelectionStart()) // when selection ends at line start
				continue;

			if (aIncrease)
			{
				if (mLines[currentLine].size() > 0)
				{
					Coordinates lineStart = { currentLine, 0 };
					Coordinates insertionEnd = lineStart;
					InsertTextAt(insertionEnd, "\t"); // sets insertion end
					u.mOperations.push_back({ "\t", lineStart, insertionEnd, UndoOperationType::Add });
					Colorize(lineStart.mLine, 1);
				}
			}
			else
			{
				Coordinates start = { currentLine, 0 };
				Coordinates end = { currentLine, mTabSize };
				int charIndex = GetCharacterIndexL(end) - 1;
				while (charIndex > -1 && (mLines[currentLine][charIndex].mChar == ' ' || mLines[currentLine][charIndex].mChar == '\t')) charIndex--;
				bool onlySpaceCharactersFound = charIndex == -1;
				if (onlySpaceCharactersFound)
				{
					u.mOperations.push_back({ GetText(start, end), start, end, UndoOperationType::Delete });
					DeleteRange(start, end);
					Colorize(currentLine, 1);
				}
			}
		}
	}

	if (u.mOperations.size() > 0)
		AddUndo(u);
}

void TextEditor::MoveUpCurrentLines()
{
	assert(!mReadOnly);

	UndoRecord u;
	u.mBefore = mState;

	std::set<int> affectedLines;
	int minLine = -1;
	int maxLine = -1;
	for (int c = mState.mCurrentCursor; c > -1; c--) // cursors are expected to be sorted from top to bottom
	{
		for (int currentLine = mState.mCursors[c].GetSelectionEnd().mLine; currentLine >= mState.mCursors[c].GetSelectionStart().mLine; currentLine--)
		{
			if (Coordinates{ currentLine, 0 } == mState.mCursors[c].GetSelectionEnd() && mState.mCursors[c].GetSelectionEnd() != mState.mCursors[c].GetSelectionStart()) // when selection ends at line start
				continue;
			affectedLines.insert(currentLine);
			minLine = minLine == -1 ? currentLine : (currentLine < minLine ? currentLine : minLine);
			maxLine = maxLine == -1 ? currentLine : (currentLine > maxLine ? currentLine : maxLine);
		}
	}
	if (minLine == 0) // can't move up anymore
		return;

	Coordinates start = { minLine - 1, 0 };
	Coordinates end = { maxLine, GetLineMaxColumn(maxLine) };
	u.mOperations.push_back({ GetText(start, end), start, end, UndoOperationType::Delete });

	for (int line : affectedLines) // lines should be sorted here
		std::swap(mLines[line - 1], mLines[line]);
	for (int c = mState.mCurrentCursor; c > -1; c--)
	{
		mState.mCursors[c].mInteractiveStart.mLine -= 1;
		mState.mCursors[c].mInteractiveEnd.mLine -= 1;
		// no need to set mCursorPositionChanged as cursors will remain sorted
	}

	end = { maxLine, GetLineMaxColumn(maxLine) }; // this line is swapped with line above, need to find new max column
	u.mOperations.push_back({ GetText(start, end), start, end, UndoOperationType::Add });
	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::MoveDownCurrentLines()
{
	assert(!mReadOnly);

	UndoRecord u;
	u.mBefore = mState;

	std::set<int> affectedLines;
	int minLine = -1;
	int maxLine = -1;
	for (int c = 0; c <= mState.mCurrentCursor; c++) // cursors are expected to be sorted from top to bottom
	{
		for (int currentLine = mState.mCursors[c].GetSelectionEnd().mLine; currentLine >= mState.mCursors[c].GetSelectionStart().mLine; currentLine--)
		{
			if (Coordinates{ currentLine, 0 } == mState.mCursors[c].GetSelectionEnd() && mState.mCursors[c].GetSelectionEnd() != mState.mCursors[c].GetSelectionStart()) // when selection ends at line start
				continue;
			affectedLines.insert(currentLine);
			minLine = minLine == -1 ? currentLine : (currentLine < minLine ? currentLine : minLine);
			maxLine = maxLine == -1 ? currentLine : (currentLine > maxLine ? currentLine : maxLine);
			}
	}
	const bool has_trailing_empty_line = !mLines.empty() && mLines.back().empty();
	const int last_movable_line = has_trailing_empty_line
		? static_cast<int>(mLines.size()) - 2
		: static_cast<int>(mLines.size()) - 1;
	if (maxLine >= last_movable_line) // can't move down anymore
		return;

	Coordinates start = { minLine, 0 };
	Coordinates end = { maxLine + 1, GetLineMaxColumn(maxLine + 1)};
	u.mOperations.push_back({ GetText(start, end), start, end, UndoOperationType::Delete });

	std::set<int>::reverse_iterator rit;
	for (rit = affectedLines.rbegin(); rit != affectedLines.rend(); rit++) // lines should be sorted here
		std::swap(mLines[*rit + 1], mLines[*rit]);
	for (int c = mState.mCurrentCursor; c > -1; c--)
	{
		mState.mCursors[c].mInteractiveStart.mLine += 1;
		mState.mCursors[c].mInteractiveEnd.mLine += 1;
		// no need to set mCursorPositionChanged as cursors will remain sorted
	}

	end = { maxLine + 1, GetLineMaxColumn(maxLine + 1) }; // this line is swapped with line below, need to find new max column
	u.mOperations.push_back({ GetText(start, end), start, end, UndoOperationType::Add });
	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::ToggleLineComment()
{
	assert(!mReadOnly);
	if (mLanguageDefinition == nullptr)
		return;
	const std::string& commentString = mLanguageDefinition->mSingleLineComment;

	UndoRecord u;
	u.mBefore = mState;

	bool shouldAddComment = false;
	std::unordered_set<int> affectedLines;
	for (int c = mState.mCurrentCursor; c > -1; c--)
	{
		for (int currentLine = mState.mCursors[c].GetSelectionEnd().mLine; currentLine >= mState.mCursors[c].GetSelectionStart().mLine; currentLine--)
		{
			if (Coordinates{ currentLine, 0 } == mState.mCursors[c].GetSelectionEnd() && mState.mCursors[c].GetSelectionEnd() != mState.mCursors[c].GetSelectionStart()) // when selection ends at line start
				continue;
			affectedLines.insert(currentLine);
			int currentIndex = 0;
			while (currentIndex < static_cast<int>(mLines[currentLine].size()) && (mLines[currentLine][currentIndex].mChar == ' ' || mLines[currentLine][currentIndex].mChar == '\t')) currentIndex++;
			if (currentIndex == static_cast<int>(mLines[currentLine].size()))
				continue;
			size_t i = 0;
			while (i < commentString.length() && currentIndex + static_cast<int>(i) < static_cast<int>(mLines[currentLine].size()) && mLines[currentLine][currentIndex + i].mChar == commentString[i]) i++;
			bool matched = i == commentString.length();
			shouldAddComment |= !matched;
		}
	}

	if (shouldAddComment)
	{
		for (int currentLine : affectedLines) // order doesn't matter as changes are not multiline
		{
			Coordinates lineStart = { currentLine, 0 };
			Coordinates insertionEnd = lineStart;
			InsertTextAt(insertionEnd, (commentString + ' ').c_str()); // sets insertion end
			u.mOperations.push_back({ (commentString + ' ') , lineStart, insertionEnd, UndoOperationType::Add });
			Colorize(lineStart.mLine, 1);
		}
	}
	else
	{
		for (int currentLine : affectedLines) // order doesn't matter as changes are not multiline
		{
			int currentIndex = 0;
			while (currentIndex < static_cast<int>(mLines[currentLine].size()) && (mLines[currentLine][currentIndex].mChar == ' ' || mLines[currentLine][currentIndex].mChar == '\t')) currentIndex++;
			if (currentIndex == static_cast<int>(mLines[currentLine].size()))
				continue;
			size_t i = 0;
			while (i < commentString.length() && currentIndex + static_cast<int>(i) < static_cast<int>(mLines[currentLine].size()) && mLines[currentLine][currentIndex + i].mChar == commentString[i]) i++;
			assert(i == commentString.length());
			if (currentIndex + static_cast<int>(i) < static_cast<int>(mLines[currentLine].size()) && mLines[currentLine][currentIndex + i].mChar == ' ')
				i++;

			Coordinates start = { currentLine, GetCharacterColumn(currentLine, currentIndex) };
			Coordinates end = { currentLine, GetCharacterColumn(currentLine, currentIndex + static_cast<int>(i)) };
			u.mOperations.push_back({ GetText(start, end) , start, end, UndoOperationType::Delete});
			DeleteRange(start, end);
			Colorize(currentLine, 1);
		}
	}

	u.mAfter = mState;
	AddUndo(u);
}

void TextEditor::RemoveCurrentLines()
{
	UndoRecord u;
	u.mBefore = mState;

	if (AnyCursorHasSelection())
	{
		for (int c = mState.mCurrentCursor; c > -1; c--)
		{
			if (!mState.mCursors[c].HasSelection())
				continue;
			u.mOperations.push_back({ GetSelectedText(c), mState.mCursors[c].GetSelectionStart(), mState.mCursors[c].GetSelectionEnd(), UndoOperationType::Delete });
			DeleteSelection(c);
		}
	}
	MoveHome();
	OnCursorPositionChanged(); // might combine cursors

	for (int c = mState.mCurrentCursor; c > -1; c--)
	{
		int currentLine = mState.mCursors[c].mInteractiveEnd.mLine;
		int nextLine = currentLine + 1;
		int prevLine = currentLine - 1;

		Coordinates toDeleteStart, toDeleteEnd;
		if (static_cast<int>(mLines.size()) > nextLine) // next line exists
		{
			toDeleteStart = Coordinates(currentLine, 0);
			toDeleteEnd = Coordinates(nextLine, 0);
			SetCursorPosition({ mState.mCursors[c].mInteractiveEnd.mLine, 0 }, c);
		}
		else if (prevLine > -1) // previous line exists
		{
			toDeleteStart = Coordinates(prevLine, GetLineMaxColumn(prevLine));
			toDeleteEnd = Coordinates(currentLine, GetLineMaxColumn(currentLine));
			SetCursorPosition({ prevLine, 0 }, c);
		}
		else
		{
			toDeleteStart = Coordinates(currentLine, 0);
			toDeleteEnd = Coordinates(currentLine, GetLineMaxColumn(currentLine));
			SetCursorPosition({ currentLine, 0 }, c);
		}

		u.mOperations.push_back({ GetText(toDeleteStart, toDeleteEnd), toDeleteStart, toDeleteEnd, UndoOperationType::Delete });

		std::unordered_set<int> handledCursors = { c };
		if (toDeleteStart.mLine != toDeleteEnd.mLine)
			RemoveLine(currentLine, &handledCursors);
		else
			DeleteRange(toDeleteStart, toDeleteEnd);
	}

	u.mAfter = mState;
	AddUndo(u);
}

float TextEditor::TextDistanceToLineStart(const Coordinates& aFrom, bool aSanitizeCoords) const
{
	if (aSanitizeCoords)
		return SanitizeCoordinates(aFrom).mColumn * mCharAdvance.x;
	else
		return aFrom.mColumn * mCharAdvance.x;
}

void TextEditor::EnsureCursorVisible(int aCursor, bool aStartToo)
{
	if (aCursor == -1)
		aCursor = mState.GetLastAddedCursorIndex();

	mEnsureCursorVisible = aCursor;
	mEnsureCursorVisibleStartToo = aStartToo;
	return;
}

TextEditor::Coordinates TextEditor::SanitizeCoordinates(const Coordinates& aValue) const
{
	// Clamp in document and line limits
	auto line = Max(aValue.mLine, 0);
	auto column = Max(aValue.mColumn, 0);
	Coordinates out;
	if (line >= (int) mLines.size())
	{
		if (mLines.empty())
		{
			line = 0;
			column = 0;
		}
		else
		{
			line = (int) mLines.size() - 1;
			column = GetLineMaxColumn(line);
		}
		out = Coordinates(line, column);
	}
	else
	{
		column = mLines.empty() ? 0 : GetLineMaxColumn(line, column);
		out = Coordinates(line, column);
	}

	// Move if inside a tab character
	int charIndex = GetCharacterIndexL(out);
	if (charIndex > -1 && charIndex < static_cast<int>(mLines[out.mLine].size()) && mLines[out.mLine][charIndex].mChar == '\t')
	{
		int columnToLeft = GetCharacterColumn(out.mLine, charIndex);
		int columnToRight = GetCharacterColumn(out.mLine, GetCharacterIndexR(out));
		if (out.mColumn - columnToLeft <= columnToRight - out.mColumn)
			out.mColumn = columnToLeft;
		else
			out.mColumn = columnToRight;
	}
	return out;
}

TextEditor::Coordinates TextEditor::GetSanitizedCursorCoordinates(int aCursor, bool aStart) const
{
	aCursor = aCursor == -1 ? mState.mCurrentCursor : aCursor;
	return SanitizeCoordinates(aStart ? mState.mCursors[aCursor].mInteractiveStart : mState.mCursors[aCursor].mInteractiveEnd);
}

// Helper to check if a modifier is present in the list
static bool HasModifier(const std::vector<std::string>& modifiers, const char* mod) {
    for (const auto& m : modifiers) {
        if (m == mod) return true;
    }
    return false;
}

// Semantic token styling result
struct SemanticTokenStyle {
    TextEditor::PaletteIndex colorIndex = TextEditor::PaletteIndex::Default;
    bool italic = false;
    bool bold = false;
    bool underline = false;
    bool strikethrough = false;
};

static SemanticTokenStyle GetStyleForSemanticToken(const std::string& type, const std::vector<std::string>& modifiers) {
    SemanticTokenStyle style;

    // Check modifiers that override everything
    bool isReadonly = HasModifier(modifiers, "readonly");
    bool isStatic = HasModifier(modifiers, "static");
    bool isDeprecated = HasModifier(modifiers, "deprecated");
    bool isAbstract = HasModifier(modifiers, "abstract");
    bool isVirtual = HasModifier(modifiers, "virtual");
    bool isDefinition = HasModifier(modifiers, "definition");
    bool isDefaultLibrary = HasModifier(modifiers, "defaultLibrary");

    // Apply style modifiers
    if (isDeprecated) {
        style.strikethrough = true;
        style.colorIndex = TextEditor::PaletteIndex::Deprecated;
    }
    if (isStatic) {
        style.underline = true;
    }
    if (isAbstract || isVirtual) {
        style.italic = true;
    }
    if (isDefinition) {
        style.bold = true;
    }

    // Map token type to palette index (unless deprecated already set it)
    if (!isDeprecated) {
        if (type == "namespace") {
            style.colorIndex = TextEditor::PaletteIndex::Namespace;
        } else if (type == "type" || type == "class" || type == "enum" ||
                   type == "interface" || type == "struct") {
            style.colorIndex = TextEditor::PaletteIndex::Type;
        } else if (type == "typeParameter") {
            style.colorIndex = TextEditor::PaletteIndex::TypeParameter;
        } else if (type == "concept") {
            style.colorIndex = TextEditor::PaletteIndex::Concept;
        } else if (type == "parameter") {
            style.colorIndex = TextEditor::PaletteIndex::Parameter;
            style.italic = true;  // Parameters are typically italic
        } else if (type == "variable") {
            if (isReadonly) {
                style.colorIndex = TextEditor::PaletteIndex::Constant;
            } else if (isStatic) {
                style.colorIndex = TextEditor::PaletteIndex::StaticSymbol;
            } else {
                style.colorIndex = TextEditor::PaletteIndex::Variable;
            }
        } else if (type == "property") {
            if (isStatic) {
                style.colorIndex = TextEditor::PaletteIndex::StaticSymbol;
            } else {
                style.colorIndex = TextEditor::PaletteIndex::Property;
            }
        } else if (type == "enumMember") {
            style.colorIndex = TextEditor::PaletteIndex::EnumMember;
        } else if (type == "event") {
            style.colorIndex = TextEditor::PaletteIndex::Variable;
        } else if (type == "function") {
            if (isDefaultLibrary) {
                style.colorIndex = TextEditor::PaletteIndex::KnownIdentifier;
            } else {
                style.colorIndex = TextEditor::PaletteIndex::Function;
            }
        } else if (type == "method") {
            if (isStatic) {
                style.colorIndex = TextEditor::PaletteIndex::StaticSymbol;
            } else {
                style.colorIndex = TextEditor::PaletteIndex::Method;
            }
        } else if (type == "macro") {
            style.colorIndex = TextEditor::PaletteIndex::Macro;
        } else if (type == "keyword" || type == "modifier") {
            style.colorIndex = TextEditor::PaletteIndex::Keyword;
        } else if (type == "comment") {
            style.colorIndex = TextEditor::PaletteIndex::Comment;
        } else if (type == "string") {
            style.colorIndex = TextEditor::PaletteIndex::String;
        } else if (type == "number") {
            style.colorIndex = TextEditor::PaletteIndex::Number;
        } else if (type == "regexp") {
            style.colorIndex = TextEditor::PaletteIndex::String;
        } else if (type == "operator") {
            style.colorIndex = TextEditor::PaletteIndex::Operator;
        } else if (type == "label") {
            style.colorIndex = TextEditor::PaletteIndex::Label;
        }
    }

    return style;
}

void TextEditor::SetSemanticTokens(const std::vector<SemanticToken>& aTokens)
{
	// Store tokens for re-application after colorization
	mSemanticTokens = aTokens;
	ReapplySemanticTokens();
}

void TextEditor::ClearSemanticTokens()
{
	mSemanticTokens.clear();
}

void TextEditor::ReapplySemanticTokens()
{
	for (const auto& token : mSemanticTokens)
	{
		if (token.mLine < 0 || token.mLine >= (int)mLines.size()) continue;

		auto& line = mLines[token.mLine];
		int startIdx = token.mStartChar;
		int endIdx = startIdx + token.mLength;

		// Ensure bounds
		if (startIdx >= (int)line.size()) continue;
		if (endIdx > (int)line.size()) endIdx = (int)line.size();

		// Get full style including modifiers
		auto style = GetStyleForSemanticToken(token.mType, token.mModifiers);
		if (style.colorIndex == PaletteIndex::Default) continue;

		for (int i = startIdx; i < endIdx; ++i)
		{
			line[i].mColorIndex = style.colorIndex;
			line[i].mComment = (style.colorIndex == PaletteIndex::Comment);
			line[i].mPreprocessor = (style.colorIndex == PaletteIndex::Preprocessor ||
			                         style.colorIndex == PaletteIndex::Macro);
			// Apply style flags
			line[i].mItalic = style.italic;
			line[i].mBold = style.bold;
			line[i].mUnderline = style.underline;
			line[i].mStrikethrough = style.strikethrough;
		}
	}
}

TextEditor::Coordinates TextEditor::ScreenPosToCoordinates(const ImVec2& aPosition, bool* isOverLineNumber) const
{
	// Use cached screen position from last Render() call
	// This is more reliable than GetCursorScreenPos() which depends on current ImGui context
	const ImVec2 origin = mEditorScreenPos;
	const ImVec2 local(aPosition.x - origin.x, aPosition.y - origin.y);

	if (isOverLineNumber != nullptr)
		*isOverLineNumber = local.x < mTextStart;

	const float text_x = local.x + mScrollX - mTextStart;
	const float text_y = local.y + mScrollY;

	const int visual_line = Max(0, (int)floor(text_y / mCharAdvance.y));
	const int doc_line = GetDocumentLineForVisualLine(visual_line);
	const int segment_start = GetVisualLineStartColumn(visual_line);
	const int segment_end = GetVisualLineEndColumn(visual_line);
	const int column_in_segment = Max(0, static_cast<int>(floor(text_x / mCharAdvance.x)));
	Coordinates out{
		doc_line,
		std::clamp(segment_start + column_in_segment, segment_start, segment_end)
	};

	out = SanitizeCoordinates(out);
	if (mWordWrapEnabled)
	{
		out.mColumn = std::clamp(out.mColumn, segment_start, segment_end);
	}
	return out;
}

ImVec2 TextEditor::CoordinatesToScreenPos(const Coordinates& aPosition) const
{
	Coordinates coords = SanitizeCoordinates(aPosition);
	const int visual_line = GetVisualLineForCoordinates(coords);
	const int segment_start = GetVisualLineStartColumn(visual_line);
	const float x = mEditorScreenPos.x + mTextStart - mScrollX + (coords.mColumn - segment_start) * mCharAdvance.x;
	const float y = mEditorScreenPos.y + (visual_line * mCharAdvance.y) - mScrollY;
	return ImVec2(x, y);
}

float TextEditor::GetLineHeight() const
{
	return mCharAdvance.y;
}

int TextEditor::CharacterIndexToColumn(int aLine, int aCharIndex) const
{
	return GetCharacterColumn(aLine, aCharIndex);
}

int TextEditor::ColumnToCharacterIndex(int aLine, int aColumn) const
{
	// Use GetCharacterIndexR to convert visual column to character index
	Coordinates coords{ aLine, aColumn };
	return GetCharacterIndexR(coords);
}

TextEditor::Coordinates TextEditor::FindWordStart(const Coordinates& aFrom) const
{
	if (aFrom.mLine >= (int)mLines.size())
		return aFrom;

	int lineIndex = aFrom.mLine;
	auto& line = mLines[lineIndex];
	int charIndex = GetCharacterIndexL(aFrom);

	if (charIndex > (int)line.size() || line.size() == 0)
		return aFrom;
	if (charIndex == (int)line.size())
		charIndex--;

	bool initialIsWordChar = CharIsWordChar(line[charIndex].mChar);
	bool initialIsSpace = isspace(static_cast<unsigned char>(line[charIndex].mChar)) != 0;
	char initialChar = line[charIndex].mChar;
	while (Move(lineIndex, charIndex, true, true))
	{
		bool isWordChar = CharIsWordChar(line[charIndex].mChar);
		bool isSpace = isspace(static_cast<unsigned char>(line[charIndex].mChar)) != 0;
		if ((initialIsSpace && !isSpace) ||
			(initialIsWordChar && !isWordChar) ||
			(!initialIsWordChar && !initialIsSpace && initialChar != line[charIndex].mChar))
		{
			Move(lineIndex, charIndex, false, true); // one step to the right
			break;
		}
	}
	return { aFrom.mLine, GetCharacterColumn(aFrom.mLine, charIndex) };
}

TextEditor::Coordinates TextEditor::FindWordEnd(const Coordinates& aFrom) const
{
	if (aFrom.mLine >= (int)mLines.size())
		return aFrom;

	int lineIndex = aFrom.mLine;
	auto& line = mLines[lineIndex];
	auto charIndex = GetCharacterIndexL(aFrom);

	if (charIndex >= (int)line.size())
		return aFrom;

	bool initialIsWordChar = CharIsWordChar(line[charIndex].mChar);
	bool initialIsSpace = isspace(static_cast<unsigned char>(line[charIndex].mChar)) != 0;
	char initialChar = line[charIndex].mChar;
	while (Move(lineIndex, charIndex, false, true))
	{
		if (charIndex == static_cast<int>(line.size()))
			break;
		bool isWordChar = CharIsWordChar(line[charIndex].mChar);
		bool isSpace = isspace(static_cast<unsigned char>(line[charIndex].mChar)) != 0;
		if ((initialIsSpace && !isSpace) ||
			(initialIsWordChar && !isWordChar) ||
			(!initialIsWordChar && !initialIsSpace && initialChar != line[charIndex].mChar))
			break;
	}
	return { lineIndex, GetCharacterColumn(aFrom.mLine, charIndex) };
}

int TextEditor::GetCharacterIndexFromColumn(const Coordinates& aCoords, bool aLeftLean) const
{
	if (aCoords.mLine >= static_cast<int>(mLines.size()))
		return -1;

	const auto& line = mLines[aCoords.mLine];
	if (line.empty() || aCoords.mColumn <= 0)
		return 0;

	int column = 0;
	int index = 0;
	const int line_size = static_cast<int>(line.size());

	while (index < line_size && column < aCoords.mColumn)
	{
		const int prev_index = index;
		MoveCharIndexAndColumn(aCoords.mLine, index, column);
		if (column > aCoords.mColumn)
			return aLeftLean ? prev_index : index;
	}
	return index;
}

int TextEditor::GetCharacterIndexL(const Coordinates& aCoords) const
{
	return GetCharacterIndexFromColumn(aCoords, true);
}

int TextEditor::GetCharacterIndexR(const Coordinates& aCoords) const
{
	return GetCharacterIndexFromColumn(aCoords, false);
}

int TextEditor::GetCharacterColumn(int aLine, int aIndex) const
{
	if (aLine >= static_cast<int>(mLines.size()))
		return 0;
	int c = 0;
	int i = 0;
	while (i < aIndex && i < static_cast<int>(mLines[aLine].size()))
		MoveCharIndexAndColumn(aLine, i, c);
	return c;
}

int TextEditor::GetFirstVisibleCharacterIndex(int aLine) const
{
	return GetFirstVisibleCharacterIndex(aLine, mFirstVisibleColumn);
}

int TextEditor::GetFirstVisibleCharacterIndex(int aLine, int aFirstVisibleColumn) const
{
	if (aLine >= static_cast<int>(mLines.size()))
		return 0;
	int c = 0;
	int i = 0;
	while (c < aFirstVisibleColumn && i < static_cast<int>(mLines[aLine].size()))
		MoveCharIndexAndColumn(aLine, i, c);
	if (c > aFirstVisibleColumn)
		i--;
	return i;
}

int TextEditor::GetLineMaxColumn(int aLine, int aLimit) const
{
	if (aLine >= static_cast<int>(mLines.size()))
		return 0;
	int c = 0;
	if (aLimit == -1)
	{
		for (int i = 0; i < static_cast<int>(mLines[aLine].size()); )
			MoveCharIndexAndColumn(aLine, i, c);
	}
	else
	{
		for (int i = 0; i < static_cast<int>(mLines[aLine].size()); )
		{
			MoveCharIndexAndColumn(aLine, i, c);
			if (c > aLimit)
				return aLimit;
		}
	}
	return c;
}

TextEditor::Line& TextEditor::InsertLine(int aIndex)
{
	assert(!mReadOnly);
	auto& result = *mLines.insert(mLines.begin() + aIndex, Line());

	for (int c = 0; c <= mState.mCurrentCursor; c++) // handle multiple cursors
	{
		if (mState.mCursors[c].mInteractiveEnd.mLine >= aIndex)
			SetCursorPosition({ mState.mCursors[c].mInteractiveEnd.mLine + 1, mState.mCursors[c].mInteractiveEnd.mColumn }, c);
	}

	return result;
}

void TextEditor::RemoveLine(int aIndex, const std::unordered_set<int>* aHandledCursors)
{
	assert(!mReadOnly);
	assert(mLines.size() > 1);

	mLines.erase(mLines.begin() + aIndex);
	assert(!mLines.empty());

	// handle multiple cursors
	for (int c = 0; c <= mState.mCurrentCursor; c++)
	{
		if (mState.mCursors[c].mInteractiveEnd.mLine >= aIndex)
		{
			if (aHandledCursors == nullptr || aHandledCursors->find(c) == aHandledCursors->end()) // move up if has not been handled already
				SetCursorPosition({ mState.mCursors[c].mInteractiveEnd.mLine - 1, mState.mCursors[c].mInteractiveEnd.mColumn }, c);
		}
	}
}

void TextEditor::RemoveLines(int aStart, int aEnd)
{
	assert(!mReadOnly);
	assert(aEnd >= aStart);
	assert(mLines.size() > (size_t)(aEnd - aStart));

	mLines.erase(mLines.begin() + aStart, mLines.begin() + aEnd);
	assert(!mLines.empty());

	// handle multiple cursors
	for (int c = 0; c <= mState.mCurrentCursor; c++)
	{
		if (mState.mCursors[c].mInteractiveEnd.mLine >= aStart)
		{
			int targetLine = mState.mCursors[c].mInteractiveEnd.mLine - (aEnd - aStart);
			targetLine = targetLine < 0 ? 0 : targetLine;
			mState.mCursors[c].mInteractiveEnd.mLine = targetLine;
		}
		if (mState.mCursors[c].mInteractiveStart.mLine >= aStart)
		{
			int targetLine = mState.mCursors[c].mInteractiveStart.mLine - (aEnd - aStart);
			targetLine = targetLine < 0 ? 0 : targetLine;
			mState.mCursors[c].mInteractiveStart.mLine = targetLine;
		}
	}
}

void TextEditor::DeleteRange(const Coordinates& aStart, const Coordinates& aEnd)
{
	assert(aEnd >= aStart);
	assert(!mReadOnly);

	if (aEnd == aStart)
		return;

	auto start = GetCharacterIndexL(aStart);
	auto end = GetCharacterIndexR(aEnd);

	if (aStart.mLine == aEnd.mLine)
	{
		auto n = GetLineMaxColumn(aStart.mLine);
		if (aEnd.mColumn >= n)
			RemoveGlyphsFromLine(aStart.mLine, start); // from start to end of line
		else
			RemoveGlyphsFromLine(aStart.mLine, start, end);
	}
	else
	{
		RemoveGlyphsFromLine(aStart.mLine, start); // from start to end of line
		RemoveGlyphsFromLine(aEnd.mLine, 0, end);
		auto& firstLine = mLines[aStart.mLine];
		auto& lastLine = mLines[aEnd.mLine];

		if (aStart.mLine < aEnd.mLine)
		{
			AddGlyphsToLine(aStart.mLine, static_cast<int>(firstLine.size()), lastLine.begin(), lastLine.end());
			for (int c = 0; c <= mState.mCurrentCursor; c++) // move up cursors in line that is being moved up
			{
				// if cursor is selecting the same range we are deleting, it's because this is being called from
				// DeleteSelection which already sets the cursor position after the range is deleted
				if (mState.mCursors[c].GetSelectionStart() == aStart && mState.mCursors[c].GetSelectionEnd() == aEnd)
					continue;
				if (mState.mCursors[c].mInteractiveEnd.mLine > aEnd.mLine)
					break;
				else if (mState.mCursors[c].mInteractiveEnd.mLine != aEnd.mLine)
					continue;
				int otherCursorEndCharIndex = GetCharacterIndexR(mState.mCursors[c].mInteractiveEnd);
				int otherCursorStartCharIndex = GetCharacterIndexR(mState.mCursors[c].mInteractiveStart);
				int otherCursorNewEndCharIndex = GetCharacterIndexR(aStart) + otherCursorEndCharIndex;
				int otherCursorNewStartCharIndex = GetCharacterIndexR(aStart) + otherCursorStartCharIndex;
				auto targetEndCoords = Coordinates(aStart.mLine, GetCharacterColumn(aStart.mLine, otherCursorNewEndCharIndex));
				auto targetStartCoords = Coordinates(aStart.mLine, GetCharacterColumn(aStart.mLine, otherCursorNewStartCharIndex));
				SetCursorPosition(targetStartCoords, c, true);
				SetCursorPosition(targetEndCoords, c, false);
			}
			RemoveLines(aStart.mLine + 1, aEnd.mLine + 1);
		}
	}
}

void TextEditor::DeleteSelection(int aCursor)
{
	if (aCursor == -1)
		aCursor = mState.mCurrentCursor;

	if (mState.mCursors[aCursor].GetSelectionEnd() == mState.mCursors[aCursor].GetSelectionStart())
		return;

	Coordinates newCursorPos = mState.mCursors[aCursor].GetSelectionStart();
	DeleteRange(newCursorPos, mState.mCursors[aCursor].GetSelectionEnd());
	SetCursorPosition(newCursorPos, aCursor);
	Colorize(newCursorPos.mLine, 1);
}

void TextEditor::RemoveGlyphsFromLine(int aLine, int aStartChar, int aEndChar)
{
	int column = GetCharacterColumn(aLine, aStartChar);
	auto& line = mLines[aLine];
	OnLineChanged(true, aLine, column, aEndChar - aStartChar, true);
	line.erase(line.begin() + aStartChar, aEndChar == -1 ? line.end() : line.begin() + aEndChar);
	OnLineChanged(false, aLine, column, aEndChar - aStartChar, true);
	++mLinesRevision;  // Invalidate visual line cache
}

void TextEditor::AddGlyphsToLine(int aLine, int aTargetIndex, Line::iterator aSourceStart, Line::iterator aSourceEnd)
{
	int targetColumn = GetCharacterColumn(aLine, aTargetIndex);
	int charsInserted = static_cast<int>(std::distance(aSourceStart, aSourceEnd));
	auto& line = mLines[aLine];
	OnLineChanged(true, aLine, targetColumn, charsInserted, false);
	line.insert(line.begin() + aTargetIndex, aSourceStart, aSourceEnd);
	OnLineChanged(false, aLine, targetColumn, charsInserted, false);
	++mLinesRevision;  // Invalidate visual line cache
}

void TextEditor::AddGlyphToLine(int aLine, int aTargetIndex, Glyph aGlyph)
{
	int targetColumn = GetCharacterColumn(aLine, aTargetIndex);
	auto& line = mLines[aLine];
	OnLineChanged(true, aLine, targetColumn, 1, false);
	line.insert(line.begin() + aTargetIndex, aGlyph);
	OnLineChanged(false, aLine, targetColumn, 1, false);
	++mLinesRevision;  // Invalidate visual line cache
}

ImU32 TextEditor::GetGlyphColor(const Glyph& aGlyph) const
{
	if (mLanguageDefinition == nullptr)
		return mPalette[(int)PaletteIndex::Default];
	if (aGlyph.mComment)
		return mPalette[(int)PaletteIndex::Comment];
	if (aGlyph.mMultiLineComment)
		return mPalette[(int)PaletteIndex::MultiLineComment];
	auto const color = mPalette[(int)aGlyph.mColorIndex];
	if (aGlyph.mPreprocessor)
	{
		const auto ppcolor = mPalette[(int)PaletteIndex::Preprocessor];
		const int c0 = ((ppcolor & 0xff) + (color & 0xff)) / 2;
		const int c1 = (((ppcolor >> 8) & 0xff) + ((color >> 8) & 0xff)) / 2;
		const int c2 = (((ppcolor >> 16) & 0xff) + ((color >> 16) & 0xff)) / 2;
		const int c3 = (((ppcolor >> 24) & 0xff) + ((color >> 24) & 0xff)) / 2;
		return ImU32(c0 | (c1 << 8) | (c2 << 16) | (c3 << 24));
	}
	return color;
}

void TextEditor::HandleKeyboardInputs(bool aParentIsFocused)
{
	if (ImGui::IsWindowFocused() || aParentIsFocused)
	{
		if (ImGui::IsWindowHovered())
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
		//ImGui::CaptureKeyboardFromApp(true);

		ImGuiIO& io = ImGui::GetIO();
		auto isOSX = io.ConfigMacOSXBehaviors;
		auto alt = io.KeyAlt;
		auto ctrl = io.KeyCtrl || io.KeySuper;
		auto shift = io.KeyShift;
		auto super = io.KeySuper;

		auto isShortcut = ctrl && !alt && !shift && !super;
		auto isShiftShortcut = ctrl && shift && !alt && !super;
		auto isWordmoveKey = isOSX ? alt : ctrl;
		auto isAltOnly = alt && !ctrl && !shift && !super;
		auto isCtrlOnly = ctrl && !alt && !shift && !super;
		auto isShiftOnly = shift && !alt && !ctrl && !super;

		io.WantCaptureKeyboard = true;
		io.WantTextInput = true;

		if (!mReadOnly && isShortcut && ImGui::IsKeyPressed(ImGuiKey_Z))
			Undo();
		else if (!mReadOnly && isAltOnly && ImGui::IsKeyPressed(ImGuiKey_Backspace))
			Undo();
		else if (!mReadOnly && isShortcut && ImGui::IsKeyPressed(ImGuiKey_Y))
			Redo();
		else if (!mReadOnly && isShiftShortcut && ImGui::IsKeyPressed(ImGuiKey_Z))
			Redo();
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGuiKey_UpArrow))
			MoveUp(1, shift);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGuiKey_DownArrow))
			MoveDown(1, shift);
		else if ((isOSX ? !ctrl : !alt) && !super && ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
			MoveLeft(shift, isWordmoveKey);
		else if ((isOSX ? !ctrl : !alt) && !super && ImGui::IsKeyPressed(ImGuiKey_RightArrow))
			MoveRight(shift, isWordmoveKey);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGuiKey_PageUp))
			MoveUp(mVisibleLineCount - 2, shift);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGuiKey_PageDown))
			MoveDown(mVisibleLineCount - 2, shift);
		else if (ctrl && !alt && !super && ImGui::IsKeyPressed(ImGuiKey_Home))
			MoveTop(shift);
		else if (ctrl && !alt && !super && ImGui::IsKeyPressed(ImGuiKey_End))
			MoveBottom(shift);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGuiKey_Home))
			MoveHome(shift);
		else if (!alt && !ctrl && !super && ImGui::IsKeyPressed(ImGuiKey_End))
			MoveEnd(shift);
		else if (!mReadOnly && !alt && !shift && !super && ImGui::IsKeyPressed(ImGuiKey_Delete))
			Delete(ctrl);
		else if (!mReadOnly && !alt && !shift && !super && ImGui::IsKeyPressed(ImGuiKey_Backspace))
			Backspace(ctrl);
		else if (!mReadOnly && !alt && ctrl && shift && !super && ImGui::IsKeyPressed(ImGuiKey_K))
			RemoveCurrentLines();
		else if (!mReadOnly && !alt && ctrl && !shift && !super && ImGui::IsKeyPressed(ImGuiKey_LeftBracket))
			ChangeCurrentLinesIndentation(false);
		else if (!mReadOnly && !alt && ctrl && !shift && !super && ImGui::IsKeyPressed(ImGuiKey_RightBracket))
			ChangeCurrentLinesIndentation(true);
		else if (!alt && ctrl && shift && !super && ImGui::IsKeyPressed(ImGuiKey_UpArrow))
			MoveUpCurrentLines();
		else if (!alt && ctrl && shift && !super && ImGui::IsKeyPressed(ImGuiKey_DownArrow))
			MoveDownCurrentLines();
		else if (ctrl && alt && !shift && !super && ImGui::IsKeyPressed(ImGuiKey_UpArrow))
			AddCursorAbove();
		else if (ctrl && alt && !shift && !super && ImGui::IsKeyPressed(ImGuiKey_DownArrow))
			AddCursorBelow();
		else if (!mReadOnly && !alt && ctrl && !shift && !super && ImGui::IsKeyPressed(ImGuiKey_Slash))
			ToggleLineComment();
		else if (isCtrlOnly && ImGui::IsKeyPressed(ImGuiKey_Insert))
			Copy();
		else if (isShortcut && ImGui::IsKeyPressed(ImGuiKey_C))
			Copy();
		else if (!mReadOnly && isShiftOnly && ImGui::IsKeyPressed(ImGuiKey_Insert))
			Paste();
		else if (!mReadOnly && isShortcut && ImGui::IsKeyPressed(ImGuiKey_V))
			Paste();
		else if (isShortcut && ImGui::IsKeyPressed(ImGuiKey_X))
			Cut();
		else if (isShiftOnly && ImGui::IsKeyPressed(ImGuiKey_Delete))
			Cut();
		else if (isShortcut && ImGui::IsKeyPressed(ImGuiKey_A))
			SelectAll();
		else if (isShortcut && ImGui::IsKeyPressed(ImGuiKey_D))
		{
			if (!AnyCursorHasSelection())
			{
				Coordinates cursorCoords = GetSanitizedCursorCoordinates();
				Coordinates wordStart = FindWordStart(cursorCoords);
				Coordinates wordEnd = FindWordEnd(cursorCoords);
				if (wordStart != wordEnd)
				{
					SetSelection(wordStart, wordEnd, mState.mCurrentCursor);
					EnsureCursorVisible(-1, true);
				}
			}
			else
			{
				AddCursorForNextOccurrence();
			}
		}
		else if (isShiftShortcut && ImGui::IsKeyPressed(ImGuiKey_L))
		{
			const int cursorIndex = mState.GetLastAddedCursorIndex();
			const Cursor& cursor = mState.mCursors[cursorIndex];
			const bool caseSensitive = mLanguageDefinition == nullptr ? true : mLanguageDefinition->mCaseSensitive;

			if (cursor.HasSelection())
			{
				const Coordinates selectionStart = cursor.GetSelectionStart();
				const Coordinates selectionEnd = cursor.GetSelectionEnd();
				std::string selectionText = GetText(selectionStart, selectionEnd);
				if (!selectionText.empty())
					SelectAllOccurrencesOf(selectionText.c_str(), static_cast<int>(selectionText.size()), caseSensitive);
			}
			else
			{
				Coordinates cursorCoords = GetSanitizedCursorCoordinates();
				Coordinates wordStart = FindWordStart(cursorCoords);
				Coordinates wordEnd = FindWordEnd(cursorCoords);
				if (wordStart != wordEnd)
				{
					std::string wordText = GetText(wordStart, wordEnd);
					if (!wordText.empty())
						SelectAllOccurrencesOf(wordText.c_str(), static_cast<int>(wordText.size()), caseSensitive);
				}
			}
		}
		else if (!mReadOnly && !alt && !ctrl && !shift && !super && (ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter)))
			EnterCharacter('\n', false);
		else if (!mReadOnly && !alt && !ctrl && !super && ImGui::IsKeyPressed(ImGuiKey_Tab))
			EnterCharacter('\t', shift);
		if (!mReadOnly && !io.InputQueueCharacters.empty() && !(ctrl && !alt) && !super) // See https://github.com/santaclose/ImGuiColorTextEdit/pull/34
		{
			for (int i = 0; i < io.InputQueueCharacters.Size; i++)
			{
				auto c = io.InputQueueCharacters[i];
				if (c != 0 && (c == '\n' || c >= 32))
					EnterCharacter(c, shift);
			}
			io.InputQueueCharacters.resize(0);
		}
	}
}

void TextEditor::HandleMouseInputs()
{
	ImGuiIO& io = ImGui::GetIO();
	auto shift = io.KeyShift;
	auto ctrl = io.KeyCtrl || io.KeySuper;
	auto alt = io.KeyAlt;

	/*
	Pan with middle mouse button
	*/
	mPanning &= ImGui::IsMouseDown(2);
	if (mPanning && ImGui::IsMouseDragging(2))
	{
		ImVec2 scroll = { ImGui::GetScrollX(), ImGui::GetScrollY() };
		ImVec2 currentMousePos = ImGui::GetMouseDragDelta(2);
			ImVec2 mouseDelta = {
				currentMousePos.x - mLastMousePos.x,
				currentMousePos.y - mLastMousePos.y
			};
			ImGui::SetScrollY(scroll.y - mouseDelta.y);
			if (!mWordWrapEnabled)
				ImGui::SetScrollX(scroll.x - mouseDelta.x);
			mLastMousePos = currentMousePos;
		}

	// Mouse left button dragging (=> update selection)
	mDraggingSelection &= ImGui::IsMouseDown(0);
	if (mDraggingSelection && ImGui::IsMouseDragging(0))
	{
		io.WantCaptureMouse = true;
		Coordinates cursorCoords = ScreenPosToCoordinates(ImGui::GetMousePos());
		SetCursorPosition(cursorCoords, mState.GetLastAddedCursorIndex(), false);
	}

	if (ImGui::IsWindowHovered())
	{
		auto click = ImGui::IsMouseClicked(0);
		if (!shift && !alt)
		{
			auto doubleClick = ImGui::IsMouseDoubleClicked(0);
			auto t = ImGui::GetTime();
			auto tripleClick = click && !doubleClick &&
				(mLastClickTime != -1.0f && (t - mLastClickTime) < io.MouseDoubleClickTime &&
					Distance(io.MousePos, mLastClickPos) < 0.01f);

			if (click)
				mDraggingSelection = true;

			/*
			Pan with middle mouse button
			*/

			if (ImGui::IsMouseClicked(2))
			{
				mPanning = true;
				mLastMousePos = ImGui::GetMouseDragDelta(2);
			}

			/*
			Left mouse button triple click
			When mCtrlClickForNavigation is true, use Alt for multi-cursor instead of Ctrl
			*/

			if (tripleClick)
			{
				// Skip Ctrl+triple-click when navigation mode is on (let LSP handle it)
				if (ctrl && mCtrlClickForNavigation)
					; // Do nothing, let LSP handle navigation
				else if ((ctrl && !mCtrlClickForNavigation) || (alt && mCtrlClickForNavigation))
					mState.AddCursor();
				else
					mState.mCurrentCursor = 0;

				if (!ctrl || !mCtrlClickForNavigation) // Only select if not Ctrl+click in nav mode
				{
					Coordinates cursorCoords = ScreenPosToCoordinates(ImGui::GetMousePos());
					Coordinates targetCursorPos = cursorCoords.mLine < static_cast<int>(mLines.size()) - 1 ?
						Coordinates{ cursorCoords.mLine + 1, 0 } :
						Coordinates{ cursorCoords.mLine, GetLineMaxColumn(cursorCoords.mLine) };
					SetSelection({ cursorCoords.mLine, 0 }, targetCursorPos, mState.mCurrentCursor);
				}

				mLastClickTime = -1.0f;
			}

			/*
			Left mouse button double click
			When mCtrlClickForNavigation is true, use Alt for multi-cursor instead of Ctrl
			*/

			else if (doubleClick)
			{
				// Skip Ctrl+double-click when navigation mode is on (let LSP handle it)
				if (ctrl && mCtrlClickForNavigation)
					; // Do nothing, let LSP handle navigation
				else if ((ctrl && !mCtrlClickForNavigation) || (alt && mCtrlClickForNavigation))
					mState.AddCursor();
				else
					mState.mCurrentCursor = 0;

				if (!ctrl || !mCtrlClickForNavigation) // Only select word if not Ctrl+click in nav mode
				{
					Coordinates cursorCoords = ScreenPosToCoordinates(ImGui::GetMousePos());
					SetSelection(FindWordStart(cursorCoords), FindWordEnd(cursorCoords), mState.mCurrentCursor);
				}

				mLastClickTime = (float)ImGui::GetTime();
				mLastClickPos = io.MousePos;
			}

			/*
			Left mouse button click
			When mCtrlClickForNavigation is true, Ctrl+Click is reserved for navigation (handled by LSP)
			Use Alt+Click for multi-cursor instead
			*/
			else if (click)
			{
				// When navigation mode is on, Ctrl+Click doesn't add cursor - it's for go-to-definition
				if (ctrl && mCtrlClickForNavigation)
				{
					// Don't add cursor, don't change position - let LSP handler deal with navigation
					// Just update click tracking for triple-click detection
					mLastClickTime = (float)ImGui::GetTime();
					mLastClickPos = io.MousePos;
				}
				else
				{
					// Alt+Click adds cursor when nav mode is on, Ctrl+Click adds cursor when nav mode is off
					if ((ctrl && !mCtrlClickForNavigation) || (alt && mCtrlClickForNavigation))
						mState.AddCursor();
					else
						mState.mCurrentCursor = 0;

					bool isOverLineNumber;
					Coordinates cursorCoords = ScreenPosToCoordinates(ImGui::GetMousePos(), &isOverLineNumber);
					if (isOverLineNumber)
					{
						Coordinates targetCursorPos = cursorCoords.mLine < static_cast<int>(mLines.size()) - 1 ?
							Coordinates{ cursorCoords.mLine + 1, 0 } :
							Coordinates{ cursorCoords.mLine, GetLineMaxColumn(cursorCoords.mLine) };
						SetSelection({ cursorCoords.mLine, 0 }, targetCursorPos, mState.mCurrentCursor);
					}
					else
						SetCursorPosition(cursorCoords, mState.GetLastAddedCursorIndex());

					mLastClickTime = (float)ImGui::GetTime();
					mLastClickPos = io.MousePos;
				}
			}
			else if (ImGui::IsMouseReleased(0))
			{
				mState.SortCursorsFromTopToBottom();
				MergeCursorsIfPossible();
			}
		}
		else if (shift)
		{
			if (click)
			{
				Coordinates newSelection = ScreenPosToCoordinates(ImGui::GetMousePos());
				SetCursorPosition(newSelection, mState.mCurrentCursor, false);
			}
		}
	}
}

void TextEditor::UpdateViewVariables(float aScrollX, float aScrollY)
{
	mContentHeight = ImGui::GetWindowHeight() - (IsHorizontalScrollbarVisible() ? IMGUI_SCROLLBAR_WIDTH : 0.0f);
	mContentWidth = ImGui::GetWindowWidth() - (IsVerticalScrollbarVisible() ? IMGUI_SCROLLBAR_WIDTH : 0.0f);

	mVisibleLineCount = Max((int)ceil(mContentHeight / mCharAdvance.y), 0);
	mFirstVisibleLine = Max((int)(aScrollY / mCharAdvance.y), 0);
	mLastVisibleLine = Max((int)((mContentHeight + aScrollY) / mCharAdvance.y), 0);

	if (mWordWrapEnabled)
	{
		const float available_width = Max(mContentWidth - mTextStart, mCharAdvance.x);
		mWrapColumn = Max(1, static_cast<int>(std::floor(available_width / mCharAdvance.x)));
		mVisibleColumnCount = mWrapColumn;
		mFirstVisibleColumn = 0;
		mLastVisibleColumn = mWrapColumn;
	}
	else
	{
		mWrapColumn = 120;
		mVisibleColumnCount = Max((int)ceil((mContentWidth - Max(mTextStart - aScrollX, 0.0f)) / mCharAdvance.x), 0);
		mFirstVisibleColumn = Max((int)(Max(aScrollX - mTextStart, 0.0f) / mCharAdvance.x), 0);
		mLastVisibleColumn = Max((int)((mContentWidth + aScrollX - mTextStart) / mCharAdvance.x), 0);
	}
}

void TextEditor::Render(bool aParentIsFocused)
{
	/* Compute metrics using the editor-local zoom model. */
	const float fontSize = ImGui::GetFontSize() * mZoomLevel;
	const float fontWidth = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, -1.0f, "#", nullptr, nullptr).x;
	const float fontHeight = ImGui::GetTextLineHeightWithSpacing() * mZoomLevel;
	mCharAdvance = ImVec2(fontWidth, fontHeight * mLineSpacing);

	// Deduce mTextStart by evaluating gutter width + line numbers width.
	// Reserve space for diagnostic icons so the gutter doesn't clip them.
	const float gutter_icon_size = fontSize * 0.55f;
	const float gutter_icon_padding = 4.0f;
	const float gutter_icon_area = gutter_icon_padding + gutter_icon_size + gutter_icon_padding;
	mTextStart = static_cast<float>(mLeftMargin) + gutter_icon_area;
	std::array<char, 16> line_number_buffer{};
	if (mShowLineNumbers)
	{
		snprintf(line_number_buffer.data(), line_number_buffer.size(), " %d ", GetMaxLineNumber());
		mTextStart += ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, -1.0f, line_number_buffer.data(), nullptr, nullptr).x;
	}

	const ImVec2 window_pos = ImGui::GetWindowPos();
	const ImVec2 cursor_pos = ImGui::GetCursorPos();
	mScrollX = mWordWrapEnabled ? 0.0f : ImGui::GetScrollX();
	mScrollY = ImGui::GetScrollY();
	if (mWordWrapEnabled && ImGui::GetScrollX() != 0.0f)
	{
		ImGui::SetScrollX(0.0f);
	}
	// GetCursorPos returns local coordinates with scroll canceled out.
	mEditorScreenPos = ImVec2(window_pos.x + cursor_pos.x, window_pos.y + cursor_pos.y);
	UpdateViewVariables(mScrollX, mScrollY);

	int maxColumnLimited = 0;
	int maxGhostColumn = 0;
	const int visual_line_count = GetVisualLineCount();
		if (visual_line_count > 0)
		{
			auto drawList = ImGui::GetWindowDrawList();
			auto* font = ImGui::GetFont();
			auto draw_text = [drawList, font, fontSize](const ImVec2& pos, ImU32 color, const char* text) {
				drawList->AddText(font, fontSize, pos, color, text);
			};
			auto draw_text_range = [drawList, font, fontSize](const ImVec2& pos, ImU32 color, const char* begin, const char* end) {
				drawList->AddText(font, fontSize, pos, color, begin, end);
			};
			float spaceSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, -1.0f, " ", nullptr, nullptr).x;
			const float underlineThickness = Max(1.0f, fontSize * 0.09f);
			// Smooth sine wave parameters for elegant squiggly underlines
			const float waveAmplitude = Max(1.0f, fontSize * 0.11f);
			const float waveWavelength = Max(spaceSize * 1.8f, fontSize * 1.2f);
			const float waveFrequency = 1.0f / waveWavelength;
			const float waveSampleStep = Max(0.75f, Max(spaceSize * 0.10f, fontSize * 0.05f));
			std::vector<ImVec2> wave_points{};

		auto drawUnderline = [&](float startX, float endX, float y, ImU32 color, UnderlineStyle style, DiagnosticSeverity severity)
		{
			if (endX <= startX)
				return;

			auto apply_alpha_mul = [](ImU32 in, float multiplier) -> ImU32
			{
				const auto r = (in >> IM_COL32_R_SHIFT) & 0xFFu;
				const auto g = (in >> IM_COL32_G_SHIFT) & 0xFFu;
				const auto b = (in >> IM_COL32_B_SHIFT) & 0xFFu;
				const auto a = (in >> IM_COL32_A_SHIFT) & 0xFFu;
				const float clamped = std::clamp(multiplier, 0.0f, 1.0f);
				const auto new_a = static_cast<ImU32>(static_cast<float>(a) * clamped);
				return IM_COL32(r, g, b, new_a);
			};

			UnderlineStyle final_style = style;
			switch (severity)
			{
			case DiagnosticSeverity::Error:
				final_style = UnderlineStyle::Wavy;
				break;
			case DiagnosticSeverity::Warning:
				final_style = UnderlineStyle::Solid;
				color = apply_alpha_mul(color, 0.85f);
				break;
			case DiagnosticSeverity::Information:
				final_style = UnderlineStyle::Solid;
				color = apply_alpha_mul(color, 0.55f);
				break;
			case DiagnosticSeverity::Hint:
				final_style = UnderlineStyle::Solid;
				color = apply_alpha_mul(color, 0.45f);
				break;
			case DiagnosticSeverity::None:
				break;
			}

			if (final_style == UnderlineStyle::Solid)
			{
				drawList->AddLine(ImVec2(startX, y), ImVec2(endX, y), color, underlineThickness);
				return;
			}
			// Draw smooth sine wave squiggly line using Bezier curves
			// This produces a much nicer VS Code-like underline than the jagged sawtooth
			float x = startX;
			const float pi = 3.14159265359f;

			// Build path points for smooth curve
				wave_points.clear();
				wave_points.reserve(static_cast<size_t>((endX - startX) / waveSampleStep) + 2);

			while (x <= endX)
			{
				float phase = (x - startX) * waveFrequency;
				float yOffset = std::sin(phase * pi * 2.0f) * waveAmplitude;
					wave_points.push_back(ImVec2(x, y + yOffset));
				x += waveSampleStep;
			}
			// Add final point at exact endX
				if (wave_points.empty() || wave_points.back().x < endX)
				{
					float phase = (endX - startX) * waveFrequency;
					float yOffset = std::sin(phase * pi * 2.0f) * waveAmplitude;
					wave_points.push_back(ImVec2(endX, y + yOffset));
				}

			// Draw the polyline with anti-aliasing
				if (wave_points.size() >= 2)
				{
					drawList->AddPolyline(wave_points.data(), static_cast<int>(wave_points.size()), color,
						ImDrawFlags_None, underlineThickness);
				}
			};

		auto render_ghost_line = [&](const ImVec2& lineStartScreenPos, const ImVec2& textScreenPos, const GhostLine& ghost) -> int
		{
			if (ghost.mBackgroundColor != 0)
			{
				const ImVec2 bg_start(mEditorScreenPos.x, lineStartScreenPos.y);
				const ImVec2 bg_end(mEditorScreenPos.x + mContentWidth, lineStartScreenPos.y + mCharAdvance.y);
				drawList->AddRectFilled(bg_start, bg_end, ghost.mBackgroundColor);
			}

			if (ghost.mMarkerColor != 0)
			{
				const float marker_width = Max(2.0f, mCharAdvance.x * 0.15f);
				const float marker_x = mEditorScreenPos.x + 1.0f;
				drawList->AddRectFilled(ImVec2(marker_x, lineStartScreenPos.y),
				                        ImVec2(marker_x + marker_width, lineStartScreenPos.y + mCharAdvance.y),
				                        ghost.mMarkerColor);
			}

			if (ghost.mSeparatorColor != 0)
			{
				const float y_top = lineStartScreenPos.y + 0.5f;
				const float y_bottom = lineStartScreenPos.y + mCharAdvance.y - 0.5f;
				const ImVec2 line_start(mEditorScreenPos.x, y_top);
				const ImVec2 line_end(mEditorScreenPos.x + mContentWidth, y_top);
				drawList->AddLine(line_start, line_end, ghost.mSeparatorColor, 1.0f);
				drawList->AddLine(ImVec2(mEditorScreenPos.x, y_bottom),
				                  ImVec2(mEditorScreenPos.x + mContentWidth, y_bottom),
				                  ghost.mSeparatorColor,
				                  1.0f);
			}

				if (mShowLineNumbers && ghost.mLineNumber > 0)
				{
					snprintf(line_number_buffer.data(), line_number_buffer.size(), "%d  ", ghost.mLineNumber);
					float lineNoWidth = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, -1.0f, line_number_buffer.data(), nullptr, nullptr).x;
					ImU32 line_color = ghost.mTextColor != 0 ? ghost.mTextColor : mPalette[(int)PaletteIndex::LineNumber];
					draw_text(ImVec2(lineStartScreenPos.x + mTextStart - lineNoWidth, lineStartScreenPos.y), line_color, line_number_buffer.data());
				}

			ImU32 text_color = ghost.mTextColor != 0 ? ghost.mTextColor : mPalette[(int)PaletteIndex::Default];
			int column = 0;
			int char_index = 0;
				const int text_size = static_cast<int>(ghost.mText.size());
			while (char_index < text_size && column <= mLastVisibleColumn)
			{
				const char c = ghost.mText[char_index];
				ImVec2 targetGlyphPos = { textScreenPos.x + column * mCharAdvance.x, lineStartScreenPos.y };

				if (c == '\t')
				{
						if (mShowWhitespaces)
						{
							ImVec2 p1, p2, p3, p4;
							const auto s = fontSize;
							const auto x1 = targetGlyphPos.x + mCharAdvance.x * 0.3f;
							const auto y = targetGlyphPos.y + fontHeight * 0.5f;

						if (mShortTabs)
						{
							const auto x2 = targetGlyphPos.x + mCharAdvance.x;
							p1 = ImVec2(x1, y);
							p2 = ImVec2(x2, y);
							p3 = ImVec2(x2 - s * 0.16f, y - s * 0.16f);
							p4 = ImVec2(x2 - s * 0.16f, y + s * 0.16f);
						}
						else
						{
							const auto x2 = targetGlyphPos.x + TabSizeAtColumn(column) * mCharAdvance.x - mCharAdvance.x * 0.3f;
							p1 = ImVec2(x1, y);
							p2 = ImVec2(x2, y);
							p3 = ImVec2(x2 - s * 0.2f, y - s * 0.2f);
							p4 = ImVec2(x2 - s * 0.2f, y + s * 0.2f);
						}

						drawList->AddLine(p1, p2, mPalette[(int)PaletteIndex::ControlCharacter]);
						drawList->AddLine(p2, p3, mPalette[(int)PaletteIndex::ControlCharacter]);
						drawList->AddLine(p2, p4, mPalette[(int)PaletteIndex::ControlCharacter]);
					}

					column += TabSizeAtColumn(column);
					char_index += 1;
					continue;
				}
				if (c == ' ')
				{
						if (mShowWhitespaces)
						{
							const auto s = fontSize;
							const auto x = targetGlyphPos.x + spaceSize * 0.5f;
							const auto y = targetGlyphPos.y + s * 0.5f;
							drawList->AddCircleFilled(ImVec2(x, y), 1.5f, mPalette[(int)PaletteIndex::ControlCharacter], 4);
					}
					column += 1;
					char_index += 1;
					continue;
				}

				int seqLength = UTF8CharLength(c);
				if (char_index + seqLength > text_size)
				{
					seqLength = 1;
				}

				draw_text_range(
					targetGlyphPos,
					text_color,
					ghost.mText.data() + char_index,
					ghost.mText.data() + char_index + seqLength);

				column += 1;
				char_index += seqLength;
			}
			return column;
		};

		for (int visual_line = mFirstVisibleLine; visual_line <= mLastVisibleLine && visual_line < visual_line_count; ++visual_line)
		{
			ImVec2 lineStartScreenPos = ImVec2(mEditorScreenPos.x - mScrollX,
			                                   mEditorScreenPos.y + visual_line * mCharAdvance.y - mScrollY);
			ImVec2 textScreenPos = ImVec2(lineStartScreenPos.x + mTextStart, lineStartScreenPos.y);

			const GhostLine* ghost_line = GetGhostLineForVisualLine(visual_line);
			if (ghost_line != nullptr)
			{
				maxGhostColumn = Max(render_ghost_line(lineStartScreenPos, textScreenPos, *ghost_line), maxGhostColumn);
				continue;
			}

			if (visual_line < 0 || visual_line >= static_cast<int>(mVisualLines.size()))
			{
				continue;
			}
			const int lineNo = mVisualLines[static_cast<std::size_t>(visual_line)].mDocumentLine;
			if (lineNo < 0 || lineNo >= static_cast<int>(mLines.size()))
			{
				continue;
			}

			auto& line = mLines[lineNo];
			const int line_segment_start_column = GetVisualLineStartColumn(visual_line);
			const int line_segment_end_column = GetVisualLineEndColumn(visual_line);
			const bool show_gutter = !mWordWrapEnabled || line_segment_start_column == 0;
			textScreenPos.x -= line_segment_start_column * mCharAdvance.x;
			maxColumnLimited = Max(line_segment_end_column - line_segment_start_column, maxColumnLimited);

			Coordinates lineStartCoord(lineNo, line_segment_start_column);
			Coordinates lineEndCoord(lineNo, line_segment_end_column);
			const int lineMaxVisibleColumn = line_segment_end_column;

			if (!mHighlights.empty())
			{
				for (const auto& highlight : mHighlights)
				{
					int startLine = highlight.mStartLine;
					int endLine = highlight.mEndLine;
					int startIndex = Max(0, highlight.mStartCharIndex);
					int endIndex = Max(0, highlight.mEndCharIndex);

					if (endLine < startLine || (endLine == startLine && endIndex < startIndex))
					{
						std::swap(startLine, endLine);
						std::swap(startIndex, endIndex);
					}

					if (lineNo < startLine || lineNo > endLine)
					{
						continue;
					}

					int startColumn = (lineNo == startLine) ? GetCharacterColumn(lineNo, startIndex) : line_segment_start_column;
					int endColumn = (lineNo == endLine) ? GetCharacterColumn(lineNo, endIndex) : lineMaxVisibleColumn;

					startColumn = Max(line_segment_start_column, Min(startColumn, lineMaxVisibleColumn));
					endColumn = Max(line_segment_start_column, Min(endColumn, lineMaxVisibleColumn));
					if (endColumn <= startColumn)
					{
						continue;
					}

					float rectStart = TextDistanceToLineStart({ lineNo, startColumn }, false);
					float rectEnd = TextDistanceToLineStart({ lineNo, endColumn }, false);
					ImU32 color = highlight.mColor != 0 ? highlight.mColor : mPalette[(int)PaletteIndex::Selection];

						drawList->AddRectFilled(
							ImVec2{ textScreenPos.x + rectStart, lineStartScreenPos.y },
							ImVec2{ textScreenPos.x + rectEnd, lineStartScreenPos.y + mCharAdvance.y },
							color);
				}
			}

			// Draw link highlight (Ctrl+hover for go-to-definition)
			if (mLinkHighlight.has_value() && mLinkHighlight->mLine == lineNo)
			{
				const auto& link = *mLinkHighlight;
				int startColumn = GetCharacterColumn(lineNo, link.mStartCharIndex);
				int endColumn = GetCharacterColumn(lineNo, link.mEndCharIndex);

					startColumn = Max(line_segment_start_column, Min(startColumn, lineMaxVisibleColumn));
					endColumn = Max(line_segment_start_column, Min(endColumn, lineMaxVisibleColumn));

				if (endColumn > startColumn)
				{
					float rectStart = TextDistanceToLineStart({ lineNo, startColumn }, false);
					float rectEnd = TextDistanceToLineStart({ lineNo, endColumn }, false);

					// Draw subtle background highlight
					ImU32 bgColor = link.mColor != 0 ? link.mColor : mPalette[(int)PaletteIndex::Selection];
					// Apply transparency to background
					bgColor = (bgColor & 0x00FFFFFFu) | 0x30000000u; // ~19% opacity
						drawList->AddRectFilled(
							ImVec2{ textScreenPos.x + rectStart, lineStartScreenPos.y },
							ImVec2{ textScreenPos.x + rectEnd, lineStartScreenPos.y + mCharAdvance.y },
							bgColor);

					// Draw underline if enabled
					if (link.mUnderline)
					{
						ImU32 underlineColor = link.mColor != 0 ? link.mColor : mPalette[(int)PaletteIndex::Default];
						float underlineY = lineStartScreenPos.y + mCharAdvance.y - 2.0f;
							drawList->AddLine(
								ImVec2{ textScreenPos.x + rectStart, underlineY },
								ImVec2{ textScreenPos.x + rectEnd, underlineY },
								underlineColor,
								1.0f);
					}
				}
			}

			// Draw selection for the current line
			for (int c = 0; c <= mState.mCurrentCursor; c++)
			{
				float rectStart = -1.0f;
				float rectEnd = -1.0f;
				Coordinates cursorSelectionStart = mState.mCursors[c].GetSelectionStart();
				Coordinates cursorSelectionEnd = mState.mCursors[c].GetSelectionEnd();
				assert(cursorSelectionStart <= cursorSelectionEnd);

				if (cursorSelectionStart <= lineEndCoord)
					rectStart = cursorSelectionStart > lineStartCoord ? TextDistanceToLineStart(cursorSelectionStart) : 0.0f;
				if (cursorSelectionEnd > lineStartCoord)
					rectEnd = TextDistanceToLineStart(cursorSelectionEnd < lineEndCoord ? cursorSelectionEnd : lineEndCoord);
				if (cursorSelectionEnd.mLine > lineNo || (cursorSelectionEnd.mLine == lineNo && cursorSelectionEnd > lineEndCoord))
				{
					rectEnd += mCharAdvance.x;
				}

				if (rectStart != -1 && rectEnd != -1 && rectStart < rectEnd)
				{
					drawList->AddRectFilled(
						ImVec2{ textScreenPos.x + rectStart, lineStartScreenPos.y },
						ImVec2{ textScreenPos.x + rectEnd, lineStartScreenPos.y + mCharAdvance.y },
						mPalette[(int)PaletteIndex::Selection]);
				}
				}

				// Draw line number (right aligned)
					if (mShowLineNumbers && show_gutter)
					{
						snprintf(line_number_buffer.data(), line_number_buffer.size(), "%d  ", lineNo + 1);
						float lineNoWidth = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, -1.0f, line_number_buffer.data(), nullptr, nullptr).x;
						draw_text(ImVec2(lineStartScreenPos.x + mTextStart - lineNoWidth, lineStartScreenPos.y), mPalette[(int)PaletteIndex::LineNumber], line_number_buffer.data());
					}

				// Draw diagnostic gutter icons (in the left margin before line numbers)
				if (!mUnderlines.empty() && show_gutter)
				{
				DiagnosticSeverity worstSeverity = DiagnosticSeverity::None;
				ImU32 iconColor = 0;

				// Find the worst severity diagnostic on this line
				for (const auto& underline : mUnderlines)
				{
					if (lineNo >= underline.mStartLine && lineNo <= underline.mEndLine)
					{
						if (underline.mSeverity != DiagnosticSeverity::None &&
							(static_cast<int>(underline.mSeverity) < static_cast<int>(worstSeverity) ||
							 worstSeverity == DiagnosticSeverity::None))
						{
							worstSeverity = underline.mSeverity;
							iconColor = underline.mColor;
						}
					}
				}

				// Draw gutter icon based on severity
				if (worstSeverity != DiagnosticSeverity::None)
				{
					const float iconSize = fontSize * 0.55f;
					const float iconX = lineStartScreenPos.x + 4.0f;
					const float iconY = lineStartScreenPos.y + (mCharAdvance.y - iconSize) * 0.5f;
					const ImVec2 iconCenter(iconX + iconSize * 0.5f, iconY + iconSize * 0.5f);

					if (worstSeverity == DiagnosticSeverity::Error)
					{
						// Red filled circle with X
						drawList->AddCircleFilled(iconCenter, iconSize * 0.5f, iconColor, 16);
						const float cross = iconSize * 0.25f;
						ImU32 white = IM_COL32(255, 255, 255, 255);
						drawList->AddLine(ImVec2(iconCenter.x - cross, iconCenter.y - cross),
						                  ImVec2(iconCenter.x + cross, iconCenter.y + cross), white, 1.5f);
						drawList->AddLine(ImVec2(iconCenter.x + cross, iconCenter.y - cross),
						                  ImVec2(iconCenter.x - cross, iconCenter.y + cross), white, 1.5f);
					}
					else if (worstSeverity == DiagnosticSeverity::Warning)
					{
						// Yellow triangle with !
						const float triHeight = iconSize * 0.85f;
						const float triWidth = iconSize * 0.9f;
						ImVec2 p1(iconCenter.x, iconY + iconSize * 0.05f);
						ImVec2 p2(iconCenter.x - triWidth * 0.5f, iconY + triHeight);
						ImVec2 p3(iconCenter.x + triWidth * 0.5f, iconY + triHeight);
						drawList->AddTriangleFilled(p1, p2, p3, iconColor);
						// Draw ! mark
						ImU32 dark = IM_COL32(0, 0, 0, 220);
						const float exclamTop = iconCenter.y - iconSize * 0.15f;
						const float exclamBot = iconCenter.y + iconSize * 0.1f;
						drawList->AddLine(ImVec2(iconCenter.x, exclamTop), ImVec2(iconCenter.x, exclamBot), dark, 1.5f);
						drawList->AddCircleFilled(ImVec2(iconCenter.x, iconCenter.y + iconSize * 0.22f), 1.2f, dark);
					}
					else if (worstSeverity == DiagnosticSeverity::Information)
					{
						// Blue circle with i
						drawList->AddCircleFilled(iconCenter, iconSize * 0.5f, iconColor, 16);
						ImU32 white = IM_COL32(255, 255, 255, 255);
						drawList->AddCircleFilled(ImVec2(iconCenter.x, iconCenter.y - iconSize * 0.15f), 1.2f, white);
						drawList->AddLine(ImVec2(iconCenter.x, iconCenter.y - iconSize * 0.02f),
						                  ImVec2(iconCenter.x, iconCenter.y + iconSize * 0.22f), white, 1.5f);
					}
					else if (worstSeverity == DiagnosticSeverity::Hint)
					{
						// Light bulb / subtle circle
						drawList->AddCircle(iconCenter, iconSize * 0.4f, iconColor, 16, 1.5f);
					}
				}
			}

				const bool focused = ImGui::IsWindowFocused() || aParentIsFocused;
				if (focused)
				{
					const float blink_period = 1.0f; // seconds
					float t = std::fmod(static_cast<float>(ImGui::GetTime()), blink_period);
					float blink_alpha = 0.5f + 0.5f * std::cos(t * 3.14159265f * 2.0f / blink_period);
					blink_alpha = 0.4f + blink_alpha * 0.6f; // Clamp to 0.4-1.0 range

					for (int c = 0; c <= mState.mCurrentCursor; ++c)
					{
						const Coordinates cursorCoords = mState.mCursors[c].mInteractiveEnd;
						if (cursorCoords.mLine != lineNo)
							continue;
						if (mWordWrapEnabled &&
						    (cursorCoords.mColumn < line_segment_start_column ||
						     cursorCoords.mColumn > line_segment_end_column))
						{
							continue;
						}

						float width = Max(2.0f, fontSize * 0.08f);
						float cx = TextDistanceToLineStart(cursorCoords);

						ImU32 cursorColor = mPalette[(int)PaletteIndex::Cursor];
						ImVec4 colorVec = ImGui::ColorConvertU32ToFloat4(cursorColor);
						colorVec.w *= blink_alpha;
						cursorColor = ImGui::ColorConvertFloat4ToU32(colorVec);

						ImVec2 cstart(textScreenPos.x + cx, lineStartScreenPos.y);
						ImVec2 cend(textScreenPos.x + cx + width, lineStartScreenPos.y + mCharAdvance.y);
						drawList->AddRectFilled(cstart, cend, cursorColor);
						if (mCursorOnBracket)
						{
							ImVec2 topLeft = { cstart.x, lineStartScreenPos.y + fontHeight + 1.0f };
							ImVec2 bottomRight = { topLeft.x + mCharAdvance.x, topLeft.y + 1.0f };
							drawList->AddRectFilled(topLeft, bottomRight, cursorColor);
						}
					}
				}

				// Render colorized text
				std::array<char, 8> glyph_utf8{};
				int charIndex = GetFirstVisibleCharacterIndex(lineNo, line_segment_start_column);
				int column = line_segment_start_column;
				while (charIndex < static_cast<int>(mLines[lineNo].size()) && column <= line_segment_end_column)
				{
					auto& glyph = line[charIndex];
					auto color = GetGlyphColor(glyph);
					ImVec2 targetGlyphPos = { textScreenPos.x + TextDistanceToLineStart({lineNo, column}, false), lineStartScreenPos.y };

				if (glyph.mChar == '\t')
				{
						if (mShowWhitespaces)
						{
							ImVec2 p1, p2, p3, p4;

							const auto s = fontSize;
							const auto x1 = targetGlyphPos.x + mCharAdvance.x * 0.3f;
							const auto y = targetGlyphPos.y + fontHeight * 0.5f;

						if (mShortTabs)
						{
							const auto x2 = targetGlyphPos.x + mCharAdvance.x;
							p1 = ImVec2(x1, y);
							p2 = ImVec2(x2, y);
							p3 = ImVec2(x2 - s * 0.16f, y - s * 0.16f);
							p4 = ImVec2(x2 - s * 0.16f, y + s * 0.16f);
						}
						else
						{
							const auto x2 = targetGlyphPos.x + TabSizeAtColumn(column) * mCharAdvance.x - mCharAdvance.x * 0.3f;
							p1 = ImVec2(x1, y);
							p2 = ImVec2(x2, y);
							p3 = ImVec2(x2 - s * 0.2f, y - s * 0.2f);
							p4 = ImVec2(x2 - s * 0.2f, y + s * 0.2f);
						}

						drawList->AddLine(p1, p2, mPalette[(int)PaletteIndex::ControlCharacter]);
						drawList->AddLine(p2, p3, mPalette[(int)PaletteIndex::ControlCharacter]);
						drawList->AddLine(p2, p4, mPalette[(int)PaletteIndex::ControlCharacter]);
					}
				}
				else if (glyph.mChar == ' ')
				{
						if (mShowWhitespaces)
						{
							const auto s = fontSize;
							const auto x = targetGlyphPos.x + spaceSize * 0.5f;
							const auto y = targetGlyphPos.y + s * 0.5f;
							drawList->AddCircleFilled(ImVec2(x, y), 1.5f, mPalette[(int)PaletteIndex::ControlCharacter], 4);
					}
				}
				else
				{
					int seqLength = UTF8CharLength(glyph.mChar);
					if (mCursorOnBracket && seqLength == 1 && mMatchingBracketCoords == Coordinates{ lineNo, column })
					{
						ImVec2 topLeft = { targetGlyphPos.x, targetGlyphPos.y + fontHeight + 1.0f };
						ImVec2 bottomRight = { topLeft.x + mCharAdvance.x, topLeft.y + 1.0f };
						drawList->AddRectFilled(topLeft, bottomRight, mPalette[(int)PaletteIndex::Cursor]);
					}
						const int safe_seq_length = std::clamp(seqLength, 0, static_cast<int>(glyph_utf8.size()) - 1);
						for (int i = 0; i < safe_seq_length; ++i)
							glyph_utf8[static_cast<std::size_t>(i)] = line[charIndex + i].mChar;
						glyph_utf8[static_cast<std::size_t>(safe_seq_length)] = '\0';
						draw_text(targetGlyphPos, color, glyph_utf8.data());

					// Render style decorations for semantic tokens
					float glyphWidth = mCharAdvance.x * seqLength;

					// Strikethrough for deprecated symbols
					if (glyph.mStrikethrough)
					{
						float strikeY = targetGlyphPos.y + fontHeight * 0.5f;
						drawList->AddLine(
							ImVec2(targetGlyphPos.x, strikeY),
							ImVec2(targetGlyphPos.x + glyphWidth, strikeY),
							color, 1.0f);
					}

					// Underline for static symbols
					if (glyph.mUnderline)
					{
						float underlineY = targetGlyphPos.y + fontHeight - 1.0f;
						drawList->AddLine(
							ImVec2(targetGlyphPos.x, underlineY),
							ImVec2(targetGlyphPos.x + glyphWidth, underlineY),
							color, 1.0f);
					}
				}

				MoveCharIndexAndColumn(lineNo, charIndex, column);
			}

				if (!mUnderlines.empty())
				{
					const int lineMaxColumn = line_segment_end_column;
					const float underlineY = lineStartScreenPos.y + fontSize - 1.0f;
					for (const auto& underline : mUnderlines)
					{
						Coordinates startCoord = SanitizeCoordinates({ underline.mStartLine, underline.mStartColumn });
						Coordinates endCoord = SanitizeCoordinates({ underline.mEndLine, underline.mEndColumn });
					if (endCoord < startCoord)
					{
						std::swap(startCoord, endCoord);
					}
					if (lineNo < startCoord.mLine || lineNo > endCoord.mLine)
					{
						continue;
					}

					int startColumn = (lineNo == startCoord.mLine) ? startCoord.mColumn : line_segment_start_column;
					int endColumn = (lineNo == endCoord.mLine) ? endCoord.mColumn : lineMaxColumn;
					startColumn = Max(line_segment_start_column, Min(startColumn, lineMaxColumn));
					endColumn = Max(line_segment_start_column, Min(endColumn, lineMaxColumn));
					if (endColumn <= startColumn)
					{
						continue;
					}

					float startX = TextDistanceToLineStart({ lineNo, startColumn }, false);
					float endX = TextDistanceToLineStart({ lineNo, endColumn }, false);
						ImU32 underlineColor = underline.mColor != 0 ? underline.mColor : mPalette[(int)PaletteIndex::ErrorMarker];
						drawUnderline(
							textScreenPos.x + startX,
							textScreenPos.x + endX,
							underlineY,
							underlineColor,
							underline.mStyle,
						underline.mSeverity);
				}
			}
		}
	}
		const int maxColumns = mWordWrapEnabled ? Max(mWrapColumn, maxGhostColumn) : Max(maxColumnLimited, maxGhostColumn);
		const int space_line_count = Max(visual_line_count, 1);
		mCurrentSpaceHeight = (space_line_count + Min(mVisibleLineCount - 1, space_line_count)) * mCharAdvance.y;
		if (mWordWrapEnabled)
		{
			mCurrentSpaceWidth = Max(mContentWidth, Max(1, maxColumns) * mCharAdvance.x);
		}
		else
		{
			mCurrentSpaceWidth = Max((maxColumns + Min(mVisibleColumnCount - 1, maxColumns)) * mCharAdvance.x, mCurrentSpaceWidth);
		}

	ImGui::SetCursorPos(ImVec2(0, 0));
	ImGui::Dummy(ImVec2(mCurrentSpaceWidth, mCurrentSpaceHeight));

	if (mEnsureCursorVisible > -1)
	{
		for (int i = 0; i < (mEnsureCursorVisibleStartToo ? 2 : 1); i++) // first pass for interactive end and second pass for interactive start
		{
				if (i) UpdateViewVariables(mScrollX, mScrollY); // second pass depends on changes made in first pass
				Coordinates targetCoords = GetSanitizedCursorCoordinates(mEnsureCursorVisible, i); // cursor selection end or start
				const int targetVisualLine = GetVisualLineForCoordinates(targetCoords);
			if (targetVisualLine <= mFirstVisibleLine)
			{
				float targetScroll = std::max(0.0f, (targetVisualLine - 0.5f) * mCharAdvance.y);
				if (targetScroll < mScrollY)
					ImGui::SetScrollY(targetScroll);
			}
			if (targetVisualLine >= mLastVisibleLine)
			{
				float targetScroll = std::max(0.0f, (targetVisualLine + 1.5f) * mCharAdvance.y - mContentHeight);
				if (targetScroll > mScrollY)
					ImGui::SetScrollY(targetScroll);
			}
				if (!mWordWrapEnabled)
				{
					if (targetCoords.mColumn <= mFirstVisibleColumn)
					{
						float targetScroll = std::max(0.0f, mTextStart + (targetCoords.mColumn - 0.5f) * mCharAdvance.x);
						if (targetScroll < mScrollX)
							ImGui::SetScrollX(mScrollX = targetScroll);
					}
					if (targetCoords.mColumn >= mLastVisibleColumn)
					{
						float targetScroll = std::max(0.0f, mTextStart + (targetCoords.mColumn + 0.5f) * mCharAdvance.x - mContentWidth);
						if (targetScroll > mScrollX)
							ImGui::SetScrollX(mScrollX = targetScroll);
					}
				}
		}
		mEnsureCursorVisible = -1;
	}
	if (mScrollToTop)
	{
		ImGui::SetScrollY(0.0f);
		mScrollToTop = false;
	}
	if (mSetViewAtLine > -1)
	{
		const int targetVisualLine = GetVisualLineForDocumentLine(mSetViewAtLine);
		float targetScroll;
		switch (mSetViewAtLineMode)
		{
		default:
		case SetViewAtLineMode::FirstVisibleLine:
			targetScroll = std::max(0.0f, (float)targetVisualLine * mCharAdvance.y);
			break;
		case SetViewAtLineMode::LastVisibleLine:
			targetScroll = std::max(0.0f, (float)(targetVisualLine - (mLastVisibleLine - mFirstVisibleLine)) * mCharAdvance.y);
			break;
		case SetViewAtLineMode::Centered:
			targetScroll = std::max(0.0f, ((float)targetVisualLine - (float)(mLastVisibleLine - mFirstVisibleLine) * 0.5f) * mCharAdvance.y);
			break;
		}
		ImGui::SetScrollY(targetScroll);
		mSetViewAtLine = -1;
	}
}

void TextEditor::OnCursorPositionChanged()
{
	if (mState.mCurrentCursor == 0 && !mState.mCursors[0].HasSelection()) // only one cursor without selection
		mCursorOnBracket = FindMatchingBracket(mState.mCursors[0].mInteractiveEnd.mLine,
			GetCharacterIndexR(mState.mCursors[0].mInteractiveEnd), mMatchingBracketCoords);
	else
		mCursorOnBracket = false;

	if (!mDraggingSelection)
	{
		mState.SortCursorsFromTopToBottom();
		MergeCursorsIfPossible();
	}
}

void TextEditor::OnLineChanged(bool aBeforeChange, int aLine, int aColumn, int aCharCount, bool aDeleted) // adjusts cursor position when other cursor writes/deletes in the same line
{
	if (aBeforeChange)
	{
		m_line_change_cursor_char_indices.clear();
		m_line_change_cursor_char_indices.reserve(static_cast<std::size_t>(mState.mCurrentCursor + 1));
		for (int c = 0; c <= mState.mCurrentCursor; c++)
		{
			if (mState.mCursors[c].mInteractiveEnd.mLine == aLine && // cursor is at the line
				mState.mCursors[c].mInteractiveEnd.mColumn > aColumn && // cursor is to the right of changing part
				mState.mCursors[c].GetSelectionEnd() == mState.mCursors[c].GetSelectionStart()) // cursor does not have a selection
			{
				int char_index = GetCharacterIndexR({ aLine, mState.mCursors[c].mInteractiveEnd.mColumn });
				char_index += aDeleted ? -aCharCount : aCharCount;
				m_line_change_cursor_char_indices.emplace_back(c, char_index);
			}
		}
	}
	else
	{
		for (const auto& item : m_line_change_cursor_char_indices)
		{
			SetCursorPosition({ aLine, GetCharacterColumn(aLine, item.second) }, item.first);
		}
	}
}

void TextEditor::MergeCursorsIfPossible()
{
	// requires the cursors to be sorted from top to bottom
	std::unordered_set<int> cursorsToDelete;
	if (AnyCursorHasSelection())
	{
		// merge cursors if they overlap
		for (int c = mState.mCurrentCursor; c > 0; c--)// iterate backwards through pairs
		{
			int pc = c - 1; // pc for previous cursor

			bool pcContainsC = mState.mCursors[pc].GetSelectionEnd() >= mState.mCursors[c].GetSelectionEnd();
			bool pcContainsStartOfC = mState.mCursors[pc].GetSelectionEnd() > mState.mCursors[c].GetSelectionStart();

			if (pcContainsC)
			{
				cursorsToDelete.insert(c);
			}
			else if (pcContainsStartOfC)
			{
				Coordinates pcStart = mState.mCursors[pc].GetSelectionStart();
				Coordinates cEnd = mState.mCursors[c].GetSelectionEnd();
				mState.mCursors[pc].mInteractiveEnd = cEnd;
				mState.mCursors[pc].mInteractiveStart = pcStart;
				cursorsToDelete.insert(c);
			}
		}
	}
	else
	{
		// merge cursors if they are at the same position
		for (int c = mState.mCurrentCursor; c > 0; c--)// iterate backwards through pairs
		{
			int pc = c - 1;
			if (mState.mCursors[pc].mInteractiveEnd == mState.mCursors[c].mInteractiveEnd)
				cursorsToDelete.insert(c);
		}
	}
	for (int c = mState.mCurrentCursor; c > -1; c--)// iterate backwards through each of them
	{
		if (cursorsToDelete.find(c) != cursorsToDelete.end())
			mState.mCursors.erase(mState.mCursors.begin() + c);
	}
	mState.mCurrentCursor -= static_cast<int>(cursorsToDelete.size());
}

void TextEditor::AddUndo(UndoRecord& aValue)
{
	assert(!mReadOnly);
	mUndoBuffer.resize((size_t)(mUndoIndex + 1));
	mUndoBuffer.back() = aValue;
	++mUndoIndex;
}

// TODO
// - multiline comments vs single-line: latter is blocking start of a ML
void TextEditor::Colorize(int aFromLine, int aLines)
{
	int toLine = aLines == -1 ? (int)mLines.size() : std::min((int)mLines.size(), aFromLine + aLines);
	mColorRangeMin = std::min(mColorRangeMin, aFromLine);
	mColorRangeMax = std::max(mColorRangeMax, toLine);
	mColorRangeMin = std::max(0, mColorRangeMin);
	mColorRangeMax = std::max(mColorRangeMin, mColorRangeMax);
	mCheckComments = true;
}

void TextEditor::ColorizeRange(int aFromLine, int aToLine)
{
	if (mLines.empty() || aFromLine >= aToLine || mLanguageDefinition == nullptr)
		return;

	std::string buffer;
	boost::cmatch results;
	std::string id;

	int endLine = std::max(0, std::min((int)mLines.size(), aToLine));
	for (int i = aFromLine; i < endLine; ++i)
	{
		auto& line = mLines[i];

		if (line.empty())
			continue;

		buffer.resize(line.size());
		for (size_t j = 0; j < line.size(); ++j)
		{
			auto& col = line[j];
			buffer[j] = col.mChar;
			col.mColorIndex = PaletteIndex::Default;
		}

		const char* bufferBegin = &buffer.front();
		const char* bufferEnd = bufferBegin + buffer.size();

		auto last = bufferEnd;

		for (auto first = bufferBegin; first != last; )
		{
			const char* token_begin = nullptr;
			const char* token_end = nullptr;
			PaletteIndex token_color = PaletteIndex::Default;

			bool hasTokenizeResult = false;

			if (mLanguageDefinition->mTokenize != nullptr)
			{
				if (mLanguageDefinition->mTokenize(first, last, token_begin, token_end, token_color))
					hasTokenizeResult = true;
			}

			if (hasTokenizeResult == false)
			{
				// todo : remove
				//printf("using regex for %.*s\n", first + 10 < last ? 10 : int(last - first), first);

				for (const auto& p : mRegexList->mValue)
				{
					bool regexSearchResult = false;
					try { regexSearchResult = boost::regex_search(first, last, results, p.first, boost::regex_constants::match_continuous); }
					catch (...) {}
					if (regexSearchResult)
					{
						hasTokenizeResult = true;

						auto& v = *results.begin();
						token_begin = v.first;
						token_end = v.second;
						token_color = p.second;
						break;
					}
				}
			}

			if (hasTokenizeResult == false)
			{
				first++;
			}
			else
			{
				const size_t token_length = token_end - token_begin;

				if (token_color == PaletteIndex::Identifier)
				{
					id.assign(token_begin, token_end);

					// todo : allmost all language definitions use lower case to specify keywords, so shouldn't this use ::tolower ?
					if (!mLanguageDefinition->mCaseSensitive)
						std::transform(id.begin(), id.end(), id.begin(), ::toupper);

					if (!line[first - bufferBegin].mPreprocessor)
					{
						if (mLanguageDefinition->mKeywords.count(id) != 0)
							token_color = PaletteIndex::Keyword;
						else if (mLanguageDefinition->mIdentifiers.count(id) != 0)
							token_color = PaletteIndex::KnownIdentifier;
						else if (mLanguageDefinition->mPreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
					}
					else
					{
						if (mLanguageDefinition->mPreprocIdentifiers.count(id) != 0)
							token_color = PaletteIndex::PreprocIdentifier;
					}
				}

				for (size_t j = 0; j < token_length; ++j)
					line[(token_begin - bufferBegin) + j].mColorIndex = token_color;

				first = token_end;
			}
		}
	}
}

template<class InputIt1, class InputIt2, class BinaryPredicate>
bool ColorizerEquals(InputIt1 first1, InputIt1 last1,
	InputIt2 first2, InputIt2 last2, BinaryPredicate p)
{
	for (; first1 != last1 && first2 != last2; ++first1, ++first2)
	{
		if (!p(*first1, *first2))
			return false;
	}
	return first1 == last1 && first2 == last2;
}
void TextEditor::ColorizeInternal()
{
	if (mLines.empty() || mLanguageDefinition == nullptr)
		return;

	if (mCheckComments)
	{
		int endLine = static_cast<int>(mLines.size());
		int endIndex = 0;
		int commentStartLine = endLine;
		int commentStartIndex = endIndex;
		auto withinString = false;
		auto withinSingleLineComment = false;
		auto withinPreproc = false;
		auto firstChar = true;			// there is no other non-whitespace characters in the line before
		auto concatenate = false;		// '\' on the very end of the line
		int currentLine = 0;
		int currentIndex = 0;
		while (currentLine < endLine || currentIndex < endIndex)
		{
			auto& line = mLines[currentLine];

			if (currentIndex == 0 && !concatenate)
			{
				withinSingleLineComment = false;
				withinPreproc = false;
				firstChar = true;
			}

			concatenate = false;

			if (!line.empty())
			{
				auto& g = line[currentIndex];
				auto c = g.mChar;

				if (c != mLanguageDefinition->mPreprocChar &&
					isspace(static_cast<unsigned char>(c)) == 0)
					firstChar = false;

				if (currentIndex == (int)line.size() - 1 && line[line.size() - 1].mChar == '\\')
					concatenate = true;

				bool inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

				if (withinString)
				{
					line[currentIndex].mMultiLineComment = inComment;

					if (c == '\"')
					{
						if (currentIndex + 1 < (int)line.size() && line[currentIndex + 1].mChar == '\"')
						{
							currentIndex += 1;
							if (currentIndex < (int)line.size())
								line[currentIndex].mMultiLineComment = inComment;
						}
						else
							withinString = false;
					}
					else if (c == '\\')
					{
						currentIndex += 1;
						if (currentIndex < (int)line.size())
							line[currentIndex].mMultiLineComment = inComment;
					}
				}
				else
				{
					if (firstChar && c == mLanguageDefinition->mPreprocChar)
						withinPreproc = true;

					if (c == '\"')
					{
						withinString = true;
						line[currentIndex].mMultiLineComment = inComment;
					}
					else
					{
						auto pred = [](const char& a, const Glyph& b) { return a == b.mChar; };
						auto from = line.begin() + currentIndex;
						auto& startStr = mLanguageDefinition->mCommentStart;
						auto& singleStartStr = mLanguageDefinition->mSingleLineComment;

						if (!withinSingleLineComment && currentIndex + startStr.size() <= line.size() &&
							ColorizerEquals(startStr.begin(), startStr.end(), from, from + startStr.size(), pred))
						{
							commentStartLine = currentLine;
							commentStartIndex = currentIndex;
						}
						else if (singleStartStr.size() > 0 &&
							currentIndex + singleStartStr.size() <= line.size() &&
							ColorizerEquals(singleStartStr.begin(), singleStartStr.end(), from, from + singleStartStr.size(), pred))
						{
							withinSingleLineComment = true;
						}

						inComment = (commentStartLine < currentLine || (commentStartLine == currentLine && commentStartIndex <= currentIndex));

						line[currentIndex].mMultiLineComment = inComment;
						line[currentIndex].mComment = withinSingleLineComment;

						auto& endStr = mLanguageDefinition->mCommentEnd;
						if (currentIndex + 1 >= (int)endStr.size() &&
							ColorizerEquals(endStr.begin(), endStr.end(), from + 1 - endStr.size(), from + 1, pred))
						{
							commentStartIndex = endIndex;
							commentStartLine = endLine;
						}
					}
				}
				if (currentIndex < (int)line.size())
					line[currentIndex].mPreprocessor = withinPreproc;
				currentIndex += UTF8CharLength(c);
				if (currentIndex >= (int)line.size())
				{
					currentIndex = 0;
					++currentLine;
				}
			}
			else
			{
				currentIndex = 0;
				++currentLine;
			}
		}
		mCheckComments = false;
	}

	if (mColorRangeMin < mColorRangeMax)
	{
		const int increment = (mLanguageDefinition->mTokenize == nullptr) ? 10 : 10000;
		const int to = std::min(mColorRangeMin + increment, mColorRangeMax);
		ColorizeRange(mColorRangeMin, to);
		mColorRangeMin = to;

		// Re-apply semantic tokens after colorization overwrites them
		if (!mSemanticTokens.empty())
		{
			ReapplySemanticTokens();
		}

		if (mColorRangeMax == mColorRangeMin)
		{
			mColorRangeMin = std::numeric_limits<int>::max();
			mColorRangeMax = 0;
		}
		return;
	}
}

const TextEditor::Palette& TextEditor::GetDarkPalette()
{
	// Refined dark palette - harmonized with vscode theme system
	// Colors designed for better contrast and reduced eye strain
	const static Palette p = { {
			0xe8eaefff,	// Default - warm white for readability
			0xe8a76aff,	// Keyword - warm orange (accent3)
			0xe5b455ff,	// Number - golden warning color
			0x6fcf8eff,	// String - fresh green (success)
			0x6fcf8eff, // Char literal - same as string
			0xa8adb8ff, // Punctuation - lighter grey for better visibility
			0xb794f6ff,	// Preprocessor - soft purple (accent2)
			0xe8eaefff, // Identifier - same as default
			0x5ac8bdff, // Known identifier - vibrant teal (accent)
			0xe5b455ff, // Preproc identifier - golden
			0x6b7280ff, // Comment (single line) - visible but muted
			0x6b7280ff, // Comment (multi line) - same as single
			0x0d0e10ff, // Background - rich dark base
			0xe8eaefff, // Cursor - matches text color
			0x5ac8bd40, // Selection - accent with 25% alpha
			0xe86b7380, // ErrorMarker - error color with alpha
			0x8b919e25, // ControlCharacter - subtle muted color
			0xe86b7340, // Breakpoint - error with alpha
			0x5d636fff, // Line number - subtle but readable
			0xe8eaef08, // Current line fill - very subtle highlight
			0xe8eaef04, // Current line fill (inactive) - even more subtle
			0x5ac8bd18, // Current line edge - accent tint
		} };
	return p;
}

const TextEditor::Palette& TextEditor::GetMarianaPalette()
{
	// Mariana palette - sublime text inspired, oceanic feel
	const static Palette p = { {
			0xf8f8f2ff,	// Default - bright white
			0xc792eaff,	// Keyword - soft purple
			0xf78c6cff,	// Number - coral orange
			0xaddb67ff,	// String - lime green
			0xaddb67ff, // Char literal - same as string
			0x89ddffff, // Punctuation - cyan
			0x82aaffff,	// Preprocessor - light blue
			0xf8f8f2ff, // Identifier - same as default
			0x80cbc4ff, // Known identifier - teal
			0xffcb6bff, // Preproc identifier - yellow
			0x637777ff, // Comment (single line) - muted teal
			0x637777ff, // Comment (multi line) - same
			0x263238ff, // Background - dark blue-grey
			0xf8f8f2ff, // Cursor - matches text
			0x54657060, // Selection - subtle highlight
			0xff5370a0, // ErrorMarker - red with alpha
			0x54657040, // ControlCharacter - subtle
			0xff537050, // Breakpoint - red with alpha
			0x546570c0, // Line number - muted
			0x54657020, // Current line fill
			0x54657010, // Current line fill (inactive)
			0x80cbc420, // Current line edge - teal tint
		} };
	return p;
}

const TextEditor::Palette& TextEditor::GetLightPalette()
{
	const static Palette p = { {
			0x404040ff,	// None
			0x060cffff,	// Keyword
			0x008000ff,	// Number
			0xa02020ff,	// String
			0x704030ff, // Char literal
			0x000000ff, // Punctuation
			0x606040ff,	// Preprocessor
			0x404040ff, // Identifier
			0x106060ff, // Known identifier
			0xa040c0ff, // Preproc identifier
			0x205020ff, // Comment (single line)
			0x205040ff, // Comment (multi line)
			0xffffffff, // Background
			0x000000ff, // Cursor
			0x00006040, // Selection
			0xff1000a0, // ErrorMarker
			0x90909090, // ControlCharacter
			0x0080f080, // Breakpoint
			0x005050ff, // Line number
			0x00000040, // Current line fill
			0x80808040, // Current line fill (inactive)
			0x00000040, // Current line edge
		} };
	return p;
}

const TextEditor::Palette& TextEditor::GetRetroBluePalette()
{
	const static Palette p = { {
			0xffff00ff,	// None
			0x00ffffff,	// Keyword
			0x00ff00ff,	// Number
			0x008080ff,	// String
			0x008080ff, // Char literal
			0xffffffff, // Punctuation
			0x008000ff,	// Preprocessor
			0xffff00ff, // Identifier
			0xffffffff, // Known identifier
			0xff00ffff, // Preproc identifier
			0x808080ff, // Comment (single line)
			0x404040ff, // Comment (multi line)
			0x000080ff, // Background
			0xff8000ff, // Cursor
			0x00ffff80, // Selection
			0xff0000a0, // ErrorMarker
			0x0080ff80, // Breakpoint
			0x008080ff, // Line number
			0x00000040, // Current line fill
			0x80808040, // Current line fill (inactive)
			0x00000040, // Current line edge
		} };
	return p;
}

TextEditor::PaletteId TextEditor::defaultPalette = TextEditor::PaletteId::Dark;
