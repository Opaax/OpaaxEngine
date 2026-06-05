#include "CollisionProfile.h"

#include <fstream>

#include "Core/Log/OpaaxLog.h"

namespace Opaax
{
    // =============================================================================
    // CTOR
    // =============================================================================
    CollisionProfile::CollisionProfile(const OpaaxString& InSourcePath, OpaaxStringID InAssetID)
        : m_AssetID(InAssetID), m_SourcePath(InSourcePath)
    {
        // Default every response to Block (collide-with-all) before any file override.
        for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
        {
            m_Responses[i] = ECollisionResponse::Block;
        }

        std::ifstream lFile(m_SourcePath.CStr());
        if (!lFile.is_open())
        {
            OPAAX_CORE_ERROR("CollisionProfile: cannot open '{}'", m_SourcePath);
            m_State = EAssetState::Failed;
            return;
        }

        try
        {
            nlohmann::json lJson;
            lFile >> lJson;
            FromJson(lJson);
            m_State = EAssetState::Loaded;
            OPAAX_CORE_INFO("CollisionProfile: loaded '{}' (channel={})",
                            m_AssetID, ToStringID(m_Channel).ToString());
        }
        catch (const std::exception& InEx)
        {
            OPAAX_CORE_ERROR("CollisionProfile: parse failed for '{}' — {}", m_SourcePath, InEx.what());
            m_State = EAssetState::Failed;
        }
    }

    // =============================================================================
    // Authoring
    // =============================================================================
    ECollisionResponse CollisionProfile::GetResponse(ECollisionChannel InAgainst) const noexcept
    {
        const Uint8 lIdx = static_cast<Uint8>(InAgainst);
        if (lIdx >= static_cast<Uint8>(ECollisionChannel::Count)) { return ECollisionResponse::Ignore; }
        return m_Responses[lIdx];
    }

    void CollisionProfile::SetResponse(ECollisionChannel InAgainst, ECollisionResponse InResponse) noexcept
    {
        const Uint8 lIdx = static_cast<Uint8>(InAgainst);
        if (lIdx >= static_cast<Uint8>(ECollisionChannel::Count)) { return; }
        m_Responses[lIdx] = InResponse;
    }

    // =============================================================================
    // Filter resolution
    // =============================================================================
    Uint64 CollisionProfile::ComputeCategoryBits() const noexcept
    {
        return CategoryBit(m_Channel);
    }

    Uint64 CollisionProfile::ComputeMaskBits() const noexcept
    {
        Uint64 lMask = 0;
        for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
        {
            if (m_Responses[i] != ECollisionResponse::Ignore)
            {
                lMask |= CategoryBit(static_cast<ECollisionChannel>(i));
            }
        }
        return lMask;
    }

    // =============================================================================
    // Serialization
    // =============================================================================
    nlohmann::json CollisionProfile::ToJson() const
    {
        nlohmann::json lResponses = nlohmann::json::object();
        for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
        {
            const OpaaxString lChannelName = g_CollisionChannelIDs[i].ToString();
            lResponses[lChannelName.CStr()] = ToString(m_Responses[i]);
        }

        return {
                { "channel",   ToStringID(m_Channel).ToString().CStr() },
                { "responses", lResponses }
        };
    }

    void CollisionProfile::FromJson(const nlohmann::json& InJson)
    {
        if (InJson.contains("channel"))
        {
            m_Channel = CollisionChannelFromStringID(OpaaxStringID(InJson["channel"].get<std::string>()));
        }

        if (InJson.contains("responses"))
        {
            const nlohmann::json& lResponses = InJson["responses"];
            for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
            {
                const OpaaxString lChannelName = g_CollisionChannelIDs[i].ToString();
                if (lResponses.contains(lChannelName.CStr()))
                {
                    m_Responses[i] = CollisionResponseFromString(
                        lResponses[lChannelName.CStr()].get<std::string>().c_str());
                }
            }
        }
    }

    bool CollisionProfile::Save() const
    {
        std::ofstream lFile(m_SourcePath.CStr());
        if (!lFile.is_open())
        {
            OPAAX_CORE_ERROR("CollisionProfile::Save — cannot write '{}'", m_SourcePath);
            return false;
        }

        lFile << ToJson().dump(4);
        OPAAX_CORE_INFO("CollisionProfile: saved '{}'", m_SourcePath);
        return true;
    }

} // namespace Opaax
