#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // InputPanel — who has the input, what is held, where the mouse is, how far it moved, and the
    //   last scroll. Read-only. Reads the same objects RouteInput uses, so it cannot disagree with it.
    // =============================================================================
    class InputPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Input);
        
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit InputPanel(EditorContext& InContext);
        ~InputPanel() override;

        // =============================================================================
        // Copy delete
        // =============================================================================
        InputPanel(const InputPanel&)            = delete;
        InputPanel& operator=(const InputPanel&) = delete;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world render depends on. */
        void OnPreRender() override {}

        /** Route state, held keys, mouse position and delta, scroll. */
        void DrawContents() override;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * The running game's actions: name, type, value and phases. Below the raw keys, so a held key
         * whose action reads zero is visible.
         */
        void DrawActions();

        /** Nothing to release. */
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 360.f, 220.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        // The last scroll value, held on screen briefly. Display only.
        static constexpr float SCROLL_HOLD_SECONDS = 0.6f;

        Vector2F m_HeldScroll{0.f, 0.f};
        float    m_ScrollHoldLeft = 0.f;
    };
}
