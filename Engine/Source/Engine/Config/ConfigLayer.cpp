#include "Engine/Config/ConfigLayer.h"

#include "Core/String/OpaaxStringJson.h"

namespace Opaax::ConfigLayer
{
    namespace
    {
        void CollectLeaves(const nlohmann::json& InNode, TDynArray<OpaaxString>& InOutKeys, TDynArray<Leaf>& OutLeaves)
        {
            for (const auto& lEntry : InNode.items())
            {
                InOutKeys.push_back(FromJsonString(lEntry.key()));

                if (lEntry.value().is_object())
                {
                    CollectLeaves(lEntry.value(), InOutKeys, OutLeaves);
                }
                else
                {
                    Leaf lLeaf;

                    // The value, wrapped in the keys that lead to it, innermost first.
                    lLeaf.Layer = lEntry.value();
                    for (Uint64 lIndex = InOutKeys.size(); lIndex > 0; --lIndex)
                    {
                        nlohmann::json lWrap = nlohmann::json::object();
                        lWrap[InOutKeys[lIndex - 1].CStr()] = Move(lLeaf.Layer);
                        lLeaf.Layer = Move(lWrap);
                    }

                    for (const OpaaxString& lKey : InOutKeys)
                    {
                        lLeaf.Path += lLeaf.Path.IsEmpty() ? lKey : "." + lKey;
                    }

                    OutLeaves.push_back(Move(lLeaf));
                }

                InOutKeys.pop_back();
            }
        }

        /** The item of InList whose "Name" is InName, or null. */
        nlohmann::json* FindNamed(nlohmann::json& InList, const nlohmann::json::object_t::key_type& InName)
        {
            for (nlohmann::json& lItem : InList)
            {
                if (lItem["Name"] == InName)
                {
                    return &lItem;
                }
            }
            return nullptr;
        }
    }

    TDynArray<Leaf> Leaves(const nlohmann::json& InLayer)
    {
        TDynArray<Leaf> lLeaves;
        if (InLayer.is_object())
        {
            TDynArray<OpaaxString> lKeys;
            CollectLeaves(InLayer, lKeys, lLeaves);
        }
        return lLeaves;
    }

    bool IsNamedList(const nlohmann::json& InValue)
    {
        if (!InValue.is_array() || InValue.empty())
        {
            return false;
        }

        for (const nlohmann::json& lItem : InValue)
        {
            const auto lName = lItem.is_object() ? lItem.find("Name") : lItem.end();
            if (!lItem.is_object() || lName == lItem.end() || !lName->is_string())
            {
                return false;
            }
        }
        return true;
    }

    bool Apply(nlohmann::json& InOutValues, const nlohmann::json& InLayer)
    {
        if (!InLayer.is_object() || !InOutValues.is_object())
        {
            return false;
        }

        for (const auto& lEntry : InLayer.items())
        {
            const auto lAt = InOutValues.find(lEntry.key());
            if (lAt == InOutValues.end())
            {
                return false;
            }

            const nlohmann::json& lValue = lEntry.value();
            if (lValue.is_object() && lAt->is_object())
            {
                if (!Apply(*lAt, lValue))
                {
                    return false;
                }
            }
            else if (lValue.is_object() && IsNamedList(*lAt))
            {
                // Item by item, each by its name; an item is set field by field.
                for (const auto& lNamed : lValue.items())
                {
                    nlohmann::json* const lItem = FindNamed(*lAt, lNamed.key());
                    if (lItem == nullptr || !lNamed.value().is_object() || !Apply(*lItem, lNamed.value()))
                    {
                        return false;
                    }
                }
            }
            else
            {
                *lAt = lValue;
            }
        }
        return true;
    }
}
