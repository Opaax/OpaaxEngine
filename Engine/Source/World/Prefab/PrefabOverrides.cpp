#include "World/Prefab/PrefabOverrides.h"

namespace Opaax
{
    namespace
    {
        const ComponentData* FindComponent(const TDynArray<ComponentData>& InComponents,
                                           OpaaxStringID InName)
        {
            for (const ComponentData& lComponent : InComponents)
            {
                if (lComponent.TypeName == InName) { return &lComponent; }
            }

            return nullptr;
        }

        // Are two JSON values equal as the engine stores them?
        //   Floats are compared at float precision: a file says 0.35 but a capture of the float writes
        //   0.3499999940395355, and a plain == would record a false override. Integers compare exactly.
        //   (A component storing a double would be compared too loosely; none does.)
        bool SameValue(const nlohmann::json& InLeft, const nlohmann::json& InRight)
        {
            if (InLeft.is_number_float() || InRight.is_number_float())
            {
                if (!InLeft.is_number() || !InRight.is_number()) { return false; }

                return static_cast<float>(InLeft.get<double>()) == static_cast<float>(InRight.get<double>());
            }

            if (InLeft.is_object() && InRight.is_object())
            {
                if (InLeft.size() != InRight.size()) { return false; }

                for (const auto& [lKey, lValue] : InLeft.items())
                {
                    const auto lIt = InRight.find(lKey);
                    if (lIt == InRight.end() || !SameValue(lValue, *lIt)) { return false; }
                }

                return true;
            }

            if (InLeft.is_array() && InRight.is_array())
            {
                if (InLeft.size() != InRight.size()) { return false; }

                for (std::size_t lIndex = 0; lIndex < InLeft.size(); ++lIndex)
                {
                    if (!SameValue(InLeft[lIndex], InRight[lIndex])) { return false; }
                }

                return true;
            }

            return InLeft == InRight;
        }

        // Builds the merge patch (RFC 7386) from InFrom to InTo: only the changed members,
        // recursing into objects. nlohmann only provides the applier.
        nlohmann::json MakeMergePatch(const nlohmann::json& InFrom, const nlohmann::json& InTo)
        {
            // Not both objects: replaced whole (arrays included; merge patch cannot address an element).
            if (!InFrom.is_object() || !InTo.is_object())
            {
                return InTo;
            }

            nlohmann::json lPatch = nlohmann::json::object();

            // Changed or added members.
            for (const auto& [lKey, lValue] : InTo.items())
            {
                const auto lIt = InFrom.find(lKey);
                if (lIt == InFrom.end())
                {
                    lPatch[lKey] = lValue;
                }
                else if (!SameValue(*lIt, lValue))
                {
                    lPatch[lKey] = MakeMergePatch(*lIt, lValue);
                }
            }

            // Removed members (null).
            for (const auto& [lKey, lValue] : InFrom.items())
            {
                if (InTo.find(lKey) == InTo.end())
                {
                    lPatch[lKey] = nullptr;
                }
            }

            return lPatch;
        }
    }

    nlohmann::json PrefabOverrides::Diff(const EntityData& InTemplate, const EntityData& InInstance,
                                         OpaaxStringID InIgnore)
    {
        nlohmann::json lComponents = nlohmann::json::object();

        // Changed and added components.
        for (const ComponentData& lInstance : InInstance.Components)
        {
            if (lInstance.TypeName == InIgnore) { continue; }

            const ComponentData* lTemplate = FindComponent(InTemplate.Components, lInstance.TypeName);

            if (lTemplate == nullptr)
            {
                // Added on the instance: the whole payload.
                lComponents[lInstance.TypeName.CStr()] = lInstance.Payload;
                continue;
            }

            // SameValue, not == (see SameValue).
            if (SameValue(lTemplate->Payload, lInstance.Payload)) { continue; }   // identical: no entry

            lComponents[lInstance.TypeName.CStr()] = MakeMergePatch(lTemplate->Payload, lInstance.Payload);
        }

        // Removed components.
        for (const ComponentData& lTemplate : InTemplate.Components)
        {
            if (lTemplate.TypeName == InIgnore) { continue; }

            if (FindComponent(InInstance.Components, lTemplate.TypeName) == nullptr)
            {
                lComponents[lTemplate.TypeName.CStr()] = nullptr;
            }
        }

        nlohmann::json lPatch = nlohmann::json::object();

        if (!lComponents.empty())
        {
            lPatch[KEY_COMPONENTS] = Move(lComponents);
        }

        if (InTemplate.Name != InInstance.Name)
        {
            lPatch[KEY_NAME] = InInstance.Name.CStr();
        }

        if (InTemplate.Parent != InInstance.Parent)
        {
            lPatch[KEY_PARENT] = InInstance.Parent.IsValid() ? InInstance.Parent.ToString().CStr() : "";
        }

        return lPatch;
    }

    void PrefabOverrides::Apply(const nlohmann::json& InPatch, EntityData& InOutEntity)
    {
        if (!InPatch.is_object())
        {
            if (!InPatch.is_null())
            {
                OPAAX_LOG(LogPrefabOverrides, Warn,
                          "Override patch on '{}' is not an object — ignored", InOutEntity.Name.CStr());
            }
            return;
        }

        if (const auto lNameIt = InPatch.find(KEY_NAME);
            lNameIt != InPatch.end() && lNameIt->is_string())
        {
            const std::string lName = lNameIt->get<std::string>();
            InOutEntity.Name = OpaaxString(lName.c_str(), static_cast<Uint32>(lName.size()));
        }

        // A parent override is the derived guid, or "" for detached. Unparseable means detached.
        if (const auto lParentIt = InPatch.find(KEY_PARENT);
            lParentIt != InPatch.end() && lParentIt->is_string())
        {
            const std::string lText = lParentIt->get<std::string>();

            Guid lParsed;
            InOutEntity.Parent = Guid::FromString(OpaaxString(lText.c_str(), static_cast<Uint32>(lText.size())), lParsed)
                                     ? lParsed
                                     : Guid{};
        }

        const auto lComponentsIt = InPatch.find(KEY_COMPONENTS);
        if (lComponentsIt == InPatch.end() || !lComponentsIt->is_object()) { return; }

        for (const auto& [lTypeName, lPatch] : lComponentsIt->items())
        {
            if (lTypeName.empty()) { continue; }

            const OpaaxStringID lId(lTypeName);

            // null removes the component.
            if (lPatch.is_null())
            {
                std::erase_if(InOutEntity.Components,
                              [lId](const ComponentData& InComponent)
                              {
                                  return InComponent.TypeName == lId;
                              });
                continue;
            }

            bool lFound = false;
            for (ComponentData& lComponent : InOutEntity.Components)
            {
                if (lComponent.TypeName != lId) { continue; }

                lComponent.Payload.merge_patch(lPatch);
                lFound = true;
                break;
            }

            // Not on the template: added by the instance, the patch is the payload.
            if (!lFound)
            {
                InOutEntity.Components.emplace_back(lId, lPatch);
            }
        }
    }

    bool PrefabOverrides::IsEmpty(const nlohmann::json& InPatch)
    {
        return !InPatch.is_object() || InPatch.empty();
    }
}
