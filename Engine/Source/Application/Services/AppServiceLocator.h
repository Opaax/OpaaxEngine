#pragma once
#include <type_traits>

#include "IAppService.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTRequire.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    // =============================================================================
    // ServiceLocator — owns app services, resolves by interface type.
    //   Get<T>() NEVER returns null: a missing provider resolves to T::Null().
    // =============================================================================
    class AppServiceLocator
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
         * Constructs and owns a service implementation.
         * @param InArgs Constructor parameters
         * @return The constructed service
         */
        template<class TInterface, class TImpl, typename... Args>
        requires TIsBaseOf(TImpl, TInterface) && TIsBaseOf(TInterface, IAppService)
        TInterface& Provide(Args&&... InArgs)
        {
            TUniquePtr<TImpl>   lImpl   = MakeUnique<TImpl>(Forward<Args>(InArgs)...);
            TInterface&         lRef    = *lImpl;
            const ServiceTypeID lId     = TInterface::StaticTypeID();
            
            m_Services[lId] = Move(lImpl);
            m_Order.emplace_back(lId);
            return lRef;
        }

        /**
         * Takes ownership of an already-constructed service (e.g. EditorPaths for IPaths).
         * @param InImpl The service to adopt
         * @return The adopted service
         */
        template<class TInterface>
        requires std::is_base_of_v<IAppService, TInterface>
        TInterface& ProvideInstance(TUniquePtr<TInterface> InImpl)
        {
            TInterface&         lRef = *InImpl;
            const ServiceTypeID lId  = TInterface::StaticTypeID();
            
            m_Services[lId] = Move(InImpl);
            m_Order.emplace_back(lId);
            return lRef;
        }

        /**
         * @return The service, or its null object if not provided. Never null.
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
            
            return T::Null();
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
         * Shuts services down in reverse order.
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
        TUnorderedMap<ServiceTypeID, TUniquePtr<IAppService>> m_Services;
        TDynArray<ServiceTypeID>                            m_Order;
    };
}
