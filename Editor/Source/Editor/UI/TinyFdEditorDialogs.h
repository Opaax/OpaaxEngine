#pragma once

#include "Editor/UI/IEditorDialogs.h"

namespace Opaax::Editor
{
    // =============================================================================
    // TinyFdEditorDialogs — IEditorDialogs over tinyfiledialogs (the OS's own dialogs). The only file
    //   that includes <tinyfiledialogs.h>. Every call blocks and runs its callback before returning,
    //   so callers may capture EditorContext&.
    // =============================================================================
    class TinyFdEditorDialogs final : public IEditorDialogs
    {
        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorDialogs interface
        void OpenFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen) override;
        void SaveFile(const FileDialogRequest& InRequest, FPathChosen InOnChosen) override;
        void Confirm(const OpaaxString& InTitle, const OpaaxString& InMessage,
                     FAnswered InOnAnswered) override;
        //~End IEditorDialogs interface
    };
}
