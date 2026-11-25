#pragma once

#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Properties/ObjectPropertyDrawer.h"   // PropertyPresentationView
#include "Renderer/Materials/Material2D.h"
#include "Renderer/Materials/ShaderParams.h"

namespace Opaax::Editor
{
    // =============================================================================
    // MaterialEditing — what a material's window does, without a window: which of the twelve shader
    //   slots it shows and under what names, and the values a newly picked shader asks for.
    //   Header-only, so the tests drive it without the editor.
    // =============================================================================
    namespace MaterialEditing
    {
        /** The steps a range is dragged in. */
        inline constexpr float RANGE_DRAG_STEPS = 200.f;

        /**
         * How InOutView shows the material field InField when its shader declares InParams.
         * A field that is no shader slot, or a shader that declares nothing (or is not loaded: null),
         * is shown as it is. A declared slot shows under its declared name, with its tooltip and range;
         * a slot the shader does not declare is hidden.
         * @return False to hide the field
         */
        inline bool Present(const OpaaxStringView InField, const ShaderParamList* InParams,
                            PropertyPresentationView& InOutView)
        {
            EShaderParamSlot lSlot = EShaderParamSlot::Count;
            if (InParams == nullptr || InParams->Params.empty() || !ShaderParams::SlotFromName(InField, lSlot))
            {
                return true;
            }

            const ShaderParam* lParam = InParams->Find(lSlot);
            if (lParam == nullptr)
            {
                return false;
            }

            InOutView.Label = lParam->Name;

            const OpaaxString lWhere = OpaaxString::Format("{} in the material file, {} in the shader",
                                                           ShaderParams::SlotName(lSlot), lParam->Name.CStr());
            InOutView.Tooltip = lParam->Tooltip.IsEmpty() ? lWhere : lParam->Tooltip + "\n" + lWhere;

            if (lParam->bHasRange)
            {
                InOutView.Meta.RangeMin = lParam->RangeMin;
                InOutView.Meta.RangeMax = lParam->RangeMax;
                InOutView.Meta.DragStep = (lParam->RangeMax - lParam->RangeMin) / RANGE_DRAG_STEPS;
            }
            return true;
        }

        /** The material's number in InSlot (Param0..7), to read or write. */
        inline float& NumberOf(Material2D& InMaterial, const EShaderParamSlot InSlot)
        {
            float Material2D::* const k_Numbers[] = {
                &Material2D::Param0, &Material2D::Param1, &Material2D::Param2, &Material2D::Param3,
                &Material2D::Param4, &Material2D::Param5, &Material2D::Param6, &Material2D::Param7,
            };
            const Uint32 lIndex = Maths::Min(static_cast<Uint32>(InSlot), 7u);
            return InMaterial.*k_Numbers[lIndex];
        }

        /**
         * Writes the defaults InParams declares (`default(v)`) into InOutMaterial: what a newly picked
         * shader asks for.
         * @return How many values changed
         */
        inline Uint32 ApplyDefaults(Material2D& InOutMaterial, const ShaderParamList& InParams)
        {
            Uint32 lChanged = 0;
            for (const ShaderParam& lParam : InParams.Params)
            {
                if (!lParam.bHasDefault || !ShaderParams::IsNumber(lParam.Slot))
                {
                    continue;
                }

                float& lValue = NumberOf(InOutMaterial, lParam.Slot);
                if (lValue != lParam.Default)
                {
                    lValue = lParam.Default;
                    ++lChanged;
                }
            }
            return lChanged;
        }
    }
}
