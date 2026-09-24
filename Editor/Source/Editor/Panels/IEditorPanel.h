#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F — the window's first-use size
#include "Core/String/OpaaxStringID.hpp"

namespace Opaax
{
    class World;
}

namespace Opaax::Editor
{
    /**
     * How EditorPanels opens this panel's window, through IEditorGui::BeginPanelWindow. Defaults
     * suit an ordinary docked list; the Viewport is the only panel that needs the padding off.
     */
    struct PanelWindowStyle
    {
        Vector2F DefaultSize = { 320.f, 400.f };
        bool     bNoPadding  = false;
    };

    // =============================================================================
    // IEditorPanel — the CONTENTS of a dockable editor panel. The window itself — its label, its
    //   size, its visibility and the Begin/End pair — belongs to EditorPanels, so a panel emits
    //   widgets and nothing else. Four lifecycle hooks, in this per-frame order:
    //
    //     Startup()      once, after the editor UI is up  — acquire GPU/host resources.
    //     OnPreRender()  every frame, BEFORE the world renders, VISIBLE OR NOT — apply state the
    //                    world's render depends on, and clear anything DrawContents measures.
    //     DrawContents() every frame while visible, inside the panel's window.
    //     Shutdown()     once, before the engine/UI tears down — release resources (LC).
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
         * The active world was replaced — drop anything cached from the old one (M4 S5).
         *
         * PUSH, not poll: every panel already re-reads GetActiveWorld() each frame, so reading the
         * new world is not the problem; NOTICING the swap is. PIE makes it routine (Play activates
         * a clone, Stop activates the original back), and a panel that cached per-world state has
         * no other moment to invalidate it.
         *
         * Default no-op, so a panel that caches nothing — most of them — says nothing. Fired by
         * EditorService AFTER the selection has been retargeted, so a panel never reads a stale one.
         *
         * @param InOld The world being left. May be null.
         * @param InNew The world now active. May be null (its world was destroyed).
         */
        virtual void OnActiveWorldChanged(World* /*InOld*/, World* /*InNew*/) {}

        // =============================================================================
        // Getter
        // =============================================================================

        /** @return How the host should open this panel's window. Override only to differ. */
        virtual PanelWindowStyle GetWindowStyle() const { return {}; }
    };
    
#define OPAAX_EDITOR_PANEL_NAME(Name) static ::Opaax::OpaaxStringID PanelID() { return ::Opaax::OpaaxStringID(#Name); }
}
