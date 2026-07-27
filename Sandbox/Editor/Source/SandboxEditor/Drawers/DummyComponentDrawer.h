#pragma once

#include "World/Components/DummyComponent.h"

// =============================================================================
// DummyComponentDrawer — the Sandbox GAME's own component drawer (Editor.md D10 dogfood). Compiled only
//   into SandboxEditor.exe and registered by SandboxEditorModule via
//   Drawers().Register<DummyComponent, DummyComponentDrawer>(); adding it took zero changes to
//   OpaaxEditorLib, which together with SandboxPanel is the M2 milestone gate.
//
//   DUCK-TYPED: no base class, no virtual (D7). The registry's closure requires exactly two things — that
//   this type is default-constructible, and that `Draw(DummyComponent&)` is callable on it. Nothing
//   inherits, nothing registers a vtable.
//
//   Draw is DECLARED here and DEFINED in the .cpp on purpose: the closure only needs to CALL it, so it
//   resolves at link time and <imgui.h> stays out of SandboxEditorModule.cpp.
//
//   Writes straight into the live component — the world is re-read from it every frame
//   (RendererManager::Render -> Renderer2D::DrawQuad), so an edit shows up on the next frame.
// =============================================================================
struct DummyComponentDrawer
{
    void Draw(Opaax::DummyComponent& InComponent) const;
};
