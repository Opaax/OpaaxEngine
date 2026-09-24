#pragma once

#include "Editor/UI/IEditorDialogs.h"

namespace Opaax::Editor
{
    // =============================================================================
    // TinyFdEditorDialogs — IEditorDialogs over tinyfiledialogs, i.e. the OS's own modals.
    //
    //   THE ONLY FILE IN THE EDITOR THAT INCLUDES <tinyfiledialogs.h>. That is the property the
    //   seam exists for: the vendor was previously named from inside six command bodies.
    //
    //   Every call BLOCKS and fires its continuation INLINE, before returning. Callers may
    //   therefore capture EditorContext& — see IEditorDialogs for what a non-blocking
    //   implementation would owe instead.
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
