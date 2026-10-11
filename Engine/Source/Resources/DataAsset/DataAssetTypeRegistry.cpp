#include "Resources/DataAsset/DataAssetTypeRegistry.h"

#include <spdlog/fmt/ranges.h>   // fmt::join

namespace Opaax
{
    bool DataAssetTypeRegistry::AddEntry(TUniquePtr<IDataAssetTypeEntry> InEntry)
    {
        if (InEntry == nullptr)
        {
            return false;
        }

        if (m_bSealed)
        {
            OPAAX_LOG(LogDataAssetTypeRegistry, Error,
                      "Register '{}' — registry is SEALED (a world already exists). Register data asset types in OnRegister.",
                      InEntry->GetName());
            return false;
        }

        // The name is what files store, so it must be unique (two namespaces, one leaf name).
        if (FindByName(InEntry->GetName()) != nullptr || FindByTypeId(InEntry->GetTypeId()) != nullptr)
        {
            OPAAX_LOG(LogDataAssetTypeRegistry, Error, "Register '{}' — that name or type is already registered.",
                      InEntry->GetName());
            return false;
        }

        m_Entries.emplace_back(Move(InEntry));
        return true;
    }

    void DataAssetTypeRegistry::Seal() noexcept
    {
        if (m_bSealed)
        {
            return;
        }

        m_bSealed = true;

        if (m_Entries.empty())
        {
            OPAAX_LOG(LogDataAssetTypeRegistry, Info, "Sealed with no data asset type");
            return;
        }

        TDynArray<OpaaxStringID> lNames;
        for (const TUniquePtr<IDataAssetTypeEntry>& lEntry : m_Entries) { lNames.push_back(lEntry->GetName()); }

        OPAAX_LOG(LogDataAssetTypeRegistry, Info, "Sealed with {} data asset type(s): {}",
                  static_cast<Uint64>(m_Entries.size()), fmt::join(lNames, ", "));
    }

    const IDataAssetTypeEntry* DataAssetTypeRegistry::FindByName(const OpaaxStringID InName) const noexcept
    {
        for (const TUniquePtr<IDataAssetTypeEntry>& lEntry : m_Entries)
        {
            if (lEntry->GetName() == InName) { return lEntry.get(); }
        }
        return nullptr;
    }

    const IDataAssetTypeEntry* DataAssetTypeRegistry::FindByTypeId(const TypeId InTypeId) const noexcept
    {
        for (const TUniquePtr<IDataAssetTypeEntry>& lEntry : m_Entries)
        {
            if (lEntry->GetTypeId() == InTypeId) { return lEntry.get(); }
        }
        return nullptr;
    }
}
