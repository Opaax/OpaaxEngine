#pragma once

#include <nlohmann/json.hpp>

#include "Assets/IAsset.hpp"
#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

#include "Physics/Collision/CollisionChannel.h"

namespace Opaax
{
    // =============================================================================
    // ECollisionResponse
    // =============================================================================
    /**
     * @enum ECollisionResponse
     * Authored, per-channel response of a profile toward another channel (Unreal-style
     * three-state matrix). Ignore = no interaction at all; Overlap = report but no block;
     * Block = solid collision response.
     *
     * **M9 runtime mapping (pragmatic):** the *kind* of interaction (sensor vs. solid) comes
     * from the collider's Mode, not from this value — here Overlap and Block both collapse to
     * "filter bit SET" (interaction allowed) and only Ignore clears the bit. Keeping the third
     * state authored now means the future faithful dual-shape path needs zero asset migration.
     */
    enum class ECollisionResponse : Uint8
    {
        Ignore,
        Overlap,
        Block
    };

    inline const char* ToString(ECollisionResponse InResponse) noexcept
    {
        switch (InResponse)
        {
            case ECollisionResponse::Ignore:  return "Ignore";
            case ECollisionResponse::Overlap: return "Overlap";
            case ECollisionResponse::Block:   return "Block";
        }
        return "Ignore";
    }

    inline ECollisionResponse CollisionResponseFromString(const OpaaxString& InName) noexcept
    {
        if (InName == "Overlap") { return ECollisionResponse::Overlap; }
        if (InName == "Block")   { return ECollisionResponse::Block; }
        return ECollisionResponse::Ignore;
    }

    // =============================================================================
    // CollisionProfile
    // =============================================================================
    /**
     * @class CollisionProfile
     *
     * First-class collision-behaviour asset (Unreal "collision preset"): an object channel
     * plus a per-channel response matrix, authored on disk as JSON and referenced by a
     * ColliderComponent. The physics backend reads it through the neutral filter helpers —
     * it never sees Box2D types.
     *
     * Editable in memory (the editor inspector mutates it, then Save() writes back); loaded
     * via CollisionProfileLoader through the standard AssetRegistry pipeline.
     */
    class OPAAX_API CollisionProfile final : public IAsset
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        // Load from disk. State ends Loaded on a parsed file, or Failed (defaults retained)
        // if the file is missing/invalid — a Failed profile still yields a usable block-all
        // filter so a bad asset never silently disables collision.
        CollisionProfile(const OpaaxString& InSourcePath, OpaaxStringID InAssetID);

        ~CollisionProfile() override = default;

        CollisionProfile(const CollisionProfile&)            = delete;
        CollisionProfile& operator=(const CollisionProfile&) = delete;
        CollisionProfile(CollisionProfile&&)                 = delete;
        CollisionProfile& operator=(CollisionProfile&&)      = delete;

        // =============================================================================
        // IAsset interface
        // =============================================================================
        //~ Begin IAsset interface
    public:
        OpaaxStringID      GetAssetID()    const override { return m_AssetID;    }
        AssetType          GetType()       const override { return AssetType::CollisionProfile; }
        EAssetState        GetState()      const override { return m_State;      }
        const OpaaxString& GetSourcePath() const override { return m_SourcePath; }
        //~ End IAsset interface

        // =============================================================================
        // Authoring
        // =============================================================================
    public:
        ECollisionChannel  GetChannel() const noexcept                 { return m_Channel; }
        void               SetChannel(ECollisionChannel InChannel) noexcept { m_Channel = InChannel; }

        ECollisionResponse GetResponse(ECollisionChannel InAgainst) const noexcept;
        void               SetResponse(ECollisionChannel InAgainst, ECollisionResponse InResponse) noexcept;

        // =============================================================================
        // Filter resolution (consumed by the physics backend via the seam)
        // =============================================================================
    public:
        // The single category bit this profile's channel occupies.
        Uint64 ComputeCategoryBits() const noexcept;

        // OR of every channel bit whose response is not Ignore (Overlap/Block both allow).
        Uint64 ComputeMaskBits() const noexcept;

        // =============================================================================
        // Serialization
        // =============================================================================
    public:
        nlohmann::json ToJson()   const;
        void           FromJson(const nlohmann::json& InJson);

        // Write the current in-memory state back to GetSourcePath(). Returns false on IO error.
        bool           Save() const;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID      m_AssetID = {};
        OpaaxString        m_SourcePath;
        EAssetState        m_State   = EAssetState::Unloaded;

        ECollisionChannel  m_Channel = ECollisionChannel::WorldDynamic;

        // Response toward each channel, indexed by static_cast<Uint8>(ECollisionChannel).
        // Default Block so a fresh profile collides with everything (least-surprise).
        ECollisionResponse m_Responses[static_cast<Uint8>(ECollisionChannel::Count)];
    };

} // namespace Opaax
