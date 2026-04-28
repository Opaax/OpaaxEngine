#include "AssetManifest.h"

namespace Opaax
{
    UnorderedMap<Uint32, AssetDescriptor> AssetManifest::s_Descriptors;

    bool AssetManifest::GenerateEmpty(const char* InAbsPath) noexcept
    {
        try
        {
            // Create directories if needed
            const std::filesystem::path lPath(InAbsPath);
            std::filesystem::create_directories(lPath.parent_path());

            std::ofstream lFile(InAbsPath);
            if (!lFile.is_open())
            {
                OPAAX_CORE_ERROR("AssetManifest::GenerateEmpty — cannot create '{}'",
                    InAbsPath);
                return false;
            }

            // Write minimal valid manifest
            const nlohmann::json lEmpty = { { "assets", nlohmann::json::array() } };
            lFile << lEmpty.dump(4);

            OPAAX_CORE_INFO("AssetManifest: generated empty manifest at '{}'", InAbsPath);
            return true;
        }
        catch (const std::exception& e)
        {
            OPAAX_CORE_ERROR("AssetManifest::GenerateEmpty — exception: {}", e.what());
            return false;
        }
    }

    Int32 AssetManifest::LoadFile(const char* InAbsPath)
    {
        std::ifstream lFile(InAbsPath);

        if (!lFile.is_open())
        {
            // [NEW] Fichier absent — on le crée avec une structure vide plutôt que
            //   de crash ou warn. Utile au premier lancement ou sur une nouvelle machine.
            OPAAX_CORE_WARN("AssetManifest::LoadFile — '{}' not found, generating empty manifest.",
                InAbsPath);

            return GenerateEmpty(InAbsPath) ? 0 : -1;
        }

        nlohmann::json lRoot;
        try
        {
            lFile >> lRoot;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            OPAAX_CORE_ERROR("AssetManifest::LoadFile — parse error in '{}': {}",
                InAbsPath, e.what());
            return -1;
        }

        if (!lRoot.contains("assets") || !lRoot["assets"].is_array())
        {
            OPAAX_CORE_ERROR("AssetManifest::LoadFile — missing 'assets' array in '{}'",
                InAbsPath);
            return -1;
        }

        Int32 lCount = 0;

        for (const auto& lEntry : lRoot["assets"])
        {
            if (!lEntry.contains("id") || !lEntry.contains("path"))
            {
                OPAAX_CORE_WARN("AssetManifest::LoadFile — entry missing 'id' or 'path', skipped.");
                continue;
            }

            AssetDescriptor lDesc;
            lDesc.ID        = OpaaxStringID(lEntry["id"].get<std::string>().c_str());
            lDesc.RelPath   = OpaaxString(lEntry["path"].get<std::string>().c_str());
            lDesc.Type      = lEntry.contains("type")
                            ? OpaaxStringID(lEntry["type"].get<std::string>().c_str())
                            : OpaaxStringID("Unknown");

            const Uint32 lKey = lDesc.ID.GetId();

            // NOTE: Later manifests override earlier ones.
            //   Game manifest loaded after engine manifest — game wins on collision.
            if (s_Descriptors.count(lKey))
            {
                OPAAX_CORE_TRACE("AssetManifest: '{}' overriding existing entry.",
                    lDesc.ID);
            }

            s_Descriptors[lKey] = std::move(lDesc);
            ++lCount;
        }

        OPAAX_CORE_INFO("AssetManifest: loaded {} entry/entries from '{}'",
            lCount, InAbsPath);

        return lCount;
    }

    Int32 AssetManifest::LoadFile(const OpaaxString& InAbsPath)
    {
        return LoadFile(InAbsPath.CStr());
    }

    const AssetDescriptor* AssetManifest::Find(OpaaxStringID InID) noexcept
    {
        auto lIt = s_Descriptors.find(InID.GetId());
        return lIt != s_Descriptors.end() ? &lIt->second : nullptr;
    }

    const AssetDescriptor* AssetManifest::FindByPath(const OpaaxString& InRelPath) noexcept
    {
        for (const auto& [lKey, lDesc] : s_Descriptors)
        {
            if (lDesc.RelPath == InRelPath)
            {
                return &lDesc;
            }
        }
        return nullptr;
    }

    bool AssetManifest::Contains(OpaaxStringID InID) noexcept
    {
        return s_Descriptors.contains(InID.GetId());
    }

    void AssetManifest::Clear() noexcept
    {
        s_Descriptors.clear();
    }

    void AssetManifest::Add(AssetDescriptor&& InDesc)
    {
        const Uint32 lKey = InDesc.ID.GetId();

        // NOTE: Never overwrite existing entries — scanner preserves manual edits.
        if (s_Descriptors.count(lKey))
        {
            return;
        }

        s_Descriptors.emplace(lKey, std::move(InDesc));
    }
    
    void AssetManifest::SetMissing(OpaaxStringID InID, bool bIsMissing) noexcept
    {
        auto lIt = s_Descriptors.find(InID.GetId());
        if (lIt != s_Descriptors.end())
        {
            lIt->second.bMissing = bIsMissing;
        }
    }
}

