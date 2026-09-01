#include "Editor/Properties/PropertyDrawers.h"

#include <cstring>   // memcpy — the string drawer's buffer

#include <glm/gtc/type_ptr.hpp>   // value_ptr — the codebase's glm<->float* idiom (Renderer2D.cpp)

namespace Opaax::Editor
{
    // NOTHING HERE NAMES A BACKEND. Every drawer speaks IEditorWidgets, so supporting a new field
    // type — which is what a GAME does when it adds one (I15) — costs no backend knowledge.
    //
    // An unset range arrives as min == max, which every drag reads as unbounded, so the ordinary
    // property needs no branch.

    void TPropertyDrawer<bool>::Draw(IEditorWidgets& InWidgets, const char* InLabel, bool& InValue,
                                     const PropertyMeta&)
    {
        InWidgets.Checkbox(InLabel, InValue);
    }

    void TPropertyDrawer<Int16>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Int16& InValue,
                                      const PropertyMeta& InMeta)
    {
        // Clamped at the REAL type's bounds: a drag past 32767 through a wider type would wrap to a
        // large negative draw order. The seam's typed drags exist for exactly this.
        const bool  lHasRange = InMeta.RangeMax > InMeta.RangeMin;
        const Int16 lMin      = lHasRange ? static_cast<Int16>(InMeta.RangeMin) : Int16{-32768};
        const Int16 lMax      = lHasRange ? static_cast<Int16>(InMeta.RangeMax) : Int16{32767};

        InWidgets.DragInt16(InLabel, InValue, lMin, lMax);
    }

    void TPropertyDrawer<Int32>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Int32& InValue,
                                      const PropertyMeta& InMeta)
    {
        InWidgets.DragInt32(InLabel, InValue,
                            static_cast<Int32>(InMeta.RangeMin), static_cast<Int32>(InMeta.RangeMax));
    }

    void TPropertyDrawer<Uint32>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Uint32& InValue,
                                       const PropertyMeta& InMeta)
    {
        // Zero is the floor whether or not the property states a range — below it, a wider type
        // would wrap to ~4 billion.
        const Uint32 lMin = InMeta.RangeMin > 0.f ? static_cast<Uint32>(InMeta.RangeMin) : 0u;
        const Uint32 lMax = InMeta.RangeMax > InMeta.RangeMin ? static_cast<Uint32>(InMeta.RangeMax) : 0u;

        InWidgets.DragUint32(InLabel, InValue, lMin, lMax);
    }

    void TPropertyDrawer<float>::Draw(IEditorWidgets& InWidgets, const char* InLabel, float& InValue,
                                      const PropertyMeta& InMeta)
    {
        InWidgets.DragFloat(InLabel, &InValue, 1, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<Vector2F>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Vector2F& InValue,
                                         const PropertyMeta& InMeta)
    {
        InWidgets.DragFloat(InLabel, glm::value_ptr(InValue), 2, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<Vector3F>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Vector3F& InValue,
                                         const PropertyMeta& InMeta)
    {
        InWidgets.DragFloat(InLabel, glm::value_ptr(InValue), 3, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<Vector4F>::Draw(IEditorWidgets& InWidgets, const char* InLabel, Vector4F& InValue,
                                         const PropertyMeta& InMeta)
    {
        InWidgets.DragFloat(InLabel, glm::value_ptr(InValue), 4, InMeta.RangeMin, InMeta.RangeMax);
    }

    void TPropertyDrawer<LinearColor>::Draw(IEditorWidgets& InWidgets, const char* InLabel,
                                            LinearColor& InValue, const PropertyMeta&)
    {
        InWidgets.ColorEdit(InLabel, glm::value_ptr(static_cast<Vector4F&>(InValue)));
    }

    void TPropertyDrawer<OpaaxString>::Draw(IEditorWidgets& InWidgets, const char* InLabel,
                                            OpaaxString& InValue, const PropertyMeta&)
    {
        // A stack buffer per frame rather than a cached one: the value is the source of truth and the
        // widget edits the buffer in place, so copying in each frame is what keeps it honest when
        // something else changes the string. No state, nothing to invalidate.
        constexpr Uint32 k_BufferSize = 512;

        // REFUSED rather than truncated. Silently dropping the tail of a path because the editor's
        // buffer is smaller than the value is the failure class this codebase hates most; a value
        // this long is not editable here, and says so.
        if (InValue.GetLength() >= k_BufferSize)
        {
            InWidgets.LabelText(InLabel, InValue.CStr());
            InWidgets.SameLine();
            InWidgets.TextDisabled("(too long to edit)");
            return;
        }

        char lBuffer[k_BufferSize];
        std::memcpy(lBuffer, InValue.CStr(), InValue.GetLength());
        lBuffer[InValue.GetLength()] = '\0';

        if (InWidgets.InputText(InLabel, lBuffer, k_BufferSize))
        {
            InValue = OpaaxString(lBuffer);
        }
    }
}
