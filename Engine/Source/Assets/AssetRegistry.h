#pragma once

#include "AssetHandle.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxString.hpp"
#include "Core/Log/OpaaxLog.h"
#include "RHI/OpenGL/OpenGLTexture2D.h"
 
#include <unordered_map>
#include <typeindex>

#include "Core/OpaaxPath.h"

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
     */
    class OPAAX_API AssetRegistry
    {
        // =============================================================================
        // Internal storage — type-erased asset entry
        // =============================================================================
    private:
        struct AssetEntry
        {
            // =============================================================================
            // Static 
            // =============================================================================

            /**
             * 
             * @tparam T 
             * @param InPtr 
             * @return 
             */
            template<typename T>
            static AssetEntry Make(T* InPtr)
            {
                AssetEntry lEntry;
                lEntry.Ptr      = InPtr;
                lEntry.Type     = typeid(T);
                lEntry.Deleter  = [](void* InRaw) { delete static_cast<T*>(InRaw); };
                lEntry.Block    = MakeUnique<AssetRefBlock>();
                
                return lEntry;
            }
            
            // =============================================================================
            // CTOR 
            // =============================================================================
            AssetEntry() : Type(typeid(void)) {}

            // =============================================================================
            // Copy -- atomic is not copyable.
            // =============================================================================
            AssetEntry(const AssetEntry&)               = delete;
            AssetEntry& operator=(const AssetEntry&)    = delete;

            // =============================================================================
            // Move
            // =============================================================================
            AssetEntry(AssetEntry&&)            = default;
            AssetEntry& operator=(AssetEntry&&) = default;
            
            // =============================================================================
            // Function 
            // =============================================================================
        public:
            void Destroy()
            {
                if (Ptr && Deleter)
                {
                    Deleter(Ptr);
                    Ptr     = nullptr;
                    Deleter = nullptr;
                }
            }

            bool IsAlive() const noexcept
            {
                return Block && Block->Get() > 0;
            }
            
            // =============================================================================
            // Members 
            // =============================================================================
        public:
            void*      Ptr  = nullptr;
            std::type_index Type;
            // NOTE: We store a deleter because we type-erased the asset.
            //   Without it we can't call the correct destructor from Unload().
            void(*Deleter)(void*) = nullptr;
            UniquePtr<AssetRefBlock> Block;
        };

        // =============================================================================
        // Function - Static
        // =============================================================================
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
        static AssetHandle<T> Load(OpaaxStringID InID)
        {
            const Uint32 lKey = InID.GetId();

            auto lIt = s_Assets.find(lKey);
            if (lIt != s_Assets.end())
            {
                // Cache hit — validate type matches
                if (lIt->second.Type != typeid(T))
                {
                    OPAAX_CORE_ERROR("AssetRegistry::Load — type mismatch for '{}'", InID);
                    return AssetHandle<T>{};
                }
                
                return AssetHandle<T>{
                    static_cast<T*>(lIt->second.Ptr),
                    InID,
                    lIt->second.Block.get()
                };
            }

            // Cache miss — construct from path
            const OpaaxString lAbsPath = InID.ToString().CStr();
            T* lAsset = new T(lAbsPath.CStr());

            // NOTE: Check IsLoaded() if the type exposes it (textures, audio, etc.).
            //   If the load failed, destroy immediately and return an invalid handle.
            //   We do NOT cache failed loads — the caller can retry with a corrected path.
            if constexpr (requires { lAsset->IsLoaded(); })
            {
                if (!lAsset->IsLoaded())
                {
                    OPAAX_CORE_ERROR("AssetRegistry::Load — failed to load '{}', handle is invalid.", InID);
                    delete lAsset;
                    return AssetHandle<T>{};
                }
            }
            
            auto& lEntry = s_Assets.emplace(lKey, AssetEntry::Make(lAsset)).first->second;

            OPAAX_CORE_INFO("AssetRegistry: loaded '{}'", InID);
            return AssetHandle<T>{ lAsset, InID, lEntry.Block.get() };
        }

        /**
         * Unload
         * Destroys the asset and removes it from the cache.
         * All existing handles to this asset become invalid after this call.
         * TODO: No ref-counting — caller is responsible for ensuring no live handles.
         * @param InID 
         */
        static void Unload(OpaaxStringID InID)
        {
            const Uint32 lKey = InID.GetId();
            auto lIt = s_Assets.find(lKey);

            if (lIt == s_Assets.end())
            {
                OPAAX_CORE_WARN("AssetRegistry::Unload — '{}' not found", InID);
                return;
            }

            if (lIt->second.IsAlive())
            {
                OPAAX_CORE_WARN("AssetRegistry::Unload — '{}' has {} live handle(s), deferred.", InID, lIt->second.Block->Get());
                lIt->second.Destroy(); // nulls Ptr, block stays
                return;
            }

            lIt->second.Destroy();
            s_Assets.erase(lIt);
            OPAAX_CORE_INFO("AssetRegistry: unloaded '{}'", InID);
        }

        static bool IsLoaded(OpaaxStringID InID)
        {
            return s_Assets.count(InID.GetId()) > 0;
        }

        /**
         * Shutdown — destroy all assets
         */
        static void Shutdown()
        {
            OPAAX_CORE_INFO("AssetRegistry::Shutdown() — {} asset(s)", s_Assets.size());
            
            for (auto& [lKey, lEntry] : s_Assets)
            {
                if (lEntry.IsAlive())
                {
                    OPAAX_CORE_WARN("AssetRegistry::Shutdown — asset {} destroyed with {} live handle(s).", lKey, lEntry.Block->Get());
                }
                lEntry.Destroy();
            }
            s_Assets.clear();
        }
         
        // TODO: async loading interface
        //   static void LoadAsync<T>(OpaaxStringID InID, TFunction<void(AssetHandle<T>)> InCallback);
        //   static void ProcessPendingLoads();   // call once per frame from RenderSubsystem
 
        // =============================================================================
        // Members
        // =============================================================================
    private:
        // Uint32 key = OpaaxStringID::GetId() — avoids hashing a string at lookup time
        static UnorderedMap<Uint32, AssetEntry> s_Assets;
    };
} // namespace Opaax
