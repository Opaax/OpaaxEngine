#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"
#include "Editor/Undo/ComponentUndoables.h"

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
    // InspectorPanel — the "Inspector": shows the selected entity's components through the
    //   registered drawers (it reads EditorSelection; the Hierarchy writes it).
    //   It knows no component type: each registered drawer checks whether it applies, so a game's
    //   component shows up by registering a drawer. "Add Component" lists ComponentRegistry's types
    //   (a type without a drawer can still be added). Redrawn every frame (no change events).
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
        /** Nothing to acquire. */
        void            Startup()               override {}

        /** Nothing the world render depends on. */
        void            OnPreRender()           override {}

        /**
         * Empty states are text: "Nothing selected." or "No drawable components." (an entity whose
         * components have no registered drawer).
         */
        void            DrawContents()          override;

        /** Nothing to release. */
        void            Shutdown()              override {}
        //~End IEditorPanel interface

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * The "Add Component" popup: every registered type the entity does not have yet.
         */
        void DrawAddComponent(Entity& InEntity);

        /**
         * The "Remove Component" popup: every type the entity has, except essential ones (Transform).
         * Also reaches components with no registered drawer.
         */
        void DrawRemoveComponent(Entity& InEntity);

        /**
         * The entity's name, as an editable field (EntityMeta has no drawer). Written through EntityOps.
         */
        void DrawNameField(Entity& InEntity);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /**
         * Whether a widget was active last Draw (a widget commits as it becomes inactive, so the next
         * frame counts as an edit too).
         */
        bool m_bWasItemActive = false;

        // The field edit's undo step, kept across frames: a drag from 100 to 150 is one step.
        EntityComponentsEdit m_Edit;

        // The name field's buffer, refreshed from the entity while not being typed into.
        char m_NameBuffer[128] = {};
    };
}
