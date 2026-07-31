#pragma once

#include "Core/String/OpaaxStringID.hpp"   // OpaaxStringID — stable interned panel identity

namespace Opaax
{
    class World;
}

namespace Opaax::Editor
{
    // =============================================================================
    // IEditorPanel — the common base for a dockable editor panel. (M2 grows the registry that
    //   owns a set of these; M1 has exactly one — the Viewport.) Four lifecycle hooks the editor
    //   host drives, in this per-frame order:
    //
    //     Startup()     once, after the editor UI is up  — acquire GPU/host resources.
    //     OnPreRender() every frame, BEFORE the world renders — apply state the world's render
    //                   depends on (e.g. a pending viewport-size change) so Render() reads it.
    //     Draw()        every frame, inside the ImGui pass — emit the panel's widgets.
    //     Shutdown()    once, before the engine/UI tears down — release resources (LC).
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
        virtual void Startup()     = 0;
        virtual void OnPreRender() = 0;
        virtual void Draw()        = 0;
        virtual void Shutdown()    = 0;

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

        /**
         * @return The panel's stable interned identity — the registry key, dock/layout id, and
         *   menu→panel routing handle (M2). O(1) integer compare; ToString() yields the display
         *   name, so id and name live in one place (matches the World/WorldManager convention).
         */
        virtual OpaaxStringID GetPanelID() const = 0;
    };
}
