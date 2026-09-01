#include "Drawers/TagsComponentDrawer.h"

#include "Editor/UI/IEditorWidgets.h"

using namespace Opaax;
using namespace Opaax::Editor;

namespace
{
    // Editor scratch for the add-field, not engine state: one exe, one Inspector, and nothing
    // outside this function can reach it. I1 is about the engine's shared object graph.
    char g_NewTagBuffer[96] = {};
}

void TagsComponentDrawer::Draw(IEditorWidgets& InWidgets, Sandbox::TagsComponent& InComponent) const
{
    if (!InWidgets.CollapsingHeader("Tags"))
    {
        return;
    }

    // Removing inside the loop would invalidate the very range being walked, so the verb is QUEUED
    // and applied after the draw pass — the same shape the Hierarchy's context menu needed.
    OpaaxTag lToRemove;

    for (const OpaaxTag lTag : InComponent.Tags)
    {
        // Keyed by the interned id: two tags never share a scope, and the label is not the identity.
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

    // A typed field is UNTRUSTED input: gate on the predicate rather than letting the OpaaxTag ctor
    // assert on every half-finished word (I14).
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
