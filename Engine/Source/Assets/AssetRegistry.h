#pragma once

#include "AssetHandle.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/Log/OpaaxLog.h"
#include "RHI/OpenGL/OpenGLTexture2D.h"
 
#include <unordered_map>
#include <typeindex>

#include "AssetManifest.h"
#include "Core/OpaaxPath.h"
#include "Loader/AssetLoaderRegistry.h"

namespace Opaax
{
    /**
     * @class AssetRegistry
     *
     * Central owner of all runtime assets.
     * Assets are loaded on first request, cached for subsequent requests.
     * Unloaded explicitly or all at once on Shutdown().
     *
     * NOTE: Thread safety — Load/Unload are NOT thread-safe. Call from the main
     * thread only. Async loading (TODO) will require a job queue + staging area.
     *
     * Load<T>
     * Key = OpaaxStringID of the ABSOLUTE resolved path.
     * Always pass paths through OpaaxPath::ToAbsolute() before calling Load().
     * Use the OPAAX_ASSET(relative) macro at callsites — it calls ToAbsolute() for you.
     *
     * Example:
     *   AssetRegistry::Load<OpenGLTexture2D>(OPAAX_ASSET("Engine/Assets/Textures/Player.png"))
     *
     * NOTE: Never pass raw relative paths directly — the cache key would differ
     *   from the resolved key and you would get duplicate loads.
     */
    class OPAAX_API AssetRegistry
    {
        // =============================================================================
        // Friends
        // =============================================================================
        friend void* Internal::AssetRegistry_TryResolveTyped(Uint32, const std::type_index&) noexcept;

        // =============================================================================
        // Internal storage — type-erased asset entry
        // =============================================================================
    private:
        struct AssetEntry
        {
            // =============================================================================
            // Static
            // =============================================================================

            template<typename T>
            static AssetEntry Make(T* InPtr)
            {
                AssetEntry lEntry;
                lEntry.Ptr      = InPtr;
                lEntry.Type     = typeid(T);
                lEntry.Deleter  = [](void* InRaw) { delete static_cast<T*>(InRaw); };
                lEntry.Block    = new AssetRefBlock();
                lEntry.Block->AddRef(); // registry owns 1 ref; live handles add their own
                return lEntry;
            }

            // =============================================================================
            // CTOR - DTOR
            // =============================================================================
            AssetEntry() : Type(typeid(void)) {}

            ~AssetEntry()
            {
                DestroyPayload();
                if (Block)
                {
                    Block->Release();
                    Block = nullptr;
                }
            }

            // =============================================================================
            // Copy
            // =============================================================================
            AssetEntry(const AssetEntry&)               = delete;
            AssetEntry& operator=(const AssetEntry&)    = delete;

            // =============================================================================
            // Move
            // =============================================================================
            AssetEntry(AssetEntry&& Other) noexcept
                : Ptr(Other.Ptr), Type(Other.Type), Deleter(Other.Deleter), Block(Other.Block)
            {
                Other.Ptr     = nullptr;
                Other.Deleter = nullptr;
                Other.Block   = nullptr;
            }

            AssetEntry& operator=(AssetEntry&& Other) noexcept
            {
                if (this != &Other)
                {
                    DestroyPayload();
                    if (Block) { Block->Release(); }
                    Ptr           = Other.Ptr;
                    Type          = Other.Type;
                    Deleter       = Other.Deleter;
                    Block         = Other.Block;
                    Other.Ptr     = nullptr;
                    Other.Deleter = nullptr;
                    Other.Block   = nullptr;
                }
                return *this;
            }

            // =============================================================================
            // Function
            // =============================================================================
        public:
            // Live HANDLE refs only (registry's own ref is excluded).
            FORCEINLINE Uint32 LiveHandleCount() const noexcept
            {
                if (!Block) { return 0; }
                const Uint32 lTotal = Block->Get();
                return (lTotal > 0) ? (lTotal - 1) : 0;
            }

        private:
            void DestroyPayload() noexcept
            {
                if (Ptr && Deleter)
                {
                    Deleter(Ptr);
                }
                Ptr     = nullptr;
                Deleter = nullptr;
            }

            // =============================================================================
            // Members
            // =============================================================================
        public:
            void*           Ptr     = nullptr;
            std::type_index Type;
            void(*Deleter)(void*)   = nullptr;
            // Intrusive ref-counted block (heap, self-deletes on last Release).
            // Entry holds 1 ref from Make() through ~AssetEntry / move-from.
            AssetRefBlock*  Block   = nullptr;
        };

        // =============================================================================
        // Function - Static
        // =============================================================================
    private:
        /**
         * 
         * @param InID 
         * @return 
         */
        static OpaaxString ResolveToAbsPath(OpaaxStringID InID)
        {
            const OpaaxString lIDStr = InID.ToString();

            if (lIDStr.IsEmpty()) { return OpaaxString(); }

            // Step 1 — already absolute
            if (OpaaxPath::IsAbsolutePath(lIDStr))
            {
                return lIDStr;
            }

            // Step 2 — manifest lookup
            // NOTE: Find() is O(1) — Uint32 hash map lookup, no string comparison.
            const AssetDescriptor* lDesc = AssetManifest::Find(InID);
            if (lDesc)
            {
                return OpaaxPath::ToAbsolute(lDesc->RelPath);
            }

            // Step 3 — direct path fallback — direct project-relative path without a manifest entry.
            return OpaaxPath::ToAbsolute(lIDStr);
        }
        
    public:
        /**
         * Load<T>
         * If the asset is already cached, returns the existing handle immediately.
         * If not, constructs a new T using InPath as the file path.
         * @tparam T must be constructible from (const char*).
         * @param InID 
         * @return handle to the asset identified by InID.
         */
        template <typename T>
        static TAssetHandle<T> Load(OpaaxStringID InID)
        {
            const OpaaxString lAbsPath = ResolveToAbsPath(InID);

            if (lAbsPath.IsEmpty())
            {
                OPAAX_CORE_ERROR("AssetRegistry::Load — could not resolve '{}'", InID);
                return TAssetHandle<T>{};
            }

            // Key on the resolved absolute path — consistent regardless of
            // whether InID was a logical name or a direct path.
            const OpaaxStringID lResolvedID(lAbsPath);
            const Uint32        lKey = lResolvedID.GetId();

            // --- Cache hit ---
            auto lIt = s_Assets.find(lKey);
            if (lIt != s_Assets.end())
            {
                if (lIt->second.Type != typeid(T))
                {
                    OPAAX_CORE_ERROR("AssetRegistry::Load — type mismatch for '{}'", InID);
                    return TAssetHandle<T>{};
                }
                return TAssetHandle<T>{ lResolvedID, lIt->second.Block };
            }

            // --- Cache miss — delegate to loader ---
            IAssetLoader<T>* lLoader = AssetLoaderRegistry::Get<T>();

            if (!lLoader)
            {
                OPAAX_CORE_ERROR(
                    "AssetRegistry::Load — no loader registered for type '{}'. "
                    "Call AssetLoaderRegistry::Register<T>() at startup.",
                    typeid(T).name());
                return TAssetHandle<T>{};
            }

            T* lAsset = lLoader->Load(lAbsPath.CStr());

            if (!lLoader->IsValid(lAsset))
            {
                OPAAX_CORE_ERROR("AssetRegistry::Load — loader failed for '{}'", lAbsPath);
                delete lAsset;
                return TAssetHandle<T>{};
            }

            auto& lEntry = s_Assets.emplace(lKey, AssetEntry::Make(lAsset))
                                    .first->second;

            OPAAX_CORE_INFO("AssetRegistry: loaded '{}' as '{}'", lAbsPath, InID);

            return TAssetHandle<T>{ lResolvedID, lEntry.Block };
        }

        /**
         * Forces a reload from disk for an already-cached asset.
         * Live TAssetHandles begin resolving to the new payload on the next Get();
         * the old AssetRefBlock survives in memory until its last handle releases
         * (intrusive RC self-delete), so handle dtors stay safe.
         * @tparam T
         * @param InID
         * @return
         */
        template<typename T>
        static TAssetHandle<T> Reload(OpaaxStringID InID)
        {
            const OpaaxString lAbsPath  = ResolveToAbsPath(InID);
            const Uint32      lKey      = OpaaxStringID(lAbsPath).GetId();

            auto lIt = s_Assets.find(lKey);
            if (lIt != s_Assets.end())
            {
                const Uint32 lLiveHandles = lIt->second.LiveHandleCount();
                if (lLiveHandles > 0)
                {
                    OPAAX_CORE_WARN(
                        "AssetRegistry::Reload — '{}' has {} live handle(s). "
                        "They will resolve to the new asset on next Get().",
                        InID, lLiveHandles);
                }
                s_Assets.erase(lIt);
            }

            OPAAX_CORE_INFO("AssetRegistry::Reload — reloading '{}'", InID);
            return Load<T>(InID);
        }

        /**
         * Unload
         * Removes the asset from the cache. Live TAssetHandles will report invalid
         * (resolver miss) on next Get(). The underlying AssetRefBlock survives until
         * the last handle releases — handle dtors remain safe to call.
         * @param InID
         */
        static void Unload(OpaaxStringID InID)
        {
            const OpaaxString lAbsPath = ResolveToAbsPath(InID);
            const Uint32      lKey     = OpaaxStringID(lAbsPath).GetId();

            auto lIt = s_Assets.find(lKey);
            if (lIt == s_Assets.end())
            {
                OPAAX_CORE_WARN("AssetRegistry::Unload — '{}' not found", InID);
                return;
            }

            const Uint32 lLiveHandles = lIt->second.LiveHandleCount();
            if (lLiveHandles > 0)
            {
                OPAAX_CORE_WARN("AssetRegistry::Unload — '{}' has {} live handle(s); resolver will return null until they drop.",
                    InID, lLiveHandles);
            }

            s_Assets.erase(lIt);
            OPAAX_CORE_INFO("AssetRegistry: unloaded '{}'", InID);
        }

        static bool IsLoaded(OpaaxStringID InID)
        {
            const OpaaxString lAbsPath = ResolveToAbsPath(InID);
            return s_Assets.count(OpaaxStringID(lAbsPath).GetId()) > 0;
            //return s_Assets.count(InID.GetId()) > 0;
        }

        /**
         * Shutdown — drops the registry's ref on every asset.
         * Surviving handles keep their AssetRefBlock alive (intrusive RC) so their
         * dtors stay safe; the resolver returns null for them after this point.
         */
        static void Shutdown()
        {
            OPAAX_CORE_INFO("AssetRegistry::Shutdown() — {} asset(s)", s_Assets.size());

            for (auto& [lKey, lEntry] : s_Assets)
            {
                const Uint32 lLiveHandles = lEntry.LiveHandleCount();
                if (lLiveHandles > 0)
                {
                    OPAAX_CORE_WARN("AssetRegistry::Shutdown — asset {} still has {} live handle(s); their next Get() will return null.",
                        lKey, lLiveHandles);
                }
            }
            s_Assets.clear();

            AssetLoaderRegistry::Shutdown();
            AssetManifest::Clear();
        }

        static const UnorderedMap<Uint32, AssetEntry>& GetAssets() noexcept
        {
            return s_Assets;
        }
         
        // TODO: async loading interface
        //   static void LoadAsync<T>(OpaaxStringID InID, TFunction<void(TAssetHandle<T>)> InCallback);
        //   static void ProcessPendingLoads();   // call once per frame from RenderSubsystem
 
        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Uint32 key = OpaaxStringID::GetId() — avoids hashing a string at lookup time
        static UnorderedMap<Uint32, AssetEntry> s_Assets;
    };
} // namespace Opaax
