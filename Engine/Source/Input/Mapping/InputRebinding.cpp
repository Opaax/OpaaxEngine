#include "Input/Mapping/InputRebinding.h"

#include "Core/Algo/Algo.h"
#include "Core/String/OpaaxStringJson.h"
#include "Input/InputKeyNames.h"

namespace Opaax
{
    namespace
    {
        /** "Input/Controls.opaaxasset: Jump on Space", for what a log or a menu says about a slot. */
        OpaaxString Describe(const InputKeySlot& InSlot)
        {
            return OpaaxString::Format("{}: {} on {}", InSlot.Context.CStr(), InSlot.Action.CStr(),
                                       ToString(InSlot.DefaultKey));
        }

        const InputContextLayout* FindLayout(const TDynArray<InputContextLayout>& InLayouts, const OpaaxString& InContext)
        {
            for (const InputContextLayout& lLayout : InLayouts)
            {
                if (lLayout.Context == InContext)
                {
                    return &lLayout;
                }
            }
            return nullptr;
        }

        bool HasSlot(const InputContextLayout& InLayout, const InputKeySlot& InSlot)
        {
            return Algo::AnyOf(InLayout.Slots, [&InSlot](const InputKeySlot& InCandidate) { return InCandidate == InSlot; });
        }

        void Drop(TDynArray<OpaaxString>* OutDropped, OpaaxString InWhy)
        {
            if (OutDropped != nullptr)
            {
                OutDropped->push_back(Move(InWhy));
            }
        }
    }

    // =============================================================================
    // Keys
    // =============================================================================
    EKeyCode InputRebinds::GetKey(const InputKeySlot& InSlot) const noexcept
    {
        const Entry* lEntry = Find(InSlot);
        return lEntry != nullptr ? lEntry->Key : InSlot.DefaultKey;
    }

    InputRebindResult InputRebinds::Rebind(const TDynArray<InputContextLayout>& InLayouts, const InputKeySlot& InSlot,
                                           const EKeyCode InKey, const EKeyConflictPolicy InPolicy,
                                           const FInputKeyCheck& InCheck)
    {
        InputRebindResult lResult;
        const auto lRefuse = [&lResult](const ERebindResult InWhy)
        {
            lResult.Result = InWhy;
            return lResult;
        };

        const InputContextLayout* lLayout = FindLayout(InLayouts, InSlot.Context);
        if (lLayout == nullptr || !HasSlot(*lLayout, InSlot))
        {
            return lRefuse(ERebindResult::NoSuchMapping);
        }
        if (!lLayout->bRebindable)
        {
            return lRefuse(ERebindResult::Locked);
        }
        if (InKey != EKeyCode::None && !IsKeyCodeBindable(InKey))
        {
            return lRefuse(ERebindResult::InvalidKey);
        }

        const EKeyCode lOld = GetKey(InSlot);
        if (InKey == lOld)
        {
            return lResult;
        }

        if (InKey != EKeyCode::None)
        {
            // A locked context keeps its keys (a menu's Escape): never taken, whatever the policy.
            for (const InputContextLayout& lOther : InLayouts)
            {
                for (const InputKeySlot& lSlot : lOther.Slots)
                {
                    if (!lOther.bRebindable && GetKey(lSlot) == InKey)
                    {
                        lResult.Others.push_back(InputKeyMapping{ lSlot, InKey });
                    }
                }
            }
            if (!lResult.Others.empty())
            {
                return lRefuse(ERebindResult::Reserved);
            }

            // Only the slot's own context: another context is active at other times, or below it.
            for (const InputKeySlot& lSlot : lLayout->Slots)
            {
                if (!(lSlot == InSlot) && GetKey(lSlot) == InKey)
                {
                    lResult.Others.push_back(InputKeyMapping{ lSlot, InKey });
                }
            }
        }

        if (!lResult.Others.empty() && InPolicy == EKeyConflictPolicy::Refuse)
        {
            return lRefuse(ERebindResult::Conflict);
        }

        const EKeyCode lOthersKey = (InPolicy == EKeyConflictPolicy::Swap) ? lOld : EKeyCode::None;
        for (InputKeyMapping& lOther : lResult.Others)
        {
            lOther.Key = lOthersKey;
        }

        // The game says no to a key it keeps for itself, or one an action must not have.
        if (InCheck)
        {
            bool bAllowed = InCheck(InSlot, InKey, lResult.Why);
            for (Uint64 lIndex = 0; bAllowed && lIndex < lResult.Others.size(); ++lIndex)
            {
                bAllowed = InCheck(lResult.Others[lIndex].Slot, lResult.Others[lIndex].Key, lResult.Why);
            }

            if (!bAllowed)
            {
                lResult.Others.clear();
                return lRefuse(ERebindResult::RefusedByGame);
            }
        }

        Set(InSlot, InKey);
        for (const InputKeyMapping& lOther : lResult.Others)
        {
            Set(lOther.Slot, lOther.Key);
        }
        return lResult;
    }

    void InputRebinds::Reset(const InputKeySlot& InSlot)
    {
        Algo::RemoveIf(m_Rebinds, [&InSlot](const Entry& InEntry) { return InEntry.Slot == InSlot; });
    }

    void InputRebinds::ResetContext(const OpaaxString& InContext)
    {
        Algo::RemoveIf(m_Rebinds, [&InContext](const Entry& InEntry) { return InEntry.Slot.Context == InContext; });
    }

    bool InputRebinds::HasRebinds(const OpaaxString& InContext) const noexcept
    {
        return Algo::AnyOf(m_Rebinds, [&InContext](const Entry& InEntry) { return InEntry.Slot.Context == InContext; });
    }

    Uint64 InputRebinds::DropUnknown(const InputContextLayout& InLayout, TDynArray<OpaaxString>* OutDropped)
    {
        const Uint64 lBefore = Num();

        Algo::RemoveIf(m_Rebinds, [&InLayout, OutDropped](const Entry& InEntry)
        {
            if (InEntry.Slot.Context != InLayout.Context)
            {
                return false;
            }
            if (!InLayout.bRebindable)
            {
                Drop(OutDropped, Describe(InEntry.Slot) + ": the context is locked");
                return true;
            }
            if (!HasSlot(InLayout, InEntry.Slot))
            {
                Drop(OutDropped, Describe(InEntry.Slot) + ": the context has no such mapping");
                return true;
            }
            return false;
        });

        return lBefore - Num();
    }

    // =============================================================================
    // As the settings keep them
    // =============================================================================
    nlohmann::json InputRebinds::ToJson() const
    {
        if (m_Rebinds.empty())
        {
            return nullptr;
        }

        nlohmann::json lJson = nlohmann::json::object();
        for (const Entry& lEntry : m_Rebinds)
        {
            lJson[lEntry.Slot.Context.CStr()][lEntry.Slot.Action.CStr()][ToString(lEntry.Slot.DefaultKey)] =
                ToString(lEntry.Key);
        }
        return lJson;
    }

    void InputRebinds::FromJson(const nlohmann::json& InJson, TDynArray<OpaaxString>* OutDropped)
    {
        m_Rebinds.clear();
        if (InJson.is_null())
        {
            return;
        }
        if (!InJson.is_object())
        {
            Drop(OutDropped, OpaaxString("the keys are an object: {\"Input/Controls.opaaxasset\": {\"Jump\": {\"Space\": \"E\"}}}"));
            return;
        }

        for (const auto& lContext : InJson.items())
        {
            const OpaaxString lContextPath = FromJsonString(lContext.key());
            if (!lContext.value().is_object())
            {
                Drop(OutDropped, lContextPath + ": not an object of actions");
                continue;
            }

            for (const auto& lAction : lContext.value().items())
            {
                const OpaaxString lActionName = FromJsonString(lAction.key());
                if (!lAction.value().is_object())
                {
                    Drop(OutDropped, lContextPath + ": " + lActionName + ": not an object of keys");
                    continue;
                }

                for (const auto& lKey : lAction.value().items())
                {
                    InputKeySlot lSlot;
                    lSlot.Context = lContextPath;
                    lSlot.Action  = OpaaxStringID(lActionName);

                    EKeyCode          lBound       = EKeyCode::None;
                    const OpaaxString lDefaultName = FromJsonString(lKey.key());
                    const OpaaxString lBoundName   = lKey.value().is_string() ? lKey.value().get<OpaaxString>()
                                                                              : ToJsonText(lKey.value());

                    const bool bDefaultKnown = KeyFromName(lDefaultName, lSlot.DefaultKey);
                    if (!bDefaultKnown || !KeyFromName(lBoundName, lBound))
                    {
                        Drop(OutDropped, OpaaxString::Format("{}: {} on {}: '{}' is no key name", lContextPath.CStr(),
                                                             lActionName.CStr(), lDefaultName.CStr(),
                                                             bDefaultKnown ? lBoundName.CStr() : lDefaultName.CStr()));
                        continue;
                    }

                    Set(lSlot, lBound);
                }
            }
        }
    }

    // =============================================================================
    // Internal
    // =============================================================================
    const InputRebinds::Entry* InputRebinds::Find(const InputKeySlot& InSlot) const noexcept
    {
        for (const Entry& lEntry : m_Rebinds)
        {
            if (lEntry.Slot == InSlot)
            {
                return &lEntry;
            }
        }
        return nullptr;
    }

    void InputRebinds::Set(const InputKeySlot& InSlot, const EKeyCode InKey)
    {
        if (InKey == InSlot.DefaultKey)
        {
            Reset(InSlot);
            return;
        }

        for (Entry& lEntry : m_Rebinds)
        {
            if (lEntry.Slot == InSlot)
            {
                lEntry.Key = InKey;
                return;
            }
        }
        m_Rebinds.push_back(Entry{ InSlot, InKey });
    }
}
