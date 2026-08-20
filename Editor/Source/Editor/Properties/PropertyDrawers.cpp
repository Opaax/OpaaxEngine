#include "Editor/Properties/PropertyDrawers.h"

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>   // value_ptr — the codebase's glm<->float* idiom (Renderer2D.cpp)

namespace Opaax::Editor
{
    void TPropertyDrawer<bool>::Draw(const char* InLabel, bool& InValue, EPropertyHint)
    {
        ImGui::Checkbox(InLabel, &InValue);
    }

    void TPropertyDrawer<Int32>::Draw(const char* InLabel, Int32& InValue, EPropertyHint)
    {
        ImGui::DragInt(InLabel, &InValue);
    }

    void TPropertyDrawer<Uint32>::Draw(const char* InLabel, Uint32& InValue, EPropertyHint)
    {
        // DragScalar rather than a cast through DragInt: a round trip via Int32 would let a drag
        // below zero wrap to ~4 billion instead of clamping at the type's own floor.
        constexpr Uint32 k_Min = 0;

        ImGui::DragScalar(InLabel, ImGuiDataType_U32, &InValue, 1.f, &k_Min);
    }

    void TPropertyDrawer<float>::Draw(const char* InLabel, float& InValue, EPropertyHint)
    {
        ImGui::DragFloat(InLabel, &InValue);
    }

    void TPropertyDrawer<Vector2F>::Draw(const char* InLabel, Vector2F& InValue, EPropertyHint)
    {
        ImGui::DragFloat2(InLabel, glm::value_ptr(InValue));
    }

    void TPropertyDrawer<Vector3F>::Draw(const char* InLabel, Vector3F& InValue, EPropertyHint)
    {
        ImGui::DragFloat3(InLabel, glm::value_ptr(InValue));
    }

    void TPropertyDrawer<Vector4F>::Draw(const char* InLabel, Vector4F& InValue, const EPropertyHint InHint)
    {
        if (InHint == EPropertyHint::Color)
        {
            ImGui::ColorEdit4(InLabel, glm::value_ptr(InValue));
            return;
        }

        ImGui::DragFloat4(InLabel, glm::value_ptr(InValue));
    }
}
