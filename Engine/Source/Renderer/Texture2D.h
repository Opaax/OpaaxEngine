#pragma once

#include "Assets/IAsset.hpp"
#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    class ITexture2D;

    // =============================================================================
    // Texture2D
    // =============================================================================
    /**
     * @class Texture2D
     *
     * Logical 2D texture asset. Composes a UniquePtr<ITexture2D> as the GPU
     * resource — the asset surface (ID, type, state, source path) lives here,
     * the raw backend handle stays inside the concrete ITexture2D impl
     * (OpenGLTexture2D today). Never names a backend type.
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

        /**
         * Runtime-only texture from in-memory pixel bytes. Not registry-tracked.
         * Channels: 4 = RGBA8, 3 = RGB8, 1 = R8 coverage (alpha-swizzled).
         * Caller owns InData and may free it after the ctor returns.
         */
        Texture2D(const unsigned char* InData, Uint32 InWidth, Uint32 InHeight, Int32 InChannels);

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

        // The composed backend texture — for bind-group population (IBindGroup::SetTexture). Not owned by the caller.
        ITexture2D* GetRHITexture() const noexcept { return m_Gpu.get(); }

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
        UniquePtr<ITexture2D>      m_Gpu;
    };

} // namespace Opaax
