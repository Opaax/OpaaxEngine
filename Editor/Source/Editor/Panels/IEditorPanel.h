#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    class World;
}

namespace Opaax::Editor
{
    /**
     * How EditorPanels opens this panel's window. The defaults suit a docked list.
     */
    struct PanelWindowStyle
    {
        Vector2F DefaultSize = { 320.f, 400.f };
        bool     bNoPadding  = false;
    };

    // =============================================================================
    // IEditorPanel — the contents of a dockable editor panel (the window itself belongs to
    //   EditorPanels). Per-frame order of the hooks:
    //
    //     Startup()      once, after the editor UI is up — acquire resources.
    //     OnPreRender()  every frame, before the world renders, visible or not — apply what the
    //                    world render depends on, and clear what DrawContents measures.
    //     DrawContents() every frame while visible, inside the panel's window.
    //     Shutdown()     once, before the engine/UI shuts down — release resources.
    // =============================================================================
    class IEditorPanel
    {
        // =============================================================================
        // Dtor
        // =============================================================================
    public:
        virtual ~IEditorPanel() = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        virtual void Startup()      = 0;
        virtual void OnPreRender()  = 0;
        virtual void DrawContents() = 0;
        virtual void Shutdown()     = 0;

        /**
         * The active world changed (Play/Stop): drop anything cached from the old one.
         * Called after the selection was retargeted.
         * @param InOld The previous world. May be null.
         * @param InNew The new active world. May be null.
         */
        virtual void OnActiveWorldChanged(World* /*InOld*/, World* /*InNew*/) {}

        // =============================================================================
        // Getter
        // =============================================================================

        /** @return How the host opens this panel's window. */
        virtual PanelWindowStyle GetWindowStyle() const { return {}; }
    };
    
#define OPAAX_EDITOR_PANEL_NAME(Name) static ::Opaax::OpaaxStringID PanelID() { return ::Opaax::OpaaxStringID(#Name); }
}
