#pragma once

#include "Editor/UI/IEditorWidgets.h"

namespace Opaax::Editor
{
    // =============================================================================
    // ImGuiEditorWidgets — IEditorWidgets over ImGui. Owned by ImGuiEditorGui, reached through
    //   IEditorGui::Widgets(). Stateless; a class so another backend can replace it.
    //   Not the same as Editor/ImguiLibrary/ImguiWidgets.h (panel-side helpers).
    // =============================================================================
    class ImGuiEditorWidgets final : public IEditorWidgets
    {
        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorWidgets interface
        void PushId(const char* InId) override;
        void PushId(Uint32 InId) override;
        void PopId() override;

        void SameLine() override;
        void Separator() override;
        void ToolbarSeparator() override;
        void BeginDisabled(bool bInDisabled) override;
        void EndDisabled() override;

        bool BeginTreeNode(const char* InLabel) override;
        void EndTreeNode() override;
        bool CollapsingHeader(const char* InLabel) override;

        void Text(const char* InText) override;
        void TextDisabled(const char* InText) override;
        void LabelText(const char* InLabel, const char* InValue) override;
        void HelpMarker(const char* InText) override;

        bool Button(const char* InLabel, float InWidth, const char* InTooltip) override;
        bool SmallButton(const char* InLabel) override;

        bool Checkbox(const char* InLabel, bool& InValue) override;
        bool DragFloat(const char* InLabel, float* InValues, Uint32 InCount, float InStep, float InMin, float InMax) override;
        bool DragInt16(const char* InLabel, Int16& InValue, Int16 InMin, Int16 InMax) override;
        bool DragInt32(const char* InLabel, Int32& InValue, Int32 InMin, Int32 InMax) override;
        bool DragUint32(const char* InLabel, Uint32& InValue, Uint32 InMin, Uint32 InMax) override;
        bool ColorEdit(const char* InLabel, float* InRgba) override;
        bool InputText(const char* InLabel, char* InBuffer, Uint32 InSize, bool bInSubmitOnEnter) override;
        bool InputTextMultiline(const char* InLabel, char* InBuffer, Uint32 InSize, Uint32 InLineCount) override;

        bool BeginCombo(const char* InLabel, const char* InPreview) override;
        bool Selectable(const char* InLabel, bool bInSelected) override;
        void SetDefaultFocus() override;
        void EndCombo() override;
        //~End IEditorWidgets interface
    };
}
