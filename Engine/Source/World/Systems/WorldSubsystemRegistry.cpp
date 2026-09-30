#include "World/Systems/WorldSubsystemRegistry.h"

#include <spdlog/fmt/ranges.h>   // fmt::join

namespace Opaax
{
    // Refusals log an error and return false (not an assert, so Release builds report them too).
    bool WorldSubsystemRegistry::AddEntry(TUniquePtr<IWorldSubsystemEntry> InEntry, OpaaxStringID InName)
    {
        if (InEntry == nullptr)
        {
            return false;
        }

        // Sealed: a world exists, and a type added now would be missing from it.
        if (m_bSealed)
        {
            OPAAX_LOG(LogWorldSubsystemRegistry, Error,
                      "Register '{}' — registry is SEALED (a world already exists). World subsystem types must be registered before the first CreateWorld.",
                      InName);
            return false;
        }

        if (!InName.IsValid())
        {
            OPAAX_LOG(LogWorldSubsystemRegistry, Error, "Register — refused a world subsystem with an empty name.");
            return false;
        }

        // Only the name is checked: the same type under two names is allowed (both are created).
        if (FindByName(InName) != nullptr)
        {
            OPAAX_LOG(LogWorldSubsystemRegistry, Error, "Register '{}' — that name is already taken.",
                      InName);
            return false;
        }

        m_Entries.emplace_back(Move(InEntry));

        return true;
    }

    void WorldSubsystemRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        TDynArray<OpaaxStringID> lNames;
        for (const auto& lEntry : m_Entries) { lNames.push_back(lEntry->GetName()); }

        OPAAX_LOG(LogWorldSubsystemRegistry, Info, "Sealed with {} world subsystem type(s): {}",
                  static_cast<Uint64>(m_Entries.size()), fmt::join(lNames, ", "));
    }

    const IWorldSubsystemEntry* WorldSubsystemRegistry::FindByName(OpaaxStringID InName) const noexcept
    {
        for (const TUniquePtr<IWorldSubsystemEntry>& lEntry : m_Entries)
        {
            if (lEntry->GetName() == InName)
            {
                return lEntry.get();
            }
        }

        return nullptr;
    }
}
