#pragma once

#include "Assets/IAsset.hpp"
#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class OpenGLTexture2D;

    // =============================================================================
    // Texture2D
    // =============================================================================
    /**
     * @class Texture2D
     *
     * Logical 2D texture asset. Composes a UniquePtr<OpenGLTexture2D> as the
     * GPU resource — the asset surface (ID, type, state, source path) lives
     * here, the raw GL handle stays inside OpenGLTexture2D.
     *
     * No RHI base interface yet: when a second backend appears, lift the GPU
     * member to UniquePtr<RHITexture> and have OpenGLTexture2D implement it.
     *
     * Lifetime: heap-allocated by TextureLoader, owned by AssetRegistry's
     * type-erased entry. Never moved or copied — deleted special members
     * surface accidental misuse at compile time.
     */
    class OPAAX_API Texture2D final : public IAsset
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /**
         * Disk-loaded texture. Constructs the GPU object via stb_image inside
         * OpenGLTexture2D's path ctor. State ends as Loaded on success or Failed
         * if the GPU upload could not complete.
         * @param InSourcePath Project-root-relative path; stored verbatim on the asset.
         * @param InAssetID    Registry-stable logical ID.
         */
        Texture2D(const OpaaxString& InSourcePath, OpaaxStringID InAssetID);

        /**
         * Runtime-only solid-colour 1x1 texture. Not registry-tracked — used by
         * Renderer2D as the white pixel for tinted untextured quads.
         */
        Texture2D(Uint32 InWidth, Uint32 InHeight);

        ~Texture2D() override;

        // =============================================================================
        // Copy - delete
        // =============================================================================
        Texture2D(const Texture2D&)            = delete;
        Texture2D& operator=(const Texture2D&) = delete;

        // =============================================================================
        // Move - delete
        // =============================================================================
        Texture2D(Texture2D&&)                 = delete;
        Texture2D& operator=(Texture2D&&)      = delete;

        // =============================================================================
        // Functions
        // =============================================================================
        //~ Begin IAsset interface
    public:
        OpaaxStringID      GetAssetID()    const override { return m_AssetID;    }
        AssetType          GetType()       const override { return AssetType::Texture2D; }
        EAssetState        GetState()      const override { return m_State;      }
        const OpaaxString& GetSourcePath() const override { return m_SourcePath; }
        //~ End IAsset interface

    public:
        void Bind(Uint32 InSlot = 0) const;
        void Unbind()                const;

        //------------------------------------------------------------------------------
        //  Get - Set

        Uint32 GetWidth()      const noexcept;
        Uint32 GetHeight()     const noexcept;
        Uint32 GetRendererID() const noexcept;
        bool   IsLoaded()      const noexcept;

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID              m_AssetID    = {};
        OpaaxString                m_SourcePath;
        EAssetState                m_State      = EAssetState::Unloaded;
        UniquePtr<OpenGLTexture2D> m_Gpu;
    };

} // namespace Opaax
