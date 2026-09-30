#pragma once

#include "Components/TagsComponent.h"

namespace Opaax::Editor { class IEditorWidgets; }

// =============================================================================
// TagsComponentDrawer — the Inspector UI for TagsComponent. No base class: Draw is declared here
//   and defined in the .cpp. Uses IEditorWidgets only.
//   The text field is untrusted input, so it is checked with OpaaxTag::IsValidTagText.
// =============================================================================
struct TagsComponentDrawer
{
    void Draw(Opaax::Editor::IEditorWidgets& InWidgets, Sandbox::TagsComponent& InComponent) const;
};
