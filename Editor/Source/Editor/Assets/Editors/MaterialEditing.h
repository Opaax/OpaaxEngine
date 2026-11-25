#pragma once

#include "Core/Maths/Maths.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Properties/ObjectPropertyDrawer.h"   // PropertyPresentationView
#include "Renderer/Materials/Material2D.h"
#include "Renderer/Materials/ShaderParams.h"
#include "Resources/Asset/AssetResource.h"
#include "Resources/ResourceManager.h"

namespace Opaax::Editor
{
    // =============================================================================
    // MaterialEditing — what a material's window does, without a window: which of the twelve shader
    //   slots it shows and under what names, the values a newly picked shader asks for, and the
    //   edited values reaching what is drawn before they are saved.
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

        // =============================================================================
        // MaterialLiveEdit — keeps the loaded copy of a material (the one the renderer, the viewport
        //   and a game played in the editor draw) showing the window's values while they are edited,
        //   saved or not. When the window closes with unsaved values, the copy takes the file's back.
        // =============================================================================
        class MaterialLiveEdit
        {
        public:
            /** Holds the loaded copy of the material at InAbsPath (loaded now if nothing has). */
            void Bind(ResourceManager& InResources, const OpaaxString& InAbsPath)
            {
                m_Resources = &InResources;
                m_AbsPath   = InAbsPath;
                m_Loaded    = InResources.Load<AssetResource>(InAbsPath.CStr());
                m_bPushed   = false;
            }

            /** The loaded copy, or null when the file did not load. */
            Material2D* GetLoaded() const
            {
                AssetResource* lResource = m_Loaded.IsValid() ? m_Loaded.Get() : nullptr;
                return lResource != nullptr ? Cast<Material2D>(lResource->Instance.Get()) : nullptr;
            }

            /**
             * Copies InEdited's values into the loaded copy when they differ.
             * @return True when the loaded copy changed
             */
            bool Push(const Material2D& InEdited)
            {
                Material2D* lLoaded = GetLoaded();
                if (lLoaded == nullptr || lLoaded == &InEdited)
                {
                    return false;
                }

                const ObjectClass&   lClass  = Material2D::StaticClass();
                const nlohmann::json lValues = lClass.WriteProperties(InEdited);
                if (lValues == lClass.WriteProperties(*lLoaded) || !lClass.ReadProperties(*lLoaded, lValues))
                {
                    return false;
                }

                m_bPushed = true;
                return true;
            }

            /**
             * Lets the loaded copy go. With bInUnsaved and values pushed, the file is read back into it
             * first: what was not saved does not stay on screen.
             */
            void Release(const bool bInUnsaved)
            {
                if (bInUnsaved && m_bPushed && m_Resources != nullptr)
                {
                    m_Resources->Reload<AssetResource>(m_AbsPath.CStr());
                }

                m_Loaded  = ResourceRef<AssetResource>();
                m_bPushed = false;
            }

        private:
            ResourceManager*           m_Resources = nullptr;
            OpaaxString                m_AbsPath;
            ResourceRef<AssetResource> m_Loaded;
            bool                       m_bPushed = false;
        };
    }
}
