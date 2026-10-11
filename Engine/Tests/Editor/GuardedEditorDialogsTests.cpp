// Suite: GuardedEditorDialogs — the editor's dialogs, shown when attended, and kept closed while an
// automation request runs: a question answered No, a file picker cancelled, what was asked kept.
#include <doctest.h>

#include "Editor/UI/GuardedEditorDialogs.hpp"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    /** Stands for the OS dialogs: counts what is shown, picks a fixed file and answers Yes. */
    class FakeDialogs final : public IEditorDialogs
    {
    public:
        explicit FakeDialogs(Int32& OutShown) : m_Shown(OutShown) {}

        void OpenFile(const FileDialogRequest&, FPathChosen InOnChosen) override   { Pick(InOnChosen); }
        void SaveFile(const FileDialogRequest&, FPathChosen InOnChosen) override   { Pick(InOnChosen); }
        void PickFolder(const FileDialogRequest&, FPathChosen InOnChosen) override { Pick(InOnChosen); }

        void Confirm(const OpaaxString&, const OpaaxString&, FAnswered InOnAnswered) override
        {
            ++m_Shown;
            InOnAnswered(EDialogAnswer::Yes);
        }

    private:
        void Pick(const FPathChosen& InOnChosen)
        {
            ++m_Shown;
            InOnChosen(OpaaxString("C:/Picked.opaaxmap"));
        }

        Int32& m_Shown;
    };

    FileDialogRequest Picker(const char* InTitle)
    {
        FileDialogRequest lRequest;
        lRequest.Title = OpaaxString(InTitle);
        return lRequest;
    }
}

TEST_CASE("GuardedEditorDialogs: attended, the dialogs are shown and answer")
{
    Int32                lShown = 0;
    GuardedEditorDialogs lDialogs(MakeUnique<FakeDialogs>(lShown));

    OpaaxString   lPicked;
    EDialogAnswer lAnswer = EDialogAnswer::No;
    lDialogs.SaveFile(Picker("New Map"), [&lPicked](const OpaaxString& InPath) { lPicked = InPath; });
    lDialogs.Confirm(OpaaxString("Unsaved changes"), OpaaxString("Lose them?"),
                     [&lAnswer](const EDialogAnswer InAnswer) { lAnswer = InAnswer; });

    CHECK(lShown == 2);
    CHECK(lPicked == "C:/Picked.opaaxmap");
    CHECK(lAnswer == EDialogAnswer::Yes);
    CHECK(lDialogs.TakeUnanswered().empty());
}

TEST_CASE("GuardedEditorDialogs: unattended, nothing is shown, a question gets No, a picker picks nothing")
{
    Int32                lShown = 0;
    GuardedEditorDialogs lDialogs(MakeUnique<FakeDialogs>(lShown));
    lDialogs.SetUnattended(true);
    REQUIRE(lDialogs.IsUnattended());

    bool          bPicked = false;
    EDialogAnswer lAnswer = EDialogAnswer::Yes;
    lDialogs.OpenFile(Picker("Open Level"), [&bPicked](const OpaaxString&) { bPicked = true; });
    lDialogs.Confirm(OpaaxString("Unsaved changes"), OpaaxString("This level has unsaved changes.\nContinue?"),
                     [&lAnswer](const EDialogAnswer InAnswer) { lAnswer = InAnswer; });
    lDialogs.PickFolder(Picker("Export Game"), [&bPicked](const OpaaxString&) { bPicked = true; });

    CHECK(lShown == 0);
    CHECK_FALSE(bPicked);
    CHECK(lAnswer == EDialogAnswer::No);

    // What was asked, in order and on one line; then forgotten.
    const TDynArray<OpaaxString> lAsked = lDialogs.TakeUnanswered();
    REQUIRE(lAsked.size() == 3);
    CHECK(lAsked[0] == "Open Level");
    CHECK(lAsked[1] == "Unsaved changes: This level has unsaved changes. Continue?");
    CHECK(lAsked[2] == "Export Game");
    CHECK(lDialogs.TakeUnanswered().empty());

    lDialogs.SetUnattended(false);
    lDialogs.Confirm(OpaaxString("Quit"), OpaaxString("Quit?"), [&lAnswer](const EDialogAnswer InAnswer) { lAnswer = InAnswer; });
    CHECK(lShown == 1);
    CHECK(lAnswer == EDialogAnswer::Yes);
}
