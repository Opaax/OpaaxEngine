#pragma once

#include "Components/TagsComponent.h"

namespace Opaax::Editor { class IEditorWidgets; }

// =============================================================================
// TagsComponentDrawer — the Inspector half of the tag dogfood (I14). Same duck-typed shape as
//   the built-in drawers: no base class, no virtual, Draw declared here and defined in the .cpp.
//
//   IT NAMES NO BACKEND. The vocabulary it needs — header, id scope, small button, text, separator,
//   text field, disabled state, button — is IEditorWidgets, the same seam the generic fold uses.
//   This drawer is the proof that the seam serves a HAND-WRITTEN drawer and not only the fold.
//
//   The text field is UNTRUSTED input, which is exactly the case OpaaxTag::IsValidTagText exists
//   for — the ctor's assert is for literals, not for whatever someone types.
// =============================================================================
struct TagsComponentDrawer
{
    void Draw(Opaax::Editor::IEditorWidgets& InWidgets, Sandbox::TagsComponent& InComponent) const;
};
