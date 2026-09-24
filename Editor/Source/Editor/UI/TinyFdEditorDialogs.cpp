#include "Editor/UI/TinyFdEditorDialogs.h"

#include <tinyfiledialogs.h>

namespace
{
    using namespace Opaax;
    using namespace Opaax::Editor;

    /**
     * tinyfiledialogs takes its patterns as a (char const* const*) array, so the request's owned
     * strings are borrowed into one for the duration of the call.
     */
    TDynArray<const char*> BorrowFilters(const FileDialogRequest& InRequest)
    {
        TDynArray<const char*> lFilters;
        lFilters.reserve(InRequest.Filters.size());

        for (const OpaaxString& lPattern : InRequest.Filters)
        {
            lFilters.emplace_back(lPattern.CStr());
        }

        return lFilters;
    }
}

namespace Opaax::Editor
{
    void TinyFdEditorDialogs::OpenFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen)
    {
        const TDynArray<const char*> lFilters = BorrowFilters(InRequest);

        const char* const lPicked = tinyfd_openFileDialog(
            InRequest.Title.CStr(),
            InRequest.DefaultPath.CStr(),
            static_cast<int>(lFilters.size()),
            lFilters.empty() ? nullptr : lFilters.data(),
            InRequest.FilterDescription.CStr(),
            /*allowMultiple*/ 0);

        // Cancel runs nothing — it is not an event, and every call site used to spell that as a
        // bare `return` on a null.
        if (lPicked != nullptr && InOnChosen)
        {
            InOnChosen(OpaaxString(lPicked));
        }
    }

    void TinyFdEditorDialogs::SaveFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen)
    {
        const TDynArray<const char*> lFilters = BorrowFilters(InRequest);

        const char* const lPicked = tinyfd_saveFileDialog(
            InRequest.Title.CStr(),
            InRequest.DefaultPath.CStr(),
            static_cast<int>(lFilters.size()),
            lFilters.empty() ? nullptr : lFilters.data(),
            InRequest.FilterDescription.CStr());

        if (lPicked != nullptr && InOnChosen)
        {
            InOnChosen(OpaaxString(lPicked));
        }
    }

    void TinyFdEditorDialogs::Confirm(const OpaaxString& InTitle, const OpaaxString& InMessage,
                                      FAnswered InOnAnswered)
    {
        // defaultButton 0 = NO. The safe answer is the default for a question that only ever
        // guards something destructive.
        const int lAnswer = tinyfd_messageBox(InTitle.CStr(), InMessage.CStr(),
                                              "yesno", "warning", /*defaultButton*/ 0);

        if (InOnAnswered)
        {
            InOnAnswered(lAnswer == 1 ? EDialogAnswer::Yes : EDialogAnswer::No);
        }
    }
}
