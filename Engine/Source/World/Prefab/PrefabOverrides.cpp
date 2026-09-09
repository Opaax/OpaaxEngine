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

        // The merge-patch (RFC 7386) that turns InFrom into InTo: only the members that differ,
        // recursing into nested objects so a Transform whose Position moved does not drag its
        // Rotation and Scale along.
        //
        // nlohmann ships `diff` (RFC 6902, a JSON PATCH — an array of ops) but NOT a merge-patch
        // producer, only the `merge_patch` applier. This is that missing half, and it is short
        // because merge-patch is the simple format: an object of the changed members, `null` for a
        // removed one.
        nlohmann::json MakeMergePatch(const nlohmann::json& InFrom, const nlohmann::json& InTo)
        {
            // Not both objects — the value is replaced wholesale. This is where an ARRAY lands, and
            // it is the documented limit: merge-patch cannot address an element.
            if (!InFrom.is_object() || !InTo.is_object())
            {
                return InTo;
            }

            nlohmann::json lPatch = nlohmann::json::object();

            // Members that changed or were added.
            for (const auto& [lKey, lValue] : InTo.items())
            {
                const auto lIt = InFrom.find(lKey);
                if (lIt == InFrom.end())
                {
                    lPatch[lKey] = lValue;
                }
                else if (*lIt != lValue)
                {
                    lPatch[lKey] = MakeMergePatch(*lIt, lValue);
                }
            }

            // Members that went away. `null` is merge-patch's removal.
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
                // Added on the instance — the whole payload, since there is nothing to diff against.
                lComponents[lInstance.TypeName.CStr()] = lInstance.Payload;
                continue;
            }

            if (lTemplate->Payload == lInstance.Payload) { continue; }   // identical: no entry

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

        const auto lComponentsIt = InPatch.find(KEY_COMPONENTS);
        if (lComponentsIt == InPatch.end() || !lComponentsIt->is_object()) { return; }

        for (const auto& [lTypeName, lPatch] : lComponentsIt->items())
        {
            if (lTypeName.empty()) { continue; }

            const OpaaxStringID lId(lTypeName);

            // `null` REMOVES — merge-patch's own convention, and what an instance that deleted a
            // component off its template records.
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

            // Not on the template: the instance ADDED this component, so the patch is the payload.
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
