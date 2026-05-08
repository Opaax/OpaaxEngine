#pragma once

#include "AssetHandle.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/Log/OpaaxLog.h"
#include "Renderer/Texture2D.h"
 
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
     * Cache key = canonical asset ID, normalized from the input:
     *   - Manifest logical ID    → kept as-is.
     *   - Project-relative path  → swapped to the manifest's logical ID if the
     *                              manifest has an entry for that path; otherwise
     *                              kept as the relative path.
     *   - Absolute path          → reduced to project-relative first, then the
     *                              same swap as above.
     * After normalization, every spelling that points at the same file collapses
     * to one cache entry, and handle.GetID() == asset.GetAssetID() == cache key.
     *
     * Example:
     *   AssetRegistry::Load<Texture2D>(OpaaxStringID("Textures/Player"))
     *   AssetRegistry::Load<Texture2D>(OPAAX_ASSET("Engine/Assets/Textures/Player.png"))
     *   // Both resolve to the same entry when the manifest maps "Textures/Player"
     *   // to "Engine/Assets/Textures/Player.png".
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
    public:
        /**
         * Result of normalizing a caller-supplied asset ID.
         * CanonicalID  — the registry's cache key + the value stamped on handles
         *                and on the asset (asset.GetAssetID()). Stable across
         *                machines because it never embeds an absolute path.
         * AbsPath      — the resolved disk path the loader actually reads from.
         *                Empty when the input was empty.
         */
        struct NormalizedAsset
        {
            OpaaxStringID CanonicalID;
            OpaaxString   AbsPath;
        };

    private:
        /**
         * Normalize a caller-supplied ID into (canonical ID, absolute path).
         * Three input shapes are accepted; all collapse to the manifest's logical
         * ID when one exists for the target file:
         *   1. logical ID      ("Textures/Player")          → manifest hit by ID.
         *   2. project-rel path ("Engine/Assets/Textures/Player.png")
         *                                                   → manifest hit by path,
         *                                                     else kept verbatim.
         *   3. absolute path   reduced to (2) before lookup.
         */
        static NormalizedAsset Normalize(OpaaxStringID InID)
        {
            NormalizedAsset lOut;

            const OpaaxString lIDStr = InID.ToString();
            if (lIDStr.IsEmpty()) { return lOut; }

            // Case 1 — ID is already a manifest logical key.
            if (const AssetDescriptor* lDesc = AssetManifest::Find(InID))
            {
                lOut.CanonicalID = InID;
                lOut.AbsPath     = OpaaxPath::ToAbsolute(lDesc->RelPath);
                return lOut;
            }

            // Reduce abs → project-relative so manifest path lookup can match.
            const OpaaxString lRelPath = OpaaxPath::IsAbsolutePath(lIDStr)
                                       ? OpaaxPath::ToProjectRelative(lIDStr)
                                       : lIDStr;

            // NOTE: If two manifest entries share the same path, FindByPath returns
            // whichever the unordered_map iteration hits first — that's a manifest
            // validation issue, not a registry concern.
            if (const AssetDescriptor* lDesc = AssetManifest::FindByPath(lRelPath))
            {
                lOut.CanonicalID = lDesc->ID;
                lOut.AbsPath     = OpaaxPath::ToAbsolute(lDesc->RelPath);
                return lOut;
            }

            // Direct-path fallback — no manifest entry. The project-relative path
            // becomes the canonical ID so subsequent Loads collide on the cache.
            lOut.CanonicalID = OpaaxStringID(lRelPath);
            lOut.AbsPath     = OpaaxPath::ToAbsolute(lRelPath);
            return lOut;
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
            const NormalizedAsset lNorm = Normalize(InID);

            if (lNorm.AbsPath.IsEmpty())
            {
                OPAAX_CORE_ERROR("AssetRegistry::Load — could not resolve '{}'", InID);
                return TAssetHandle<T>{};
            }

            const Uint32 lKey = lNorm.CanonicalID.GetId();

            // --- Cache hit ---
            auto lIt = s_Assets.find(lKey);
            if (lIt != s_Assets.end())
            {
                if (lIt->second.Type != typeid(T))
                {
                    OPAAX_CORE_ERROR("AssetRegistry::Load — type mismatch for '{}'", InID);
                    return TAssetHandle<T>{};
                }
                return TAssetHandle<T>{ lNorm.CanonicalID, lIt->second.Block };
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

            T* lAsset = lLoader->Load(lNorm.AbsPath.CStr(), lNorm.CanonicalID);

            if (!lLoader->IsValid(lAsset))
            {
                OPAAX_CORE_ERROR("AssetRegistry::Load — loader failed for '{}'", lNorm.AbsPath);
                delete lAsset;
                return TAssetHandle<T>{};
            }

            auto& lEntry = s_Assets.emplace(lKey, AssetEntry::Make(lAsset))
                                    .first->second;

            OPAAX_CORE_INFO("AssetRegistry: loaded '{}' as '{}'", lNorm.AbsPath, lNorm.CanonicalID);

            return TAssetHandle<T>{ lNorm.CanonicalID, lEntry.Block };
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
            const NormalizedAsset lNorm = Normalize(InID);
            const Uint32          lKey  = lNorm.CanonicalID.GetId();

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
            const NormalizedAsset lNorm = Normalize(InID);
            const Uint32          lKey  = lNorm.CanonicalID.GetId();

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
            const NormalizedAsset lNorm = Normalize(InID);
            return s_Assets.count(lNorm.CanonicalID.GetId()) > 0;
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
