#include "Engine/GameInstance/GameInstanceSubsystemRegistry.h"

#include <spdlog/fmt/ranges.h>   // fmt::join

namespace Opaax
{
    // Refusals log an error and return false (not an assert, so Release builds report them too).
    bool GameInstanceSubsystemRegistry::AddEntry(TUniquePtr<IGameInstanceSubsystemEntry> InEntry,
                                                OpaaxStringID InName)
    {
        if (InEntry == nullptr)
        {
            return false;
        }

        if (m_bSealed)
        {
            OPAAX_LOG(LogGameInstanceSubsystemRegistry, Error,
                      "Register '{}' — registry is SEALED (a world already exists). Game instance subsystem types must be registered before the first CreateWorld.",
                      InName);
            return false;
        }

        if (!InName.IsValid())
        {
            OPAAX_LOG(LogGameInstanceSubsystemRegistry, Error,
                      "Register — refused a game instance subsystem with an empty name.");
            return false;
        }

        if (FindByName(InName) != nullptr)
        {
            OPAAX_LOG(LogGameInstanceSubsystemRegistry, Error, "Register '{}' — that name is already taken.",
                      InName);
            return false;
        }

        m_Entries.emplace_back(Move(InEntry));

        return true;
    }

    void GameInstanceSubsystemRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        TDynArray<OpaaxStringID> lNames;
        for (const auto& lEntry : m_Entries) { lNames.push_back(lEntry->GetName()); }

        OPAAX_LOG(LogGameInstanceSubsystemRegistry, Info, "Sealed with {} game instance subsystem type(s): {}",
                  static_cast<Uint64>(m_Entries.size()), fmt::join(lNames, ", "));
    }

    const IGameInstanceSubsystemEntry* GameInstanceSubsystemRegistry::FindByName(OpaaxStringID InName) const noexcept
    {
        for (const TUniquePtr<IGameInstanceSubsystemEntry>& lEntry : m_Entries)
        {
            if (lEntry->GetName() == InName)
            {
                return lEntry.get();
            }
        }

        return nullptr;
    }
}
