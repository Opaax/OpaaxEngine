#include "Editor/Properties/PropertyDrawers.h"

#include <cstring>   // memcpy — the string drawer's buffer

#include <imgui.h>
#include <glm/gtc/type_ptr.hpp>   // value_ptr — the codebase's glm<->float* idiom (Renderer2D.cpp)

namespace Opaax::Editor
{
    namespace
    {
        // Every ImGui drag reads min >= max as "no bounds", so an unset range (0, 0) needs no branch
        // — the facet is passed through verbatim and the ordinary property costs nothing.
        constexpr float DRAG_SPEED = 1.f;
    }

    void TPropertyDrawer<bool>::Draw(const char* InLabel, bool& InValue, const PropertyMeta&)
    {
        ImGui::Checkbox(InLabel, &InValue);
    }

    void TPropertyDrawer<Int16>::Draw(const char* InLabel, Int16& InValue, const PropertyMeta& InMeta)
    {
        // DragScalar on the REAL type, not a round trip through Int32: the narrower type has to
        // clamp at its own bounds, or a drag past 32767 wraps to a large negative draw order.
        const Int16 lMin = InMeta.RangeMax > InMeta.RangeMin ? static_cast<Int16>(InMeta.RangeMin) : Int16{-32768};
        const Int16 lMax = InMeta.RangeMax > InMeta.RangeMin ? static_cast<Int16>(InMeta.RangeMax) : Int16{32767};

        ImGui::DragScalar(InLabel, ImGuiDataType_S16, &InValue, DRAG_SPEED, &lMin, &lMax);
    }

    void TPropertyDrawer<Int32>::Draw(const char* InLabel, Int32& InValue, const PropertyMeta& InMeta)
    {
        ImGui::DragInt(InLabel, &InValue, DRAG_SPEED,
                       static_cast<int>(InMeta.RangeMin), static_cast<int>(InMeta.RangeMax));
    }

    void TPropertyDrawer<Uint32>::Draw(const char* InLabel, Uint32& InValue, const PropertyMeta& InMeta)
    {
        // DragScalar rather than a cast through DragInt: a round trip via Int32 would let a drag
        // below zero wrap to ~4 billion instead of clamping at the type's own floor. Zero is that
        // floor whether or not the property states a range.
        const Uint32 lMin = InMeta.RangeMin > 0.f ? static_cast<Uint32>(InMeta.RangeMin) : 0u;
        const Uint32 lMax = InMeta.RangeMax > InMeta.RangeMin ? static_cast<Uint32>(InMeta.RangeMax) : 0u;

        ImGui::DragScalar(InLabel, ImGuiDataType_U32, &InValue, DRAG_SPEED, &lMin,
                          lMax > lMin ? &lMax : nullptr);
    }

    void TPropertyDrawer<float>::Draw(const char* InLabel, float& InValue, const PropertyMeta& InMeta)
    {
        ImGui::DragFloat(InLabel, &InValue, DRAG_SPEED, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<Vector2F>::Draw(const char* InLabel, Vector2F& InValue, const PropertyMeta& InMeta)
    {
        ImGui::DragFloat2(InLabel, glm::value_ptr(InValue), DRAG_SPEED, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<Vector3F>::Draw(const char* InLabel, Vector3F& InValue, const PropertyMeta& InMeta)
    {
        ImGui::DragFloat3(InLabel, glm::value_ptr(InValue), DRAG_SPEED, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<Vector4F>::Draw(const char* InLabel, Vector4F& InValue, const PropertyMeta& InMeta)
    {
        ImGui::DragFloat4(InLabel, glm::value_ptr(InValue), DRAG_SPEED, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<LinearColor>::Draw(const char* InLabel, LinearColor& InValue, const PropertyMeta&)
    {
        ImGui::ColorEdit4(InLabel, glm::value_ptr(static_cast<Vector4F&>(InValue)));
    }

    void TPropertyDrawer<OpaaxString>::Draw(const char* InLabel, OpaaxString& InValue, const PropertyMeta&)
    {
        // A stack buffer per frame rather than a cached one: the value is the source of truth and
        // ImGui edits the buffer in place, so copying in each frame is what keeps the widget honest
        // when something else changes the string. No state, nothing to invalidate.
        constexpr Uint32 k_BufferSize = 512;

        // REFUSED rather than truncated. Silently dropping the tail of a path because the editor's
        // buffer is smaller than the value is the failure class this codebase hates most; a value
        // this long is not editable here, and says so.
        if (InValue.GetLength() >= k_BufferSize)
        {
            ImGui::LabelText(InLabel, "%s", InValue.CStr());
            ImGui::SameLine();
            ImGui::TextDisabled("(too long to edit)");
            return;
        }

        char lBuffer[k_BufferSize];
        std::memcpy(lBuffer, InValue.CStr(), InValue.GetLength());
        lBuffer[InValue.GetLength()] = '\0';

        if (ImGui::InputText(InLabel, lBuffer, k_BufferSize))
        {
            InValue = OpaaxString(lBuffer);
        }
    }
}
