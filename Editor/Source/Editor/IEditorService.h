#pragma once

#include "Application/Services/IAppService.h"

namespace Opaax::Editor
{
    // =============================================================================
    // IEditorService — the editor, exposed as an application service (Editor.md D1). Provided ONLY by
    //   EditorApplication::OnProvideServices, so it exists only in editor executables; the engine never
    //   knows it (D4). It is the editor's composition root (D3): it resolves its dependencies once,
    //   builds the EditorContext, and owns the editor's lifecycle. Resolved via GetAppService by editor
    //   code only — never by the engine or a runtime target.
    // =============================================================================
    class IEditorService : public IAppService
    {
    public:
        OPAAX_SERVICE_TYPE(IEditorService)

        // Build the EditorContext + editor state. Called by EditorApplication AFTER engine startup,
        // when the subsystems the context references exist. Separate from construction because the
        // service is provided during Bootstrap, before the engine runs.
        virtual void Initialize() = 0;

        //----- null object ----------------------------------------------------
        static IEditorService& Null();
    };
}
