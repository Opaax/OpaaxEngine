#pragma once

#include "Core/OpaaxTypes.h"            // TFunction, TDynArray
#include "Core/String/OpaaxString.hpp"

namespace Opaax::Editor
{
    // =============================================================================
    // IEditorDialogs — the editor's MODAL seam: pick a file, answer a question.
    //
    //   The second UI backend the editor names, and the one MR2d did not cover: file pickers and
    //   message boxes were reached straight from command bodies, which is exactly the shape that
    //   milestone removed for ImGui. It is a seam of its own rather than a section of IEditorGui
    //   for the reason EditorContext already gives for keeping UIBackend beside Gui — it is the
    //   NARROWER dependency, and a command that asks where to save a file has no business with the
    //   menu bar.
    //
    //   THE RESULT ARRIVES BY CONTINUATION, NOT BY RETURN, AND THAT IS THE WHOLE POINT OF THE
    //   SHAPE. The native implementation blocks and calls it INLINE before returning, so a caller
    //   may safely capture EditorContext& and act at once. An in-editor implementation — an ImGui
    //   modal, which is inherently multi-frame — would call it frames later instead, and a
    //   returned value could never have expressed that. Writing the call sites this way is what
    //   makes a second implementation a class swap rather than a rewrite of every one of them.
    //
    //   The constraint that shape imposes on any SECOND implementation: by the time a deferred
    //   continuation runs, the world it captured may be gone. A native one cannot hit that; an
    //   in-editor one must answer it (re-resolve from the context, or refuse to fire on a world
    //   that changed).
    //
    //   No OPAAX_API: OpaaxEditorLib is a static lib archived into the editor exe, not a DLL.
    // =============================================================================

    enum class EDialogAnswer : Uint8
    {
        Yes,
        No
    };

    /**
     * I11's rule for an enum: a free ToString beside it.
     *
     * Two values because the one question asked today is a yes/no. A three-button
     * Save/Don't Save/Cancel is the obvious next one and costs an enumerator plus tinyfd's
     * "yesnocancel" — named, not built, because nothing asks it yet.
     */
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
         * ABSOLUTE. A trailing separator means "open in this directory"; a full filename also
         * pre-fills the name box.
         */
        OpaaxString DefaultPath;

        /**
         * Patterns, e.g. "*.opaaxmap". PLURAL from the start: every caller today passes one, but
         * an image field wants "*.png", "*.jpg", "*.tga" and a singular field would have to grow.
         */
        TDynArray<OpaaxString> Filters;

        /** What the filter row is called — "Opaax Map". */
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
        /** Called with an ABSOLUTE path. NOT called when the user cancels — cancel is not an event. */
        using FPathChosen = TFunction<void(const OpaaxString& InAbsPath)>;

        /** Always called: the caller branches on the answer rather than on being called. */
        using FAnswered = TFunction<void(EDialogAnswer InAnswer)>;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /** Pick an EXISTING file. */
        virtual void OpenFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen) = 0;

        /** Pick a destination. Whether an existing file is warned about is the backend's business. */
        virtual void SaveFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen) = 0;

        /** A yes/no question. */
        virtual void Confirm(const OpaaxString& InTitle, const OpaaxString& InMessage,
                             FAnswered InOnAnswered) = 0;
    };
}
