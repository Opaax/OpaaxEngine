#include "Drawers/TagsComponentDrawer.h"

#include <imgui.h>

using namespace Opaax;

namespace
{
    // Editor scratch for the add-field, not engine state: one exe, one Inspector, and nothing
    // outside this function can reach it. I1 is about the engine's shared object graph.
    char g_NewTagBuffer[96] = {};
}

void TagsComponentDrawer::Draw(Sandbox::TagsComponent& InComponent) const
{
    if (!ImGui::CollapsingHeader("Tags", ImGuiTreeNodeFlags_DefaultOpen))
    {
        return;
    }

    // Removing inside the loop would invalidate the very range being walked, so the verb is QUEUED
    // and applied after the draw pass — the same shape the Hierarchy's context menu needed.
    OpaaxTag lToRemove;

    for (const OpaaxTag lTag : InComponent.Tags)
    {
        ImGui::PushID(static_cast<int>(lTag.GetName().GetId()));

        if (ImGui::SmallButton("x")) { lToRemove = lTag; }

        ImGui::SameLine();
        ImGui::TextUnformatted(lTag.GetName().CStr());

        ImGui::PopID();
    }

    if (InComponent.Tags.IsEmpty())
    {
        ImGui::TextDisabled("No tags");
    }

    if (lToRemove.IsValid()) { InComponent.Tags.RemoveTag(lToRemove); }

    ImGui::Separator();

    const bool lSubmitted = ImGui::InputText("##NewTag", g_NewTagBuffer, sizeof(g_NewTagBuffer),
                                             ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();

    // A typed field is UNTRUSTED input: gate on the predicate rather than letting the OpaaxTag ctor
    // assert on every half-finished word (I14).
    const OpaaxStringView lTyped(g_NewTagBuffer);
    const bool            lIsValid = OpaaxTag::IsValidTagText(lTyped);

    ImGui::BeginDisabled(!lIsValid);
    const bool lAdded = ImGui::Button("Add");
    ImGui::EndDisabled();

    if (lIsValid && (lAdded || lSubmitted))
    {
        InComponent.Tags.AddTag(OpaaxTag(lTyped));
        g_NewTagBuffer[0] = '\0';
    }

    if (!lTyped.IsEmpty() && !lIsValid)
    {
        ImGui::TextDisabled("Tag needs dotted segments, e.g. Sandbox.Quad.White");
    }
}
