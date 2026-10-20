// Suite: what the font family editor does to a family's faces — finding a cell's entry, adding a style
// once, and undoing an edit that would give a face a style another one has
// (Editor/Assets/Editors/FontFamilyEditing.h).
#include <doctest.h>

#include "Editor/Assets/Editors/FontFamilyEditing.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    FontStyleKey Style(const EFontSubset InSubset, const EFontWeight InWeight)
    {
        FontStyleKey lStyle;
        lStyle.Subset = InSubset;
        lStyle.Weight = InWeight;
        return lStyle;
    }
}

TEST_CASE("FamilyEditing: a cell finds its entry by its whole style")
{
    TDynArray<FontFamilyEntry> lEntries;
    REQUIRE(FamilyEditing::AddEntry(lEntries, Style(EFontSubset::Latin, EFontWeight::Regular)) == 0);
    REQUIRE(FamilyEditing::AddEntry(lEntries, Style(EFontSubset::Greek, EFontWeight::Regular)) == 1);

    CHECK(FamilyEditing::FindEntry(lEntries, Style(EFontSubset::Greek, EFontWeight::Regular)) == 1);
    CHECK(FamilyEditing::FindEntry(lEntries, Style(EFontSubset::Greek, EFontWeight::Bold)) == -1);

    // The same script and weight in italic is another cell.
    FontStyleKey lItalic = Style(EFontSubset::Latin, EFontWeight::Regular);
    lItalic.Slant = EFontSlant::Italic;
    CHECK(FamilyEditing::FindEntry(lEntries, lItalic) == -1);
}

TEST_CASE("FamilyEditing: a style is added once, with no face yet")
{
    TDynArray<FontFamilyEntry> lEntries;

    const Int32 lAdded = FamilyEditing::AddEntry(lEntries, Style(EFontSubset::Cyrillic, EFontWeight::Bold));
    REQUIRE(lAdded == 0);
    CHECK(lEntries[0].Face.IsEmpty());
    CHECK(lEntries[0].Style.Subset == EFontSubset::Cyrillic);

    // A second face for the same style could never be drawn.
    CHECK(FamilyEditing::AddEntry(lEntries, Style(EFontSubset::Cyrillic, EFontWeight::Bold)) == -1);
    CHECK(lEntries.size() == 1u);
}

TEST_CASE("FamilyEditing: an edit that takes another face's style is undone; a free style stays")
{
    TDynArray<FontFamilyEntry> lEntries;
    FamilyEditing::AddEntry(lEntries, Style(EFontSubset::Latin, EFontWeight::Regular));
    FamilyEditing::AddEntry(lEntries, Style(EFontSubset::Latin, EFontWeight::Bold));

    // The dropdown already wrote Regular into the Bold face.
    const FontStyleKey lBefore = lEntries[1].Style;
    lEntries[1].Style.Weight   = EFontWeight::Regular;

    CHECK(FamilyEditing::IsStyleTaken(lEntries, lEntries[1].Style, 1u));
    CHECK(FamilyEditing::RevertTakenStyle(lEntries, 1u, lBefore));
    CHECK(lEntries[1].Style.Weight == EFontWeight::Bold);

    // A weight nobody has is kept.
    lEntries[1].Style.Weight = EFontWeight::Black;
    CHECK_FALSE(FamilyEditing::RevertTakenStyle(lEntries, 1u, lBefore));
    CHECK(lEntries[1].Style.Weight == EFontWeight::Black);

    // A face never collides with itself, and no such entry is no change.
    CHECK_FALSE(FamilyEditing::IsStyleTaken(lEntries, lEntries[0].Style, 0u));
    CHECK_FALSE(FamilyEditing::RevertTakenStyle(lEntries, 7u, lBefore));
}
