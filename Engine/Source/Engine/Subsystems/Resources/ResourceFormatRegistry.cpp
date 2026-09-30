#include "ResourceFormatRegistry.h"

#include <spdlog/fmt/ranges.h>   // fmt::join

namespace Opaax
{
    // Refusals log an error and return false (not an assert, so Release builds report them too).
    bool ResourceFormatRegistry::AddEntry(Uint32 InTypeId, OpaaxStringID InName, const ResourceFormat* InFormat,
                                          ResourceAcquireFn InAcquire)
    {
        if (InFormat == nullptr || InAcquire == nullptr)
        {
            return false;
        }

        if (m_bSealed)
        {
            OPAAX_LOG(LogResourceFormatRegistry, Error,
                      "Register '{}' — registry is SEALED. Resource formats must be registered before the first world.",
                      InName);
            return false;
        }

        if (!InName.IsValid())
        {
            OPAAX_LOG(LogResourceFormatRegistry, Error, "Register — refused a resource format with an empty name.");
            return false;
        }

        if (FindByTypeId(InTypeId) != nullptr)
        {
            OPAAX_LOG(LogResourceFormatRegistry, Error, "Register '{}' — that type is already registered.", InName);
            return false;
        }

        if (InFormat->ExtensionCount == 0)
        {
            OPAAX_LOG(LogResourceFormatRegistry, Error, "Register '{}' — claims no extension, so nothing could ever match it.", InName);
            return false;
        }

        // Check every extension before claiming any: no half registration.
        TDynArray<OpaaxStringID> lExtensions;
        lExtensions.reserve(InFormat->ExtensionCount);

        for (Uint32 lIndex = 0; lIndex < InFormat->ExtensionCount; ++lIndex)
        {
            const OpaaxStringID lExtension = NormalizeExtension(InFormat->Extensions[lIndex]);

            if (!lExtension.IsValid())
            {
                OPAAX_LOG(LogResourceFormatRegistry, Error, "Register '{}' — refused an empty extension.", InName);
                return false;
            }

            if (const ResourceFormatEntry* lOwner = FindByExtension(lExtension))
            {
                // Two loaders for one extension: refuse, naming both.
                OPAAX_LOG(LogResourceFormatRegistry, Error,
                          "Register '{}' — extension '{}' is already claimed by '{}'.",
                          InName, lExtension, lOwner->Name);
                return false;
            }

            lExtensions.emplace_back(lExtension);
        }

        const Uint64 lEntryIndex = static_cast<Uint64>(m_Entries.size());
        m_Entries.emplace_back(InTypeId, InName, InFormat, InAcquire);

        for (const OpaaxStringID& lExtension : lExtensions)
        {
            m_ByExtension.emplace(lExtension.GetId(), lEntryIndex);
        }

        return true;
    }

    void ResourceFormatRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        TDynArray<OpaaxStringID> lNames;
        for (const ResourceFormatEntry& lEntry : m_Entries) { lNames.push_back(lEntry.Name); }

        OPAAX_LOG(LogResourceFormatRegistry, Info, "Sealed with {} resource format(s) over {} extension(s): {}",
                  static_cast<Uint64>(m_Entries.size()), static_cast<Uint64>(m_ByExtension.size()),
                  fmt::join(lNames, ", "));
    }

    const ResourceFormatEntry* ResourceFormatRegistry::FindByExtension(OpaaxStringID InExtension) const noexcept
    {
        if (!InExtension.IsValid())
        {
            return nullptr;
        }

        const auto lIt = m_ByExtension.find(InExtension.GetId());
        return (lIt != m_ByExtension.end()) ? &m_Entries[lIt->second] : nullptr;
    }

    const ResourceFormatEntry* ResourceFormatRegistry::FindByTypeId(Uint32 InTypeId) const noexcept
    {
        for (const ResourceFormatEntry& lEntry : m_Entries)
        {
            if (lEntry.TypeId == InTypeId)
            {
                return &lEntry;
            }
        }

        return nullptr;
    }
}
