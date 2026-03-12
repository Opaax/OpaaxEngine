#pragma once

#include <algorithm>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * @class ISubsystem
     *
     * 
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
        virtual bool Startup() = 0;
        virtual void Shutdown() = 0;
        virtual void Update(double DeltaTime) {}
        virtual void FixedUpdate(double FixedDeltaTime) {}
        virtual void Render(double AlphaPhysicStep) {}
    };

    /**
     * @class ISubsystemManager
     *
     * The idea is similar to Unreal Subsystem, Engine Subsystem, World System etc....
     * 
     * @tparam TSubsystem Which subsystem that managers manage
     */
    template <class TSubsystem>
    requires std::is_base_of_v<ISubsystem, TSubsystem>
    class OPAAX_API ISubsystemManager
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
         * Register a subsystem.
         * Here register a type that must be manage by this manager.
         * Create a unique ptr func to start up the subsystem later i.e StartupAll
         * 
         * @tparam T 
         * @tparam Args 
         * @param InArgs 
         */
        template<typename T, typename... Args>
        requires std::is_base_of_v<SubsystemType, T>
        void RegisterSubsystem(Args&&... InArgs)
        {
            m_Factories.push_back([...Arguments = std::forward<Args>(InArgs)]() mutable
            {
                return MakeUnique<T>(std::forward<decltype(Arguments)>(Arguments)...);
            });
        }

        /**
         * Start up all systems
         */
        void StartupAll()
        {
            for (auto& lFactoryFunc : m_Factories)
            {
                m_Systems.push_back(lFactoryFunc());
            }

            // startup in order
            for (auto& lSystem : m_Systems)
            {
                lSystem->Startup();
            }
        }

        /**
         * 
         * @param DeltaTime 
         */
        void UpdateAll(double DeltaTime)
        {
            for (auto& lSystem : m_Systems)
            {
                lSystem->Update(DeltaTime);
            }
        }

        /**
         * 
         * @param FixedDeltaTime 
         */
        virtual void FixedUpdateAll(double FixedDeltaTime)
        {
            for (auto& lSystem : m_Systems)
            {
                lSystem->FixedUpdate(FixedDeltaTime);
            }
        }

        /**
         * Shutdown all subsystems
         *
         * Shutdown in reverse order from register
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

        // =============================================================================
        // MEMBERS
        // =============================================================================
    private:
        TDynArray<TFunction<UniquePtr<SubsystemType>()>> m_Factories;
        TDynArray<UniquePtr<SubsystemType>> m_Systems;
    };
}