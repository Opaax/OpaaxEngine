#pragma once

#include "Core/Maths/MathTypes.h"   // Vector2F — the held scroll value
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // InputPanel — five lines: who has the input, what is held, where the mouse is, how far it
    //   moved, and the last wheel notch (Editor.md D5, M-Input).
    //
    //   READ-ONLY, and deliberately small. Input is otherwise entirely silent, so "nothing happens
    //   when I press W" has several causes — and the FIRST line answers it, because "the editor
    //   has the input" versus "the key is not arriving" are different problems. Everything beyond
    //   those five lines was noise that made the panel harder to read, not easier.
    //
    //   Reads InputManager through EditorContext::Engine and the route through
    //   EditorContext::Route — the same objects RouteInput gates on, never a second copy of the
    //   rule, so the readout cannot disagree with the behaviour it is describing.
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
        /** Nothing to acquire — everything is read through the context. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        /** Route state, held keys, last press/release, mouse position + delta, scroll. */
        void DrawContents() override;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * The running game's ACTIONS: name, shape, live value and which phases fired.
         *
         * Below the raw keys on purpose — the two halves of the input chain in the order they
         * run, so a key that is down while its action reads zero is a visible contradiction
         * rather than something to go looking for (IM1).
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

        // A wheel notch is one frame of non-zero — about 16 ms, which the eye cannot catch. The
        // last value is held on screen for a moment. DISPLAY ONLY: the engine's scroll is
        // per-frame and untouched, so nothing downstream inherits this smoothing.
        static constexpr float SCROLL_HOLD_SECONDS = 0.6f;

        Vector2F m_HeldScroll{0.f, 0.f};
        float    m_ScrollHoldLeft = 0.f;
    };
}
