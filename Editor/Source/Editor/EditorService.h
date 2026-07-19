#pragma once

#include "Editor/IEditorService.h"
#include "Editor/EditorContext.h"
#include "Core/OpaaxTypes.h"   // UniquePtr

namespace Opaax::Editor
{
    // =============================================================================
    // EditorService — the concrete IEditorService and the editor's composition root (D3). At
    //   Initialize() it resolves its engine dependencies ONCE (the only place editor code touches the
    //   locator) and builds the EditorContext that every panel/drawer receives by ctor. Owns nothing
    //   the engine owns — the context is references. Provided last (OnProvideServices), so it tears
    //   down FIRST (reverse order), before the engine service it references.
    // =============================================================================
    class EditorService final : public IEditorService
    {
    public:
        EditorService()           = default;
        ~EditorService() override = default;

        EditorService(const EditorService&)            = delete;
        EditorService& operator=(const EditorService&) = delete;

        //~Begin IEditorService
        void Initialize() override;
        //~End IEditorService

        //~Begin IAppService
        void OnShutdown() override;   // release the context while the engine it references is still alive
        //~End IAppService

        // The injected context (valid after Initialize). Panels are constructed with this (S10+).
        EditorContext& GetContext() noexcept { return *m_Context; }

    private:
        UniquePtr<EditorContext> m_Context;   // built at Initialize (refs valid post engine startup)
    };
}
