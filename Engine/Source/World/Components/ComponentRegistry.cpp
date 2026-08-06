#include "World/Components/ComponentRegistry.h"

namespace Opaax
{
    // NOTE: every refusal below is an Error log + a false return, deliberately NOT OPAAX_ASSERT.
    // OPAAX_ASSERT is a __debugbreak (EngineAPI.h) — it takes the process down in Debug, which
    // makes these paths untestable, and it compiles to NOTHING in Release, which is exactly the
    // build where a game module registering late needs to be reported. An Error log is loud in
    // both, and the false return lets the caller decide.
    bool ComponentRegistry::AddEntry(TUniquePtr<IComponentEntry> InEntry, entt::id_type InTypeId)
    {
        if (InEntry == nullptr)
        {
            return false;
        }

        // Sealed means a world already exists. A type accepted now would be absent from every
        // entity in that world without a single error — refuse loudly instead (Editor.md §3 L1).
        if (m_bSealed)
        {
            OPAAX_LOG(LogComponentRegistry, Error,
                      "Register '{}' — registry is SEALED (a world already exists). Component types must be registered before the first CreateWorld.",
                      InEntry->GetName().ToString().CStr());
            return false;
        }

        const OpaaxStringID lName = InEntry->GetName();

        if (!lName.IsValid())
        {
            OPAAX_LOG(LogComponentRegistry, Error, "Register — refused a component with an empty name.");
            return false;
        }

        // The name is the on-disk key: a duplicate would make a map file ambiguous.
        if (FindByName(lName) != nullptr)
        {
            OPAAX_LOG(LogComponentRegistry, Error, "Register '{}' — that name is already taken.",
                      lName.ToString().CStr());
            return false;
        }

        // Re-registering the same TYPE under a second name would give one component two
        // serialized forms; the round trip could then pick either.
        if (FindByTypeId(InTypeId) != nullptr)
        {
            OPAAX_LOG(LogComponentRegistry, Error, "Register '{}' — that type is already registered.",
                      lName.ToString().CStr());
            return false;
        }

        m_Entries.push_back(Move(InEntry));

        OPAAX_LOG(LogComponentRegistry, Trace, "Registered component '{}' ({} total)",
                  lName.ToString().CStr(), static_cast<Uint64>(m_Entries.size()));

        return true;
    }

    void ComponentRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        OPAAX_LOG(LogComponentRegistry, Info, "Sealed with {} component type(s).",
                  static_cast<Uint64>(m_Entries.size()));
    }

    const IComponentEntry* ComponentRegistry::FindByName(OpaaxStringID InName) const noexcept
    {
        for (const TUniquePtr<IComponentEntry>& lEntry : m_Entries)
        {
            if (lEntry->GetName() == InName)
            {
                return lEntry.get();
            }
        }

        return nullptr;
    }

    const IComponentEntry* ComponentRegistry::FindByTypeId(entt::id_type InTypeId) const noexcept
    {
        for (const TUniquePtr<IComponentEntry>& lEntry : m_Entries)
        {
            if (lEntry->GetTypeId() == InTypeId)
            {
                return lEntry.get();
            }
        }

        return nullptr;
    }
}
