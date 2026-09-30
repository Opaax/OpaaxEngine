#include "Editor/Imgui/ImGuiEditorWidgets.h"

#include <imgui.h>

namespace
{
    /**
     * ImGui drags read min >= max as unbounded, so an unset range (0, 0) passes through as is.
     */
    constexpr float DRAG_SPEED = 0.1f;
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
        // Negative width fills (ImGui's convention); 0 fits the label.
        const float lWidth = InWidth < 0.f ? ImGui::CalcItemWidth() : InWidth;

        const bool lClicked = ImGui::Button(InLabel, ImVec2(lWidth, 0.f));

        // The hover check is done here, not by the caller.
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

    bool ImGuiEditorWidgets::DragFloat(const char* InLabel, float* InValues, const Uint32 InCount, float InStep,
                                       const float InMin, const float InMax)
    {
        float lSpeed = InStep != 0 ? InStep :DRAG_SPEED;
        switch (InCount)
        {
        case 1:  return ImGui::DragFloat(InLabel, InValues, lSpeed, InMin, InMax);
        case 2:  return ImGui::DragFloat2(InLabel, InValues,lSpeed, InMin, InMax);
        case 3:  return ImGui::DragFloat3(InLabel, InValues,lSpeed, InMin, InMax);
        case 4:  return ImGui::DragFloat4(InLabel, InValues,lSpeed, InMin, InMax);
        default: return false;
        }
    }

    bool ImGuiEditorWidgets::DragInt16(const char* InLabel, Int16& InValue, const Int16 InMin, const Int16 InMax)
    {
        // DragScalar on the real type: via Int32 a drag past 32767 would wrap instead of clamping.
        return ImGui::DragScalar(InLabel, ImGuiDataType_S16, &InValue, DRAG_SPEED, &InMin, &InMax);
    }

    bool ImGuiEditorWidgets::DragInt32(const char* InLabel, Int32& InValue, const Int32 InMin, const Int32 InMax)
    {
        return ImGui::DragInt(InLabel, &InValue, DRAG_SPEED, InMin, InMax);
    }

    bool ImGuiEditorWidgets::DragUint32(const char* InLabel, Uint32& InValue, const Uint32 InMin, const Uint32 InMax)
    {
        // Same for the low end: via Int32 a drag below zero would wrap to ~4 billion. A max not above the
        // min is passed as null (unbounded).
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

    bool ImGuiEditorWidgets::InputTextMultiline(const char* InLabel, char* InBuffer, const Uint32 InSize,
                                                const Uint32 InLineCount)
    {
        // Width 0 fills the remaining space; height is rows x line height (scales with the font).
        const ImVec2 lSize(0.f, ImGui::GetTextLineHeight() * static_cast<float>(InLineCount));

        return ImGui::InputTextMultiline(InLabel, InBuffer, InSize, lSize);
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
