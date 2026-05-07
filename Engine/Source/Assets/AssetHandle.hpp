#pragma once

#include "AssetRefBlock.hpp"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"

#include <typeindex>
#include <typeinfo>

namespace Opaax
{
    // =============================================================================
    // Internal — registry resolver entry point
    // =============================================================================
    namespace Internal
    {
        /**
         * Returns the live payload pointer for InKey if the cached entry's runtime
         * type matches InExpected; nullptr otherwise (missing key, type mismatch,
         * or entry destroyed but still mapped).
         * Defined in AssetRegistry.cpp; declared here to avoid the
         * AssetRegistry.h <-> AssetHandle.hpp include cycle.
         */
        OPAAX_API void* AssetRegistry_TryResolveTyped(Uint32 InKey, const std::type_index& InExpected) noexcept;
    }

    // =============================================================================
    // TAssetHandle
    // =============================================================================
    /**
     * @class TAssetHandle
     *
     * Type-safe, ID-keyed reference to an asset owned by AssetRegistry.
     * Storage = OpaaxStringID + AssetRefBlock* — no cached payload pointer.
     * Every Get() resolves through the registry, so the handle correctly reports
     * invalid after Unload / type-mismatch / Shutdown without manual cleanup.
     *
     * Implicit-conversion policy: explicit Get() / IsValid() / operator-> / operator*
     * only. There is intentionally no operator T* — call sites must opt in to
     * payload access through Get(), making "is the asset live right now?" a
     * conscious check rather than an implicit one.
     *
     *  TAssetHandle<OpenGLTexture2D> lHandle =
     *      AssetRegistry::Load<OpenGLTexture2D>(OPAAX_ID("Engine/Assets/Textures/Player.png"));
     *  if (lHandle.IsValid()) Renderer2D::DrawSprite(pos, size, lHandle);
     *
     * The referenced asset and its AssetRefBlock are owned by AssetRegistry.
     *  Do NOT store TAssetHandle past the lifetime of the registry.
     *  Do NOT delete the pointer obtained from Get().
     */
    template<typename T>
    class TAssetHandle
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        TAssetHandle() noexcept = default;

        TAssetHandle(OpaaxStringID InID, AssetRefBlock* InBlock) noexcept
            : m_ID(InID), m_Block(InBlock)
        {
            if (m_Block)
            {
                m_Block->AddRef();
            }
        }

        ~TAssetHandle()
        {
            ReleaseRef();
        }

        // =============================================================================
        // Copy
        // =============================================================================
        TAssetHandle(const TAssetHandle& Other) noexcept
            : m_ID(Other.m_ID), m_Block(Other.m_Block)
        {
            if (m_Block)
            {
                m_Block->AddRef();
            }
        }

        TAssetHandle& operator=(const TAssetHandle& Other) noexcept
        {
            if (this != &Other)
            {
                ReleaseRef();
                m_ID    = Other.m_ID;
                m_Block = Other.m_Block;
                if (m_Block) { m_Block->AddRef(); }
            }
            return *this;
        }

        // =============================================================================
        // Move
        // =============================================================================
        TAssetHandle(TAssetHandle&& Other) noexcept
            : m_ID(Other.m_ID), m_Block(Other.m_Block)
        {
            Other.m_ID    = OpaaxStringID{};
            Other.m_Block = nullptr;
        }

        TAssetHandle& operator=(TAssetHandle&& Other) noexcept
        {
            if (this != &Other)
            {
                ReleaseRef();
                m_ID          = Other.m_ID;
                m_Block       = Other.m_Block;
                Other.m_ID    = OpaaxStringID{};
                Other.m_Block = nullptr;
            }
            return *this;
        }

        // =============================================================================
        // Operators
        // =============================================================================
    public:
        T* operator->() const noexcept
        {
            T* lPtr = Get();
            OPAAX_CORE_ASSERT(lPtr)
            return lPtr;
        }

        T& operator*() const noexcept
        {
            T* lPtr = Get();
            OPAAX_CORE_ASSERT(lPtr)
            return *lPtr;
        }

        explicit operator bool()                      const noexcept { return IsValid(); }
        bool operator==(const TAssetHandle& Other)    const noexcept { return m_ID == Other.m_ID; }
        bool operator!=(const TAssetHandle& Other)    const noexcept { return m_ID != Other.m_ID; }

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void Reset() noexcept
        {
            ReleaseRef();
            m_ID    = OpaaxStringID{};
            m_Block = nullptr;
        }

        //------------------------------------------------------------------------------
        //  Get - Set

        FORCEINLINE T*             Get()         const noexcept
        {
            return static_cast<T*>(Internal::AssetRegistry_TryResolveTyped(m_ID.GetId(), typeid(T)));
        }
        FORCEINLINE bool           IsValid()     const noexcept { return Get() != nullptr; }
        FORCEINLINE OpaaxStringID  GetID()       const noexcept { return m_ID; }
        FORCEINLINE Uint32         GetRefCount() const noexcept { return m_Block ? m_Block->Get() : 0u; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID  m_ID    = {};
        AssetRefBlock* m_Block = nullptr;

        void ReleaseRef() noexcept
        {
            if (m_Block) { m_Block->Release(); }
        }
    };

    // =============================================================================
    // Aliases
    // =============================================================================
    class OpenGLTexture2D;
    using TextureHandle = TAssetHandle<OpenGLTexture2D>;

} // namespace Opaax
