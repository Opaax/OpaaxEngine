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
        // CTORs - DTOR
        // =============================================================================
        AssetHandle() noexcept = default;
 
        AssetHandle(T* InPtr, OpaaxStringID InID) noexcept
            : m_Ptr(InPtr), m_ID(InID)
        {
            AddRef();
        }

        ~AssetHandle()
        {
            Release();
        }

        // =============================================================================
        // Copy - cheap, both handles point to the same registry-owned asset
        // =============================================================================
        AssetHandle(const AssetHandle& Other)noexcept
            : m_Ptr(Other.m_Ptr), m_ID(Other.m_ID)
        {
            AddRef();
        }
        
        AssetHandle& operator=(const AssetHandle& Other) noexcept
        {
            if (this != &Other)
            {
                Release();
                m_Ptr = Other.m_Ptr;
                m_ID  = Other.m_ID;
                AddRef();
            }
            return *this;
        }
 
        // =============================================================================
        // Move
        // =============================================================================
        AssetHandle(AssetHandle&& Other) noexcept
        {
            Other.m_Ptr = nullptr;
            Other.m_ID  = OpaaxStringID{};
        }
        
        AssetHandle& operator=(AssetHandle&& Other) noexcept
        {
            if (this != &Other)
            {
                Release();
                m_Ptr       = Other.m_Ptr;
                m_ID        = Other.m_ID;
                Other.m_Ptr = nullptr;
                Other.m_ID  = OpaaxStringID{};
            }
            return *this;
        }
 
        // =============================================================================
        // Function
        // =============================================================================
    private:
        // NOTE: AddRef/Release are no-ops on null handles.
        //   They forward into AssetRegistry — the handle itself stores no count.

        /**
         * AddRef is no-ops on null handles.
         * Forward into AssetRegistry — the handle itself stores no count.
         */
        void AddRef() noexcept;

        /**
         * Release is no-ops on null handles.
         * Forward into AssetRegistry — the handle itself stores no count.
         */
        void Release() noexcept;
        
    public:
        void Reset() noexcept
        {
            Release();
            m_Ptr = nullptr;
            m_ID  = OpaaxStringID{};
        }
        
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

    // Forward — implementation is in AssetRegistry.h after AssetRegistry is defined.
    // Convenience alias for the most common case
    // FIXME: replace OpenGLTexture2D with a backend-agnostic Texture2D handle
    //   when the RHI abstraction is extended to textures.
    class OpenGLTexture2D;
    using TextureHandle = AssetHandle<OpenGLTexture2D>;
 
} // namespace Opaax