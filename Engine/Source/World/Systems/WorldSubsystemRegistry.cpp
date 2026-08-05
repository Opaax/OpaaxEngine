#include "World/Systems/WorldSubsystemRegistry.h"

namespace Opaax
{
    // NOTE: every refusal is an Error log + a false return, deliberately NOT OPAAX_ASSERT — same
    // reasoning as ComponentRegistry::AddEntry. An assert is a __debugbreak in Debug (untestable)
    // and nothing in Release, which is the build where a module registering late must be reported.
    bool WorldSubsystemRegistry::AddEntry(TUniquePtr<IWorldSubsystemEntry> InEntry, OpaaxStringID InName)
    {
        if (InEntry == nullptr)
        {
            return false;
        }

        // Sealed means a world already exists. A candidate accepted now would simply be absent
        // from that world's subsystem set, with nothing to say so.
        if (m_bSealed)
        {
            OPAAX_LOG(LogWorldSubsystemRegistry, Error,
                      "Register '{}' — registry is SEALED (a world already exists). World subsystem types must be registered before the first CreateWorld.",
                      InName.ToString().CStr());
            return false;
        }

        if (!InName.IsValid())
        {
            OPAAX_LOG(LogWorldSubsystemRegistry, Error, "Register — refused a world subsystem with an empty name.");
            return false;
        }

        // Unlike ComponentRegistry there is no second key to check: a component's TYPE id is
        // load-bearing because it maps to on-disk json, whereas a candidate is only ever
        // identified by name. Registering the same type twice under two names is refused by the
        // name check below only if the names collide — which is correct: two candidates of the
        // same type WOULD both be created, and that is the caller's stated intent.
        if (FindByName(InName) != nullptr)
        {
            OPAAX_LOG(LogWorldSubsystemRegistry, Error, "Register '{}' — that name is already taken.",
                      InName.ToString().CStr());
            return false;
        }

        m_Entries.push_back(Move(InEntry));

        OPAAX_LOG(LogWorldSubsystemRegistry, Trace, "Registered world subsystem '{}' ({} total)",
                  InName.ToString().CStr(), static_cast<Uint64>(m_Entries.size()));

        return true;
    }

    void WorldSubsystemRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        OPAAX_LOG(LogWorldSubsystemRegistry, Info, "Sealed with {} world subsystem type(s).",
                  static_cast<Uint64>(m_Entries.size()));
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
