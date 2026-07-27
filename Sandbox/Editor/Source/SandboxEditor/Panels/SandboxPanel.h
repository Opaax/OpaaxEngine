#pragma once

#include "Core/OpaaxTypes.h"                // Uint64
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax::Editor
{
    struct EditorContext;
}

// =============================================================================
// SandboxPanel — the Sandbox GAME's own editor panel (Editor.md D10 dogfood). Compiled only into
//   SandboxEditor.exe, and registered by SandboxEditorModule through the SAME PanelRegistry route the
//   editor's native Hierarchy travels. Adding it took zero changes to OpaaxEditorLib — which is exactly
//   the M2a milestone gate, and the reason this file lives here rather than in the editor.
//
//   Content is deliberately thin: the live entity count plus a "Spawn Quad" button. That is enough to
//   prove a game panel can both READ and MUTATE the world through EditorContext alone, never the
//   locator (D3) and never an engine-side hook written on its behalf.
// =============================================================================
class SandboxPanel final : public Opaax::Editor::IEditorPanel
{
    // =============================================================================
    // Ctor - Dtor
    // =============================================================================
public:
    explicit SandboxPanel(Opaax::Editor::EditorContext& InContext);
    ~SandboxPanel() override;

    // =============================================================================
    // Copy delete
    // =============================================================================
public:
    SandboxPanel(const SandboxPanel&)            = delete;
    SandboxPanel& operator=(const SandboxPanel&) = delete;

    // =============================================================================
    // Functions
    // =============================================================================
private:
    /**
     * Creates one DummyComponent quad in the active world, laid out so successive spawns tile instead of
     * stacking on one spot. Safe to call from Draw(): the ImGui pass runs AFTER Engine().Loop(), so no
     * render or Hierarchy iteration is in flight — the new entity simply appears next frame.
     */
    void SpawnQuad();

    // =============================================================================
    // Override
    // =============================================================================
public:
    //~Begin IEditorPanel interface
    /** No resource to acquire — the panel reaches the world through the context. */
    void                    Startup()               override {}

    /** Nothing the world's render depends on. */
    void                    OnPreRender()           override {}

    /** Entity count + the Spawn Quad button; explicit text when there is no world (never a blank panel). */
    void                    Draw()                  override;

    /** No resource to release. */
    void                    Shutdown()              override {}

    Opaax::OpaaxStringID    GetPanelID()    const   override { return m_PanelID; }
    //~End IEditorPanel interface

    // =============================================================================
    // Members
    // =============================================================================
private:
    Opaax::Editor::EditorContext& m_Context;

    const Opaax::OpaaxStringID m_PanelID{ OPAAX_ID("Sandbox Panel") };
    const Opaax::OpaaxString   m_Title = m_PanelID.ToString();

    Opaax::Uint64 m_SpawnCount = 0;   // drives both the spawn layout and the log's "#N"
};
