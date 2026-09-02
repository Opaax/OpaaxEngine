#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Undo/ComponentUndoables.h"   // the step a field edit records (⑤)

namespace Opaax
{
    class Entity;

    namespace Editor
    {
        struct EditorContext;
    }
}

namespace Opaax::Editor
{

    // =============================================================================
    // InspectorPanel — the dockable "Inspector": the editor's READER of EditorSelection (Hierarchy is
    //   the writer), showing the selected entity's components through the registered drawers.
    //
    //   It knows nothing about any component type. It walks EditorContext::Extensions.Drawers() and
    //   invokes each entry, and each entry self-checks whether it applies to this entity — so a game
    //   module's component becomes inspectable purely by registering a drawer, with no editor change.
    //   That inversion is what makes component reflection unnecessary (see DrawerRegistry).
    //
    //   Two registries, two questions: Drawers() answers "how is this shown?", ComponentRegistry
    //   answers "what types exist?" — which is what "Add Component" needs, since a type with no
    //   drawer is still a type you can attach.
    //
    //   No change notification: ImGui is immediate-mode, so Draw() simply renders whatever the selection
    //   is right now. An event would be stored and then read here anyway (user decision, 2026-07-27).
    // =============================================================================
    class InspectorPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Inspector);
        
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit InspectorPanel(EditorContext& InContext);
        ~InspectorPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
    public:
        InspectorPanel(const InspectorPanel&)            = delete;
        InspectorPanel& operator=(const InspectorPanel&) = delete;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** No resource to acquire — the panel reads the selection and the routes through the context. */
        void            Startup()               override {}

        /** Nothing the world's render depends on. */
        void            OnPreRender()           override {}

        /**
         * Empty states are explicit text, never a blank panel (L12), and they are distinct on purpose:
         * "Nothing selected." (no entity) vs "No drawable components." (an entity, but no registered
         * drawer applied to it) — the second is why FDrawerInvoke reports whether it drew.
         */
        void            DrawContents()          override;

        /** No resource to release. */
        void            Shutdown()              override {}
        //~End IEditorPanel interface

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * The "Add Component" popup — every registered type the entity does NOT already carry.
         *
         * This is the second consumer ComponentRegistry was built for (its own header names it), and
         * it stays type-blind for the same reason Draw() does: the registry answers in terms of
         * IComponentEntry, so a game module's component appears here by being registered, full stop.
         */
        void DrawAddComponent(Entity& InEntity);

        /**
         * The "Remove Component" popup — every type the entity carries that is not ESSENTIAL.
         *
         * A popup beside Add rather than a control on each component's header: a drawer draws its
         * own header, and TDrawerRegistry is generic over subjects (a config cannot be removed), so
         * putting it there would push a component-only concern into shared machinery. It also
         * reaches a component whose drawer is NOT registered — present in the map, invisible in the
         * panel, and otherwise impossible to get rid of.
         *
         * TransformComponent never appears: IComponentEntry::IsEssential, and Remove refuses it
         * anyway, so the guarantee does not depend on this menu remembering.
         */
        void DrawRemoveComponent(Entity& InEntity);

        /**
         * The entity's name as an editable field. EntityMeta is identity rather than user data (I8)
         * so it has no drawer and no properties — this panel has always drawn it by hand, and now
         * writes it back through EntityOps like every other mutation.
         */
        void DrawNameField(Entity& InEntity);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /**
         * Was an ImGui widget active on the PREVIOUS Draw. A widget commits its value as it goes
         * inactive — ImGui has cleared ActiveId by the time this panel asks — so the frame after
         * counts as an edit too. See Draw() for why this is asked of ImGui rather than of the drawer.
         */
        bool m_bWasItemActive = false;

        // ⑤ — the field edit's undo step, held ACROSS FRAMES: opened with the component values as
        // they were when the gesture began, closed with them as they are when it ends. That is what
        // makes a drag from 100 to 150 ONE entry rather than one per frame.
        EntityComponentsEdit m_Edit;

        // The name field's edit buffer. Refreshed from the entity whenever the field is NOT being
        // typed into, so it follows the selection without fighting the keystrokes.
        char m_NameBuffer[128] = {};
    };
}
