#pragma once

#include "IAssetLoader.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Log/OpaaxLog.h"

#include <typeindex>
#include <unordered_map>
#include <memory>

namespace Opaax
{
    /**
     * @class AssetLoaderRegistry
     *
     * Maps std::type_index → IAssetLoader<T> instance.
     * Usage:
     *  AssetLoaderRegistry::Register<Texture2D>(MakeUnique<TextureLoader>());
     *  IAssetLoader<Texture2D>* lLoader = AssetLoaderRegistry::Get<Texture2D>();
     */
    class OPAAX_API AssetLoaderRegistry
    {
        // =============================================================================
        // Static
        // =============================================================================
    public:
        /**
         * 
         * @tparam T 
         * @param InLoader 
         */
        template<typename T>
        static void Register(UniquePtr<IAssetLoader<T>> InLoader)
        {
            const std::type_index lKey = typeid(T);

            if (s_Loaders.count(lKey))
            {
                OPAAX_CORE_WARN("AssetLoaderRegistry::Register — loader for '{}' already registered, replacing.", typeid(T).name());
            }

            // We type-erase the loader into a void* UniquePtr with a custom deleter.
            //   Get<T>() casts back safely because we key on typeid(T).
            s_Loaders[lKey] = {
                static_cast<void*>(InLoader.release()),
                [](void* InPtr) { delete static_cast<IAssetLoader<T>*>(InPtr); }
            };

            OPAAX_CORE_TRACE("AssetLoaderRegistry: registered loader for '{}'", typeid(T).name());
        }

        /**
         * 
         * @tparam T 
         * @return 
         */
        template<typename T>
        static IAssetLoader<T>* Get() noexcept
        {
            auto lIt = s_Loaders.find(typeid(T));
            if (lIt == s_Loaders.end()) { return nullptr; }
            return static_cast<IAssetLoader<T>*>(lIt->second.Ptr);
        }

        /**
         * 
         */
        static void Shutdown()
        {
            // NOTE: Deleters are called here — each loader is destroyed with the
            //   correct type, even though stored as void*.
            for (auto& [lKey, lEntry] : s_Loaders)
            {
                if (lEntry.Ptr && lEntry.Deleter)
                {
                    lEntry.Deleter(lEntry.Ptr);
                    lEntry.Ptr = nullptr;
                }
            }
            s_Loaders.clear();
        }

    private:
        struct LoaderEntry
        {
            void* Ptr             = nullptr;
            void(*Deleter)(void*) = nullptr;
        };
        
        static UnorderedMap<std::type_index, LoaderEntry> s_Loaders;
    };

} // namespace Opaax