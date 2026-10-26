#pragma once

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// A subsystem is a service that lives as long as its owner (Unreal's USubsystem). Derive the
// base of the lifetime you want and register the class; its owner makes one for itself:
//
//   class WaveSpawner final : public Opaax::WorldSubsystem
//   {
//       OPAAX_OBJECT(WaveSpawner, Opaax::WorldSubsystem)
//
//   public:
//       bool  ShouldCreate(const Opaax::World& InWorld) const override;   // optional
//       Opaax::Int32 GetTickOrder() const override { return Opaax::WorldSubsystemOrder::Gameplay; }
//
//       bool Startup() override;                 // GetContext() is valid from here
//       void Update(double InDeltaTime) override;
//   };
//   OPAAX_REGISTER_OBJECT(WaveSpawner);
//
//   EngineSubsystem        the engine's services, made by the engine itself
//   GameInstanceSubsystem  one per game, across level changes
//   WorldSubsystem         one per world that ShouldCreate accepts
//
// The constructor makes nothing: the class default object is built with it, outside any owner.
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    class ObjectClassRegistry;

    OPAAX_LOG_CATEGORY(Subsystem);

    // =============================================================================
    // Subsystem — the base of every subsystem. Abstract: derive EngineSubsystem,
    //   GameInstanceSubsystem or WorldSubsystem.
    // =============================================================================
    class Subsystem : public Object
    {
        OPAAX_ABSTRACT_OBJECT(Subsystem, Object)

        // =============================================================================
        // Lifetime
        // =============================================================================
    public:
        /**
         * Starts the subsystem. Every subsystem of its owner exists by then (not every one started),
         * so one can find another here.
         * @return False when it could not start (it logs why); it stays with its owner
         */
        virtual bool Startup() { return true; }

        /**
         * First shutdown step: everything is still alive. Release here anything that depends on
         * another subsystem; in Shutdown() other subsystems may be gone.
         */
        virtual void TearDown() {}

        virtual void Shutdown() {}

        // =============================================================================
        // Tick
        // =============================================================================
    public:
        virtual void Update(double /*InDeltaTime*/) {}
        virtual void FixedUpdate(double /*InFixedDeltaTime*/) {}
        virtual void Render(double /*InAlpha*/) {}

        /**
         * Where it starts and ticks among the subsystems its owner finds by class (a game's, a
         * world's): lower first, equal orders by class name. Read on the class default object.
         */
        virtual Int32 GetTickOrder() const { return 0; }
    };

    /**
     * The registered concrete classes deriving from InBase, in the order their owner starts them:
     * by GetTickOrder(), then by class name.
     */
    TDynArray<const ObjectClass*> CollectSubsystemClasses(const ObjectClassRegistry& InClasses,
                                                          const ObjectClass& InBase);

    // =============================================================================
    // TSubsystemCollection<TBase> — owns one owner's subsystems and runs their lifetime. Every one is
    //   made before any starts, so a Startup can find a sibling. They start and tick in the order they
    //   were added, and stop in reverse.
    // =============================================================================
    template<typename TBase>
    requires CDerivedFrom<TBase, Subsystem>
    class TSubsystemCollection
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        TSubsystemCollection()          = default;
        virtual ~TSubsystemCollection() = default;

        TSubsystemCollection(const TSubsystemCollection&)            = delete;
        TSubsystemCollection& operator=(const TSubsystemCollection&) = delete;

        TSubsystemCollection(TSubsystemCollection&&)            = default;
        TSubsystemCollection& operator=(TSubsystemCollection&&) = default;

        // =============================================================================
        // Adding
        // =============================================================================
    public:
        /**
         * Adds a subsystem of class T, made at StartupAll.
         * @param InArgs For T's constructor, when it takes some (an engine subsystem's settings)
         */
        template<typename T, typename... TArgs>
        requires CDerivedFrom<T, TBase>
        void Add(TArgs&&... InArgs)
        {
            m_Factories.emplace_back([... lArgs = Forward<TArgs>(InArgs)]() mutable
            {
                return TStrongObjectPtr<TBase>(NewObject<T>(T::StaticClass().GetName(),
                                                            Forward<decltype(lArgs)>(lArgs)...));
            });
        }

        /**
         * Adds a subsystem of InClass (a class found at run time), made at StartupAll.
         * @return False (logged) when InClass cannot be made or is not a TBase
         */
        bool Add(const ObjectClass& InClass)
        {
            if (InClass.IsAbstract() || !InClass.IsChildOf(TBase::StaticClass()))
            {
                OPAAX_LOG(LogSubsystem, Error, "'{}' is not a concrete {}: not added",
                          InClass.GetName().CStr(), TBase::StaticClass().GetName().CStr());
                return false;
            }

            m_Factories.emplace_back([&InClass]()
            {
                return Cast<TBase>(InClass.CreateInstance(InClass.GetName()));
            });
            return true;
        }

        // =============================================================================
        // Lifetime
        // =============================================================================
    public:
        /** Makes every added subsystem, then starts them all, in order. */
        void StartupAll()
        {
            for (auto& lFactory : m_Factories)
            {
                if (TStrongObjectPtr<TBase> lSystem = lFactory())
                {
                    OnCreated(*lSystem);
                    m_Systems.emplace_back(Move(lSystem));
                }
            }

            m_Factories.clear();
            m_Factories.shrink_to_fit();

            for (const TStrongObjectPtr<TBase>& lSystem : m_Systems)
            {
                lSystem->Startup();
            }
        }

        /** First shutdown step, in reverse order. */
        void TearDownAll()
        {
            for (auto lIt = m_Systems.rbegin(); lIt != m_Systems.rend(); ++lIt)
            {
                (*lIt)->TearDown();
            }
        }

        /** Shuts every subsystem down, in reverse order. They are destroyed with the collection. */
        void ShutdownAll()
        {
            for (auto lIt = m_Systems.rbegin(); lIt != m_Systems.rend(); ++lIt)
            {
                (*lIt)->Shutdown();
            }
        }

        // =============================================================================
        // Tick
        // =============================================================================
    public:
        void UpdateAll(const double InDeltaTime)
        {
            for (const TStrongObjectPtr<TBase>& lSystem : m_Systems)
            {
                lSystem->Update(InDeltaTime);
            }
        }

        void FixedUpdateAll(const double InFixedDeltaTime)
        {
            for (const TStrongObjectPtr<TBase>& lSystem : m_Systems)
            {
                lSystem->FixedUpdate(InFixedDeltaTime);
            }
        }

        void RenderAll(const double InAlpha)
        {
            for (const TStrongObjectPtr<TBase>& lSystem : m_Systems)
            {
                lSystem->Render(InAlpha);
            }
        }

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** The first subsystem that is a T (its class or a child of it), or null. */
        template<typename T>
        requires CDerivedFrom<T, TBase>
        T* GetSubsystem() { return Find<T>(); }

        template<typename T>
        requires CDerivedFrom<T, TBase>
        const T* GetSubsystem() const { return Find<T>(); }

        /** Every made subsystem, in start order. */
        const TDynArray<TStrongObjectPtr<TBase>>& GetSystems() const noexcept { return m_Systems; }

        // =============================================================================
        // Owner hook
        // =============================================================================
    protected:
        /** Called for each subsystem once made, before any Startup: where an owner hands its context. */
        virtual void OnCreated(TBase& /*InSubsystem*/) {}

    private:
        template<typename T>
        T* Find() const
        {
            for (const TStrongObjectPtr<TBase>& lSystem : m_Systems)
            {
                if (lSystem->template IsA<T>())
                {
                    return static_cast<T*>(lSystem.Get());
                }
            }
            return nullptr;
        }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TFunction<TStrongObjectPtr<TBase>()>> m_Factories;
        TDynArray<TStrongObjectPtr<TBase>>              m_Systems;
    };
}
