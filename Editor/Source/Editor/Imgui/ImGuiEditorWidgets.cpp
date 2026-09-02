#include "Editor/Imgui/ImGuiEditorWidgets.h"

#include <imgui.h>

namespace
{
    /**
     * Every ImGui drag reads min >= max as "no bounds", so an unset range (0, 0) passes through
     * verbatim and the ordinary property costs no branch.
     */
    constexpr float DRAG_SPEED = 1.f;
}

namespace Opaax::Editor
{
    void ImGuiEditorWidgets::PushId(const char* InId)  { ImGui::PushID(InId); }
    void ImGuiEditorWidgets::PushId(const Uint32 InId) { ImGui::PushID(static_cast<int>(InId)); }
    void ImGuiEditorWidgets::PopId()                   { ImGui::PopID(); }

    void ImGuiEditorWidgets::SameLine()                { ImGui::SameLine(); }
    void ImGuiEditorWidgets::Separator()               { ImGui::Separator(); }
    void ImGuiEditorWidgets::ToolbarSeparator()        { ImGui::TextDisabled("|"); }

    void ImGuiEditorWidgets::BeginDisabled(const bool bInDisabled) { ImGui::BeginDisabled(bInDisabled); }
    void ImGuiEditorWidgets::EndDisabled()                         { ImGui::EndDisabled(); }

    bool ImGuiEditorWidgets::BeginTreeNode(const char* InLabel)
    {
        return ImGui::TreeNodeEx(InLabel, ImGuiTreeNodeFlags_DefaultOpen);
    }

    void ImGuiEditorWidgets::EndTreeNode() { ImGui::TreePop(); }

    bool ImGuiEditorWidgets::CollapsingHeader(const char* InLabel)
    {
        return ImGui::CollapsingHeader(InLabel, ImGuiTreeNodeFlags_DefaultOpen);
    }

    void ImGuiEditorWidgets::Text(const char* InText)         { ImGui::TextUnformatted(InText); }
    void ImGuiEditorWidgets::TextDisabled(const char* InText) { ImGui::TextDisabled("%s", InText); }

    void ImGuiEditorWidgets::LabelText(const char* InLabel, const char* InValue)
    {
        ImGui::LabelText(InLabel, "%s", InValue);
    }

    void ImGuiEditorWidgets::HelpMarker(const char* InText)
    {
        ImGui::TextDisabled("(?)");
        ImGui::SetItemTooltip("%s", InText);
    }

    bool ImGuiEditorWidgets::Button(const char* InLabel, const float InWidth, const char* InTooltip)
    {
        // Negative width fills, matching ImGui's own convention; 0 fits the label.
        const float lWidth = InWidth < 0.f ? ImGui::CalcItemWidth() : InWidth;

        const bool lClicked = ImGui::Button(InLabel, ImVec2(lWidth, 0.f));

        // The hover query lives HERE rather than at the call site, which is what keeps the seam
        // free of submission-order semantics.
        if (InTooltip != nullptr && ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", InTooltip);
        }

        return lClicked;
    }

    bool ImGuiEditorWidgets::SmallButton(const char* InLabel) { return ImGui::SmallButton(InLabel); }

    bool ImGuiEditorWidgets::Checkbox(const char* InLabel, bool& InValue)
    {
        return ImGui::Checkbox(InLabel, &InValue);
    }

    bool ImGuiEditorWidgets::DragFloat(const char* InLabel, float* InValues, const Uint32 InCount,
                                       const float InMin, const float InMax)
    {
        switch (InCount)
        {
        case 1:  return ImGui::DragFloat(InLabel, InValues, DRAG_SPEED, InMin, InMax);
        case 2:  return ImGui::DragFloat2(InLabel, InValues, DRAG_SPEED, InMin, InMax);
        case 3:  return ImGui::DragFloat3(InLabel, InValues, DRAG_SPEED, InMin, InMax);
        case 4:  return ImGui::DragFloat4(InLabel, InValues, DRAG_SPEED, InMin, InMax);
        default: return false;
        }
    }

    bool ImGuiEditorWidgets::DragInt16(const char* InLabel, Int16& InValue, const Int16 InMin, const Int16 InMax)
    {
        // DragScalar on the REAL type: a round trip through Int32 would let a drag past 32767 wrap
        // to a large negative value instead of clamping.
        return ImGui::DragScalar(InLabel, ImGuiDataType_S16, &InValue, DRAG_SPEED, &InMin, &InMax);
    }

    bool ImGuiEditorWidgets::DragInt32(const char* InLabel, Int32& InValue, const Int32 InMin, const Int32 InMax)
    {
        return ImGui::DragInt(InLabel, &InValue, DRAG_SPEED, InMin, InMax);
    }

    bool ImGuiEditorWidgets::DragUint32(const char* InLabel, Uint32& InValue, const Uint32 InMin, const Uint32 InMax)
    {
        // Same reason as Int16, at the other end: via Int32 a drag below zero wraps to ~4 billion.
        // A max that does not exceed the min is passed as null, i.e. unbounded above.
        return ImGui::DragScalar(InLabel, ImGuiDataType_U32, &InValue, DRAG_SPEED, &InMin,
                                 InMax > InMin ? &InMax : nullptr);
    }

    bool ImGuiEditorWidgets::ColorEdit(const char* InLabel, float* InRgba)
    {
        return ImGui::ColorEdit4(InLabel, InRgba);
    }

    bool ImGuiEditorWidgets::InputText(const char* InLabel, char* InBuffer, const Uint32 InSize,
                                       const bool bInSubmitOnEnter)
    {
        return ImGui::InputText(InLabel, InBuffer, InSize,
                                bInSubmitOnEnter ? ImGuiInputTextFlags_EnterReturnsTrue
                                                 : ImGuiInputTextFlags_None);
    }

    bool ImGuiEditorWidgets::BeginCombo(const char* InLabel, const char* InPreview)
    {
        return ImGui::BeginCombo(InLabel, InPreview);
    }

    bool ImGuiEditorWidgets::Selectable(const char* InLabel, const bool bInSelected)
    {
        return ImGui::Selectable(InLabel, bInSelected);
    }

    void ImGuiEditorWidgets::SetDefaultFocus() { ImGui::SetItemDefaultFocus(); }
    void ImGuiEditorWidgets::EndCombo()        { ImGui::EndCombo(); }
}
