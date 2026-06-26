#pragma once
#include <type_traits>

#include "IAppService.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // ServiceLocator — owns app services, resolves by interface type.
    //   Get<T>() NEVER returns null: a missing provider resolves to T::Null().
    // =============================================================================
    class OPAAX_API AppServiceLocator
    {
        // =============================================================================
        // CTORs - DTOR
        // =============================================================================
    public:
        AppServiceLocator() = default;
        ~AppServiceLocator() { ShutdownAll(); }
        
        // =============================================================================
        // Copy - Move = Delete
        // =============================================================================

        AppServiceLocator(const AppServiceLocator&)             = delete;
        AppServiceLocator& operator=(const AppServiceLocator&)  = delete;

        AppServiceLocator(AppServiceLocator&&)                  = delete;
        AppServiceLocator& operator=(AppServiceLocator&&)       = delete;
        
        // =============================================================================
        // Functions
        // =============================================================================
        
        /**
         * provide: construct + own a real impl
         * @tparam TInterface 
         * @tparam TImpl 
         * @tparam Args Ctor Params
         * @param InArgs Ctor Params
         * @return A reference of the constructed service
         */
        template<class TInterface, class TImpl, typename... Args>
        requires std::is_base_of_v<TInterface, TImpl> && std::is_base_of_v<IAppService, TInterface>
        TInterface& Provide(Args&&... InArgs)
        {
            UniquePtr<TImpl> lImpl = MakeUnique<TImpl>(std::forward<Args>(InArgs)...);
            TInterface&      lRef  = *lImpl;
            const ServiceTypeID lId = TInterface::StaticTypeID();
            m_Services[lId] = std::move(lImpl);
            m_Order.push_back(lId);
            return lRef;
        }

        /**
         * resolve: never null
         * @tparam T 
         * @return A reference of the service or its null version
         */
        template<class T>
        requires std::is_base_of_v<IAppService, T>
        T& Get() const
        {
            const auto lIt = m_Services.find(T::StaticTypeID());
            if (lIt != m_Services.end() && lIt->second)
            {
                return *static_cast<T*>(lIt->second.get());
            }
            
            return T::Null(); // <-- null object
        }

        //Not safe for now
        ////----- precise checks (opt-in) ----------------------------------------
        ///**
        // * 
        // * @tparam T 
        // * @return 
        // */
        //template<class T>
        //requires std::is_base_of_v<IAppService, T>
        //T*   TryGet() const          // raw ptr to the REAL impl, or null
        //{
        //    const auto lIt = m_Services.find(T::StaticTypeID());
        //    return (lIt != m_Services.end()) ? static_cast<T*>(lIt->second.get()) : nullptr;
        //}
        //template<class T> bool Has() const { return TryGet<T>() != nullptr; }

        /**
         * Shutdown services in reverse order
         */
        void ShutdownAll()
        {
            for (auto it = m_Order.rbegin(); it != m_Order.rend(); ++it)
            {
                if (auto lPairIDRef = m_Services.find(*it); lPairIDRef != m_Services.end() && lPairIDRef->second)
                {
                    lPairIDRef->second->OnShutdown();
                }
            }
            
            m_Services.clear();
            m_Order.clear();
        }

    private:
        UnorderedMap<ServiceTypeID, UniquePtr<IAppService>> m_Services;
        TDynArray<ServiceTypeID>                            m_Order;
    };
}
