#pragma once

#include "Components/TagsComponent.h"

// =============================================================================
// TagsComponentDrawer — the Inspector half of the tag dogfood (I14). Same duck-typed shape as
//   DummyComponentDrawer: no base class, no virtual, Draw declared here and defined in the .cpp so
//   <imgui.h> stays out of SandboxEditorModule.cpp.
//
//   The text field is UNTRUSTED input, which is exactly the case OpaaxTag::IsValidTagText exists
//   for — the ctor's assert is for literals, not for whatever someone types.
// =============================================================================
struct TagsComponentDrawer
{
    void Draw(Sandbox::TagsComponent& InComponent) const;
};
