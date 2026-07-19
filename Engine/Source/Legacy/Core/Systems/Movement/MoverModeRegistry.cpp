#include "MoverModeRegistry.h"

#include "Core/Systems/Movement/IMoverMode.h"

namespace Opaax
{
    UnorderedMap<Uint32, UniquePtr<IMoverMode>>& MoverModeRegistry::Modes()
    {
        static UnorderedMap<Uint32, UniquePtr<IMoverMode>> s_Modes;
        return s_Modes;
    }

    void MoverModeRegistry::Register(OpaaxStringID InId, UniquePtr<IMoverMode> InMode)
    {
        if (InMode == nullptr) { return; }
        Modes()[InId.GetId()] = Move(InMode);
    }

    IMoverMode* MoverModeRegistry::Find(OpaaxStringID InId)
    {
        auto& lModes = Modes();
        const auto lIt = lModes.find(InId.GetId());
        return lIt != lModes.end() ? lIt->second.get() : nullptr;
    }

    TDynArray<OpaaxStringID> MoverModeRegistry::GetModeIds()
    {
        TDynArray<OpaaxStringID> lIds;
        const auto& lModes = Modes();
        lIds.reserve(lModes.size());
        for (const auto& [lKey, lMode] : lModes)
        {
            lIds.push_back(OpaaxStringID(lKey));
        }
        return lIds;
    }

    void MoverModeRegistry::Clear()
    {
        Modes().clear();
    }

} // namespace Opaax
