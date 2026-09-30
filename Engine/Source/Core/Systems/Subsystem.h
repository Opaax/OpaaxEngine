#pragma once

#include <algorithm>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    using SubsystemTypeID = uintptr_t;
    
    /**
     * Base interface for subsystems.
     */
    class OPAAX_API ISubsystem
    {
        // =============================================================================
        // CTORs
        // =============================================================================
    public:
        virtual ~ISubsystem() = default;

        // =============================================================================
        // Functions
        // =============================================================================
        virtual bool            Startup()                           = 0;

        /**
         * First shutdown step: everything is still alive. Release here anything that
         * depends on another subsystem; in Shutdown() other subsystems may be gone.
         */
        virtual void            TearDown()                          {}

        virtual void            Shutdown()                          = 0;
        virtual void            Update(double DeltaTime)            {}
        virtual void            FixedUpdate(double FixedDeltaTime)  {}
        virtual void            Render(double AlphaPhysicStep)      {}
        virtual SubsystemTypeID GetTypeID() const noexcept          = 0;
    };

    // Add to each concrete subsystem. One type tag per type. Works across the DLL/exe
    // boundary because engine subsystems are OPAAX_API; a non-exported subsystem needs an
    // out-of-line StaticTypeID (like OPAAX_SERVICE_TYPE).
#define OPAAX_SUBSYSTEM_TYPE(ClassName)                                         \
static ::Opaax::SubsystemTypeID StaticTypeID() noexcept                     \
{                                                                            \
static const int s_TypeTag = 0;                                         \
return reinterpret_cast<::Opaax::SubsystemTypeID>(&s_TypeTag);          \
}                                                                            \
::Opaax::SubsystemTypeID GetTypeID() const noexcept override                \
{                                                                            \
return StaticTypeID();                                                   \
}

    /**
     * Owns and ticks a group of subsystems (like Unreal's engine/world subsystems).
     * Not OPAAX_API: never export a class template.
     * @tparam TSubsystem The subsystem base type this manager handles
     */
    template <class TSubsystem>
    requires std::is_base_of_v<ISubsystem, TSubsystem>
    class ISubsystemManager
    {
        using SubsystemType = TSubsystem;

        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        ISubsystemManager() = default;
        virtual ~ISubsystemManager() = default;
        
        //Remove Copy
        ISubsystemManager(const ISubsystemManager&) = delete;
        ISubsystemManager& operator=(const ISubsystemManager&) = delete;

        //Move
        ISubsystemManager(ISubsystemManager&&) = default;
        ISubsystemManager& operator=(ISubsystemManager&&) = default;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Registers a subsystem type. It is created and started in StartupAll.
         */
        template<typename T, typename... Args>
        requires std::is_base_of_v<SubsystemType, T>
        void RegisterSubsystem(Args&&... InArgs)
        {
            m_Factories.emplace_back([...Arguments = std::forward<Args>(InArgs)]() mutable
            {
                return MakeUnique<T>(std::forward<decltype(Arguments)>(Arguments)...);
            });
        }

        /**
         * Creates every registered subsystem, then starts them in registration order.
         * All are created before any starts, so a subsystem can find another in its Startup.
         */
        void StartupAll()
        {
            for (auto& lFactoryFunc : m_Factories)
            {
                m_Systems.emplace_back(lFactoryFunc());
            }

            // startup in order
            for (auto& lSystem : m_Systems)
            {
                lSystem->Startup();
            }

            //consumes it
            m_Factories.clear();
            m_Factories.shrink_to_fit();
        }

        void UpdateAll(double DeltaTime)
        {
            for (auto& lSystem : m_Systems)
            {
                lSystem->Update(DeltaTime);
            }
        }

        virtual void FixedUpdateAll(double FixedDeltaTime)
        {
            for (auto& lSystem : m_Systems)
            {
                lSystem->FixedUpdate(FixedDeltaTime);
            }
        }

        virtual void RenderAll(double Alpha)
        {
            for (auto& lSystem : m_Systems)
            {
                lSystem->Render(Alpha);
            }
        }

        /**
         * First shutdown step, in reverse registration order.
         */
        void TearDownAll()
        {
            for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it)
            {
                if (*it != nullptr)
                {
                    (*it)->TearDown();
                }
            }
        }

        /**
         * Shuts down every subsystem, in reverse registration order.
         */
        void ShutdownAll()
        {
            for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it)
            {
                if (*it != nullptr)
                {
                    (*it)->Shutdown();
                }
            }
        }

        /*----------------------------- Get - Set -------------------------------*/

        const TDynArray<TUniquePtr<SubsystemType>>& GetSystems() const { return m_Systems; }
        
        template<typename T>
        requires std::is_base_of_v<SubsystemType, T>
        T* GetSubsystem()
        {
            for (auto& lSystem : m_Systems)
            {
                if (lSystem->GetTypeID() == T::StaticTypeID())
                {
                    // Safe: the type was checked with StaticTypeID().
                    return static_cast<T*>(lSystem.get());
                }
            }
            return nullptr;
        }

        template<typename T>
        requires std::is_base_of_v<SubsystemType, T>
        const T* GetSubsystem() const
        {
            for (const auto& lSystem : m_Systems)
            {
                if (lSystem->GetTypeID() == T::StaticTypeID())
                {
                    return static_cast<const T*>(lSystem.get());
                }
            }
            return nullptr;
        }

        // =============================================================================
        // MEMBERS
        // =============================================================================
    private:
        TDynArray<TFunction<TUniquePtr<SubsystemType>()>> m_Factories;
        TDynArray<TUniquePtr<SubsystemType>> m_Systems;
    };
}