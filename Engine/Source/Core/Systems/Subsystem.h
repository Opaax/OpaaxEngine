#pragma once

#include <algorithm>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    using SubsystemTypeID = uintptr_t;
    
    /**
     * @class ISubsystem
     *
     * Base interface for subsystem
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
         * Phase 1 of the two-phase stop: the frame loop has stopped, but NOTHING is
         * destroyed yet — every sibling subsystem, every app service, the window and the
         * GPU context are all still alive.
         *
         * This is the only phase in which a subsystem may safely reach a sibling to release
         * something. By Shutdown() the teardown is already under way and a sibling may
         * already be gone, so anything that needs a live neighbour MUST happen here.
         *
         * Not pure: a subsystem with nothing to release at this phase simply ignores it.
         */
        virtual void            TearDown()                          {}

        virtual void            Shutdown()                          = 0;
        virtual void            Update(double DeltaTime)            {}
        virtual void            FixedUpdate(double FixedDeltaTime)  {}
        virtual void            Render(double AlphaPhysicStep)      {}
        virtual SubsystemTypeID GetTypeID() const noexcept          = 0;
    };

    // Stamp onto any concrete subsystem class. StaticTypeID() returns the address of a
    // function-local static — one tag per type, zero runtime cost. This inline form is
    // DLL-safe *because every engine subsystem is OPAAX_API*: dllexport in the engine,
    // dllimport in consumers, so MSVC imports the single exported inline definition into the
    // exe instead of re-emitting it — the DLL and exe share ONE s_TypeTag. Proven by the S9
    // probe (2026-07-20): GetSubsystem<WorldManager>() is non-null across the exe/DLL line
    // (instance tag == exe-side static tag). CAVEAT: a subsystem that is NOT dll-exported
    // (defined in a static lib / game module) would get a per-module copy — resolve it
    // out-of-line like OPAAX_SERVICE_TYPE, or export it. See ARCHITECTURE.md I2.
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
         * Start up all systems: construct every registered subsystem, then start them in
         * registration order. The create pass completes before ANY Startup runs, which is what
         * lets a subsystem resolve a sibling during its own Startup (F3).
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

            //consumes it
            m_Factories.clear();
            m_Factories.shrink_to_fit();
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

        virtual void RenderAll(double Alpha)
        {
            for (auto& lSystem : m_Systems)
            {
                lSystem->Render(Alpha);
            }
        }

        /**
         * Tear down all subsystems, reverse registration order (mirrors ShutdownAll).
         *
         * Reverse order is what makes this useful: the subsystems registered EARLIEST are
         * the foundational ones (event bus, resources), so they tear down LAST and are still
         * alive while the later ones release through them.
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

        /*----------------------------- Get - Set -------------------------------*/

        const TDynArray<UniquePtr<SubsystemType>>& GetSystems() const { return m_Systems; }
        
        template<typename T>
        requires std::is_base_of_v<SubsystemType, T>
        T* GetSubsystem()
        {
            for (auto& lSystem : m_Systems)
            {
                if (lSystem->GetTypeID() == T::StaticTypeID())
                {
                    // NOTE: static_cast est safe ici — on a vérifié le type via StaticTypeID().
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
        TDynArray<TFunction<UniquePtr<SubsystemType>()>> m_Factories;
        TDynArray<UniquePtr<SubsystemType>> m_Systems;
    };
}