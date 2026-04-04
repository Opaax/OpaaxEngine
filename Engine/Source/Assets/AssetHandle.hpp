#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"
 
namespace Opaax
{
    /**
     * @struct AssetHandle
     *
     * Typed, non-owning reference to an asset loaded by AssetRegistry.
     * Cheap to copy — it is just a pointer + a StringID (one Uint32).
     *
     * The referenced asset is owned by AssetRegistry.
     *  Do NOT store AssetHandle past the lifetime of the registry.
     *  Do NOT delete the pointer obtained from Get().
     *
     *  AssetHandle<OpenGLTexture2D> lHandle = AssetRegistry::Load<OpenGLTexture2D>(OPAAX_ID("Textures/Player.png"));
     *
     * if (lHandle.IsValid()) Renderer2D::DrawSprite(pos, size, lHandle);
     *
     * 
     * @tparam T 
     */
    template<typename T>
    struct AssetHandle
    {
        // =============================================================================
        // CTORs
        // =============================================================================
        AssetHandle() noexcept = default;
 
        AssetHandle(T* InPtr, OpaaxStringID InID) noexcept
            : m_Ptr(InPtr), m_ID(InID)
        {}

        // =============================================================================
        // Copy - cheap, both handles point to the same registry-owned asset
        // =============================================================================
        AssetHandle(const AssetHandle&)            = default;
        AssetHandle& operator=(const AssetHandle&) = default;
 
        // =============================================================================
        // Move
        // =============================================================================
        AssetHandle(AssetHandle&&)            noexcept = default;
        AssetHandle& operator=(AssetHandle&&) noexcept = default;
 
        // =============================================================================
        // Function
        // =============================================================================
    public:
        //------------------------------------------------------------------------------
        //  Get - Set
        FORCEINLINE bool             IsValid()  const noexcept { return m_Ptr != nullptr; }
        FORCEINLINE T*               Get()      const noexcept { return m_Ptr; }
        FORCEINLINE OpaaxStringID    GetID()    const noexcept { return m_ID;  }
 
        // =============================================================================
        // Operator
        // =============================================================================
    public:
        T* operator->() const noexcept
        {
            OPAAX_CORE_ASSERT(m_Ptr)
            return m_Ptr;
        }
 
        T& operator*() const noexcept
        {
            OPAAX_CORE_ASSERT(m_Ptr)
            return *m_Ptr;
        }
 
        explicit operator bool() const noexcept { return IsValid(); }
 
        bool operator==(const AssetHandle& Other) const noexcept { return m_ID == Other.m_ID; }
        bool operator!=(const AssetHandle& Other) const noexcept { return m_ID != Other.m_ID; }
 
        // =============================================================================
        // Members
        // =============================================================================
    private:
        T*            m_Ptr = nullptr;   // non-owning — registry owns the asset
        OpaaxStringID m_ID;              // stable identity key
    };
 
    // Convenience alias for the most common case
    // FIXME: replace OpenGLTexture2D with a backend-agnostic Texture2D handle
    //   when the RHI abstraction is extended to textures.
    class OpenGLTexture2D;
    using TextureHandle = AssetHandle<OpenGLTexture2D>;
 
} // namespace Opaax