#include "Engine/GameInstance/GameInstanceSubsystemRegistry.h"

namespace Opaax
{
    // NOTE: every refusal is an Error log + a false return, deliberately NOT OPAAX_ASSERT — an
    // assert is a __debugbreak in Debug (untestable) and nothing in Release, which is the build
    // where a module registering late must be reported. WorldSubsystemRegistry::AddEntry, verbatim.
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

        OPAAX_LOG(LogGameInstanceSubsystemRegistry, Trace, "Registered game instance subsystem '{}' ({} total)",
                  InName, static_cast<Uint64>(m_Entries.size()));

        return true;
    }

    void GameInstanceSubsystemRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        OPAAX_LOG(LogGameInstanceSubsystemRegistry, Info, "Sealed with {} game instance subsystem type(s).",
                  static_cast<Uint64>(m_Entries.size()));
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
