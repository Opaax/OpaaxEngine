#include "World/Components/ComponentRegistry.h"

#include <spdlog/fmt/ranges.h>   // fmt::join

namespace Opaax
{
    // Refusals log an error and return false (not an assert, so Release builds report them too).
    bool ComponentRegistry::AddEntry(TUniquePtr<IComponentEntry> InEntry, entt::id_type InTypeId)
    {
        if (InEntry == nullptr)
        {
            return false;
        }

        // Sealed: a world exists, and a type added now would be missing from it.
        if (m_bSealed)
        {
            OPAAX_LOG(LogComponentRegistry, Error,
                      "Register '{}' — registry is SEALED (a world already exists). Component types must be registered before the first CreateWorld.",
                      InEntry->GetName());
            return false;
        }

        const OpaaxStringID lName = InEntry->GetName();

        if (!lName.IsValid())
        {
            OPAAX_LOG(LogComponentRegistry, Error, "Register — refused a component with an empty name.");
            return false;
        }

        // The name is saved in map files: it must be unique.
        if (FindByName(lName) != nullptr)
        {
            OPAAX_LOG(LogComponentRegistry, Error, "Register '{}' — that name is already taken.",
                      lName);
            return false;
        }

        // One type under two names would give it two saved forms.
        if (FindByTypeId(InTypeId) != nullptr)
        {
            OPAAX_LOG(LogComponentRegistry, Error, "Register '{}' — that type is already registered.",
                      lName);
            return false;
        }

        m_Entries.emplace_back(Move(InEntry));

        return true;
    }

    void ComponentRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        TDynArray<OpaaxStringID> lNames;
        for (const TUniquePtr<IComponentEntry>& lEntry : m_Entries) { lNames.push_back(lEntry->GetName()); }

        OPAAX_LOG(LogComponentRegistry, Info, "Sealed with {} component type(s): {}",
                  static_cast<Uint64>(m_Entries.size()), fmt::join(lNames, ", "));
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
