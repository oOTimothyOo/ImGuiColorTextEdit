#include <TextEditor.h>

void TextEditor::UnitTests()
{
	SetText(" \t  \t   \t \t\n");
	// --- GetCharacterColumn --- //
	{
		// Returns column given line and character index in that line
		assert(GetCharacterColumn(0, 0) == 0);
		assert(GetCharacterColumn(0, 1) == 1);
		assert(GetCharacterColumn(0, 2) == 4);
		assert(GetCharacterColumn(0, 3) == 5);
		assert(GetCharacterColumn(0, 4) == 6);
		assert(GetCharacterColumn(0, 5) == 8);
		assert(GetCharacterColumn(0, 6) == 9);
		assert(GetCharacterColumn(0, 7) == 10);
		assert(GetCharacterColumn(0, 8) == 11);
		assert(GetCharacterColumn(0, 9) == 12);
		assert(GetCharacterColumn(0, 10) == 13);
		assert(GetCharacterColumn(0, 11) == 16);
		assert(GetCharacterColumn(0, 12) == 16); // out of range
		// empty line
		assert(GetCharacterColumn(1, 0) == 0);
		assert(GetCharacterColumn(1, 1) == 0); // out of range
		assert(GetCharacterColumn(1, 2) == 0); // out of range
		// nonexistent line
		assert(GetCharacterColumn(2, 0) == 0);
		assert(GetCharacterColumn(2, 1) == 0);
		assert(GetCharacterColumn(2, 2) == 0);
	}

	// --- GetCharacterIndexLeftSide --- //
	{
		// Returns character index from coordinates, if coordinates are in the middle of a tab character it returns character index of that tab character
		assert(GetCharacterIndexLeftSide({ 0, 0 }) == 0);
		assert(GetCharacterIndexLeftSide({ 0, 1 }) == 1);
		assert(GetCharacterIndexLeftSide({ 0, 2 }) == 1);
		assert(GetCharacterIndexLeftSide({ 0, 3 }) == 1);
		assert(GetCharacterIndexLeftSide({ 0, 4 }) == 2);
		assert(GetCharacterIndexLeftSide({ 0, 5 }) == 3);
		assert(GetCharacterIndexLeftSide({ 0, 6 }) == 4);
		assert(GetCharacterIndexLeftSide({ 0, 7 }) == 4);
		assert(GetCharacterIndexLeftSide({ 0, 8 }) == 5);
		assert(GetCharacterIndexLeftSide({ 0, 9 }) == 6);
		assert(GetCharacterIndexLeftSide({ 0, 10 }) == 7);
		assert(GetCharacterIndexLeftSide({ 0, 11 }) == 8);
		assert(GetCharacterIndexLeftSide({ 0, 12 }) == 9);
		assert(GetCharacterIndexLeftSide({ 0, 13 }) == 10);
		assert(GetCharacterIndexLeftSide({ 0, 14 }) == 10);
		assert(GetCharacterIndexLeftSide({ 0, 15 }) == 10);
		assert(GetCharacterIndexLeftSide({ 0, 16 }) == 11);
		assert(GetCharacterIndexLeftSide({ 0, 17 }) == 11); // out of range
		assert(GetCharacterIndexLeftSide({ 0, 18 }) == 11); // out of range
		// empty line
		assert(GetCharacterIndexLeftSide({ 1, 0 }) == 0);
		assert(GetCharacterIndexLeftSide({ 1, 1 }) == 0); // out of range
		assert(GetCharacterIndexLeftSide({ 1, 2 }) == 0); // out of range
		// nonexistent line
		assert(GetCharacterIndexLeftSide({ 2, 0 }) == -1);
		assert(GetCharacterIndexLeftSide({ 2, 1 }) == -1);
		assert(GetCharacterIndexLeftSide({ 2, 2 }) == -1);
	}

	// --- GetCharacterIndex --- //
	{
		// Returns character index from coordinates, if coordinates are in the middle of a tab character it returns character index of that tab character
		assert(GetCharacterIndex({ 0, 0 }) == 0);
		assert(GetCharacterIndex({ 0, 1 }) == 1);
		assert(GetCharacterIndex({ 0, 2 }) == 2);
		assert(GetCharacterIndex({ 0, 3 }) == 2);
		assert(GetCharacterIndex({ 0, 4 }) == 2);
		assert(GetCharacterIndex({ 0, 5 }) == 3);
		assert(GetCharacterIndex({ 0, 6 }) == 4);
		assert(GetCharacterIndex({ 0, 7 }) == 5);
		assert(GetCharacterIndex({ 0, 8 }) == 5);
		assert(GetCharacterIndex({ 0, 9 }) == 6);
		assert(GetCharacterIndex({ 0, 10 }) == 7);
		assert(GetCharacterIndex({ 0, 11 }) == 8);
		assert(GetCharacterIndex({ 0, 12 }) == 9);
		assert(GetCharacterIndex({ 0, 13 }) == 10);
		assert(GetCharacterIndex({ 0, 14 }) == 11);
		assert(GetCharacterIndex({ 0, 15 }) == 11);
		assert(GetCharacterIndex({ 0, 16 }) == 11);
		assert(GetCharacterIndex({ 0, 17 }) == 11); // out of range
		assert(GetCharacterIndex({ 0, 18 }) == 11); // out of range
		// empty line
		assert(GetCharacterIndex({ 1, 0 }) == 0);
		assert(GetCharacterIndex({ 1, 1 }) == 0); // out of range
		assert(GetCharacterIndex({ 1, 2 }) == 0); // out of range
		// nonexistent line
		assert(GetCharacterIndex({ 2, 0 }) == -1);
		assert(GetCharacterIndex({ 2, 1 }) == -1);
		assert(GetCharacterIndex({ 2, 2 }) == -1);
	}

	// --- Advance --- //
	{
		// Returns coordinates one character to the right, taking into account tab size
		Coordinates testCoords = { 0, 0 };
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 1);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 4);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 5);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 6);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 8);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 9);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 10);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 11);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 12);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 13);
		Advance(testCoords);
		assert(testCoords.mLine == 0 && testCoords.mColumn == 16);
		Advance(testCoords);
		assert(testCoords.mLine == 1 && testCoords.mColumn == 0);
		Advance(testCoords);
		assert(testCoords.mLine == 1 && testCoords.mColumn == 0);
	}

	// --- GetText --- //
	{
		// Gets text from aStart to aEnd, tabs are counted on the start position
		std::string text = GetText({ 0, 0 }, { 0, 1 });
		assert(text.compare(" ") == 0);
		text = GetText({ 0, 1 }, { 0, 2 });
		assert(text.compare("\t") == 0);
		text = GetText({ 0, 2 }, { 0, 3 });
		assert(text.compare("") == 0);
		text = GetText({ 0, 3 }, { 0, 4 });
		assert(text.compare("") == 0);
		text = GetText({ 0, 4 }, { 0, 5 });
		assert(text.compare(" ") == 0);
		text = GetText({ 0, 5 }, { 0, 6 });
		assert(text.compare(" ") == 0);
		text = GetText({ 0, 6 }, { 0, 7 });
		assert(text.compare("\t") == 0);
		text = GetText({ 0, 7 }, { 0, 8 });
		assert(text.compare("") == 0);

		text = GetText({ 0, 0 }, { 0, 8 });
		assert(text.compare(" \t  \t") == 0);
		text = GetText({ 0, 0 }, { 0, 7 });
		assert(text.compare(" \t  \t") == 0);
		text = GetText({ 0, 0 }, { 0, 6 });
		assert(text.compare(" \t  ") == 0);
		text = GetText({ 0, 4 }, { 0, 12 });
		assert(text.compare("  \t   \t") == 0);
		text = GetText({ 0, 4 }, { 0, 13 });
		assert(text.compare("  \t   \t ") == 0);
		text = GetText({ 0, 4 }, { 0, 14 });
		assert(text.compare("  \t   \t \t") == 0);
		text = GetText({ 0, 4 }, { 0, 15 });
		assert(text.compare("  \t   \t \t") == 0);
		text = GetText({ 0, 4 }, { 0, 16 });
		assert(text.compare("  \t   \t \t") == 0);

		text = GetText({ 0, 0 }, { 1, 0 });
		assert(text.compare(" \t  \t   \t \t\n") == 0);
	}

}
