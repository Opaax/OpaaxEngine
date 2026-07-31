#pragma once

#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // InputPanel — what the engine currently believes about input, and whether it is being told
    //   anything at all (Editor.md D5, M-Input).
    //
    //   READ-ONLY, and that is the point: input is otherwise entirely silent, so "nothing happens
    //   when I press W" has at least four different causes — the route is closed for one of three
    //   reasons, or the key genuinely is not arriving. This panel distinguishes them, which is the
    //   difference between an instrument and a decoration (L15).
    //
    //   Reads InputManager through EditorContext::Engine and the route through
    //   EditorContext::Route — the same objects RouteInput gates on, never a second copy of the
    //   rule, so the readout cannot disagree with the behaviour it is describing.
    // =============================================================================
    class InputPanel final : public IEditorPanel
    {
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
        void Draw()        override;

        /** Nothing to release. */
        void Shutdown()    override {}

        OpaaxStringID GetPanelID() const override { return m_PanelID; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        const OpaaxStringID m_PanelID{ OPAAX_ID("Input") };
        const OpaaxString   m_Title = m_PanelID.ToString();

        // "Last pressed / released" have to be REMEMBERED: an edge is true for exactly one frame,
        // and a human cannot read a value that exists for 16 ms. Panel-local because they are a
        // display convenience, not engine state.
        OpaaxString m_LastPressed  = "—";
        OpaaxString m_LastReleased = "—";
    };
}
