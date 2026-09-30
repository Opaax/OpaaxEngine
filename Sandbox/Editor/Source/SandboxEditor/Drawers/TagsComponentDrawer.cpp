#include "Drawers/TagsComponentDrawer.h"

#include "Editor/UI/IEditorWidgets.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    // Editor scratch buffer for the add field (only this function uses it).
    char g_NewTagBuffer[96] = {};
}

void TagsComponentDrawer::Draw(IEditorWidgets& InWidgets, Sandbox::TagsComponent& InComponent) const
{
    if (!InWidgets.CollapsingHeader("Tags"))
    {
        return;
    }

    // Removing inside the loop would invalidate the range, so the removal is queued until after the draw.
    OpaaxTag lToRemove;

    for (const OpaaxTag lTag : InComponent.Tags)
    {
        // Keyed by the interned id: two tags never share a scope.
        InWidgets.PushId(lTag.GetName().GetId());

        if (InWidgets.SmallButton("x")) { lToRemove = lTag; }

        InWidgets.SameLine();
        InWidgets.Text(lTag.GetName().CStr());

        InWidgets.PopId();
    }

    if (InComponent.Tags.IsEmpty())
    {
        InWidgets.TextDisabled("No tags");
    }

    if (lToRemove.IsValid()) { InComponent.Tags.RemoveTag(lToRemove); }

    InWidgets.Separator();

    const bool lSubmitted = InWidgets.InputText("##NewTag", g_NewTagBuffer, sizeof(g_NewTagBuffer),
                                                /*bInSubmitOnEnter*/ true);
    InWidgets.SameLine();

    // Typed text is untrusted: check it first instead of letting the OpaaxTag ctor assert.
    const OpaaxStringView lTyped(g_NewTagBuffer);
    const bool            lIsValid = OpaaxTag::IsValidTagText(lTyped);

    InWidgets.BeginDisabled(!lIsValid);
    const bool lAdded = InWidgets.Button("Add");
    InWidgets.EndDisabled();

    if (lIsValid && (lAdded || lSubmitted))
    {
        InComponent.Tags.AddTag(OpaaxTag(lTyped));
        g_NewTagBuffer[0] = '\0';
    }

    if (!lTyped.IsEmpty() && !lIsValid)
    {
        InWidgets.TextDisabled("Tag needs dotted segments, e.g. Sandbox.Quad.White");
    }
}
