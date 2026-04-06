#pragma once

#include "AssetRefBlock.hpp"
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
    class AssetHandle
    {
        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        AssetHandle() noexcept = default;

        // NOTE: Only AssetRegistry calls this constructor.
        AssetHandle(T* InPtr, OpaaxStringID InID, AssetRefBlock* InBlock) noexcept
            : m_Ptr(InPtr), m_ID(InID), m_Block(InBlock)
        {
            if (m_Block)
            {
                m_Block->AddRef();
            }
        }

        ~AssetHandle()
        {
            ReleaseRef();
        }

        // =============================================================================
        // Copy — increments ref count
        // =============================================================================
        AssetHandle(const AssetHandle& Other) noexcept
            : m_Ptr(Other.m_Ptr), m_ID(Other.m_ID), m_Block(Other.m_Block)
        {
            if (m_Block)
            {
                m_Block->AddRef();
            }
        }

        AssetHandle& operator=(const AssetHandle& Other) noexcept
        {
            if (this != &Other)
            {
                ReleaseRef();
                m_Ptr   = Other.m_Ptr;
                m_ID    = Other.m_ID;
                m_Block = Other.m_Block;
                if (m_Block) { m_Block->AddRef(); }
            }
            return *this;
        }

        // =============================================================================
        // Move — transfers ownership, no ref count change
        // =============================================================================
        AssetHandle(AssetHandle&& Other) noexcept
            : m_Ptr(Other.m_Ptr), m_ID(Other.m_ID), m_Block(Other.m_Block)
        {
            Other.m_Ptr   = nullptr;
            Other.m_ID    = OpaaxStringID{};
            Other.m_Block = nullptr;
        }

        AssetHandle& operator=(AssetHandle&& Other) noexcept
        {
            if (this != &Other)
            {
                ReleaseRef();
                m_Ptr         = Other.m_Ptr;
                m_ID          = Other.m_ID;
                m_Block       = Other.m_Block;
                Other.m_Ptr   = nullptr;
                Other.m_ID    = OpaaxStringID{};
                Other.m_Block = nullptr;
            }
            return *this;
        }

        // =============================================================================
        // Function
        // =============================================================================
    public:
        void Reset() noexcept
        {
            ReleaseRef();
            m_Ptr   = nullptr;
            m_ID    = OpaaxStringID{};
            m_Block = nullptr;
        }

        //------------------------------------------------------------------------------
        //  Get - Set
        
        FORCEINLINE bool           IsValid()        const noexcept { return m_Ptr != nullptr; }
        FORCEINLINE T*             Get()            const noexcept { return m_Ptr; }
        FORCEINLINE OpaaxStringID  GetID()          const noexcept { return m_ID; }
        FORCEINLINE Uint32         GetRefCount()    const noexcept { return m_Block ? m_Block->Get() : 0u; }

        // =============================================================================
        // Operators
        // =============================================================================
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

        explicit operator bool()                    const noexcept { return IsValid(); }
        bool operator==(const AssetHandle& Other)   const noexcept { return m_ID == Other.m_ID; }
        bool operator!=(const AssetHandle& Other)   const noexcept { return m_ID != Other.m_ID; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        T*             m_Ptr   = nullptr;
        OpaaxStringID  m_ID    = {};
        AssetRefBlock* m_Block = nullptr;   // non-owning — registry owns the block

        void ReleaseRef() noexcept
        {
            // NOTE: We release but never destroy — the registry checks RefCount
            //   and handles destruction. AssetHandle is never the last owner.
            if (m_Block) { m_Block->Release(); }
        }
    };

    // Convenience alias
    class OpenGLTexture2D;
    using TextureHandle = AssetHandle<OpenGLTexture2D>;

} // namespace Opaax