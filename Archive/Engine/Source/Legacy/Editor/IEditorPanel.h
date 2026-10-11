#pragma once

#if OPAAX_WITH_EDITOR

#include "Core/EngineAPI.h"

namespace Opaax::Editor
{
    class EditorEventBus;

    /**
     * @class IEditorPanel
     * Base interface for all editor panels.
     * Startup/Shutdown manage GPU/resource lifecycle.
     * OnSubscribe is called by EditorSubsystem immediately after Startup — override
     *   to register handlers against the bus and store the returned SubscriptionTokens
     *   as members (RAII unsubscribe on panel destruction).
     * Draw() handles per-frame ImGui content — override only when the panel takes no external params.
     * Panels with parameterised draw calls (Hierarchy, Inspector) inherit for lifecycle only.
     */
    class OPAAX_API IEditorPanel
    {
        // =============================================================================
        // DTOR
        // =============================================================================
    public:
        virtual ~IEditorPanel() = default;

        // =============================================================================
        // Function
        // =============================================================================
    public:
        virtual void        Startup()                            {}
        virtual void        OnSubscribe(EditorEventBus& /*Bus*/) {}
        virtual void        Shutdown()                           {}
        virtual void        Draw()                               {}
        virtual const char* GetPanelName() const                 = 0;
    };

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
