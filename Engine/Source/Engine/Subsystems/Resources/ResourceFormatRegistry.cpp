#include "ResourceFormatRegistry.h"

namespace Opaax
{
    // NOTE: every refusal below is an Error log + a false return, never OPAAX_ASSERT — the same
    // ruling ComponentRegistry::AddEntry carries, for the same reasons (a debugbreak is untestable
    // in Debug and compiles away in Release, which is the build where a late registration matters).
    bool ResourceFormatRegistry::AddEntry(Uint32 InTypeId, OpaaxStringID InName, const ResourceFormat* InFormat)
    {
        if (InFormat == nullptr)
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

        // Normalize and check EVERY extension before claiming any: a type that half-registers would
        // leave the table describing a loader that was refused.
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
                // Two loaders for one extension means the answer to "what opens this file?" depends
                // on registration order. Refuse, and name both sides so the fix is obvious.
                OPAAX_LOG(LogResourceFormatRegistry, Error,
                          "Register '{}' — extension '{}' is already claimed by '{}'.",
                          InName, lExtension, lOwner->Name);
                return false;
            }

            lExtensions.emplace_back(lExtension);
        }

        const Uint64 lEntryIndex = static_cast<Uint64>(m_Entries.size());
        m_Entries.emplace_back(InTypeId, InName, InFormat);

        for (const OpaaxStringID& lExtension : lExtensions)
        {
            m_ByExtension.emplace(lExtension.GetId(), lEntryIndex);
        }

        OPAAX_LOG(LogResourceFormatRegistry, Trace, "Registered resource format '{}' ({} extension(s), {} total)",
                  InName, InFormat->ExtensionCount, static_cast<Uint64>(m_Entries.size()));

        return true;
    }

    void ResourceFormatRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        OPAAX_LOG(LogResourceFormatRegistry, Info, "Sealed with {} resource format(s) over {} extension(s).",
                  static_cast<Uint64>(m_Entries.size()), static_cast<Uint64>(m_ByExtension.size()));
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
