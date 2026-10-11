#pragma once

#include "Core/OpaaxTypes.h"            // TFunction, TDynArray
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // IEditorDialogs — the editor's modal dialogs: pick a file, answer a question.
    //   The result arrives through a callback, not a return value. The native implementation blocks
    //   and calls it before returning; an in-editor (multi-frame) implementation would call it later,
    //   and would then have to handle the world having changed in between.
    // =============================================================================

    enum class EDialogAnswer : Uint8
    {
        Yes,
        No
    };

    /** Enum to string (for logs). */
    inline const char* ToString(const EDialogAnswer InAnswer) noexcept
    {
        switch (InAnswer)
        {
        case EDialogAnswer::Yes: return "Yes";
        case EDialogAnswer::No:  return "No";
        }

        return "No";
    }

    /** What a file picker is asking for. */
    struct FileDialogRequest
    {
        OpaaxString Title;

        /**
         * Absolute. A trailing separator opens that directory; a full filename also fills the name box.
         */
        OpaaxString DefaultPath;

        /**
         * Patterns, e.g. "*.opaaxmap".
         */
        TDynArray<OpaaxString> Filters;

        /** The filter's display name, e.g. "Opaax Map". */
        OpaaxString FilterDescription;
    };

    class IEditorDialogs
    {
        // =============================================================================
        // Dtor
        // =============================================================================
    public:
        virtual ~IEditorDialogs() = default;

        // =============================================================================
        // Types
        // =============================================================================
    public:
        /** Called with an absolute path. Not called when the user cancels. */
        using FPathChosen = TFunction<void(const OpaaxString& InAbsPath)>;

        /** Always called; the caller checks the answer. */
        using FAnswered = TFunction<void(EDialogAnswer InAnswer)>;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Picks an existing file. */
        virtual void OpenFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen) = 0;

        /** Picks a destination. Whether overwriting is warned about is up to the backend. */
        virtual void SaveFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen) = 0;

        /** Picks a folder (the request's filters are not used). */
        virtual void PickFolder(const FileDialogRequest& InRequest, FPathChosen InOnChosen) = 0;

        /** A yes/no question. */
        virtual void Confirm(const OpaaxString& InTitle, const OpaaxString& InMessage,
                             FAnswered InOnAnswered) = 0;
    };
}
