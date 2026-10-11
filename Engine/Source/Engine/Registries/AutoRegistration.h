#pragma once

#include "Core/EngineAPI.h"
#include "Core/Hash/OpaaxHash.h"
#include "Core/OpaaxMacro.hpp"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/TypeInfo.h"
#include "Engine/Registries/ModuleRegistrar.h"

// =============================================================================
// ================================== USAGE ====================================
// =============================================================================
// A type registers itself with one line, next to its definition (header or .cpp):
//
//   OPAAX_REGISTER_COMPONENT(Health);
//   OPAAX_REGISTER_BEHAVIOUR(Coin);
//   OPAAX_REGISTER_WORLD_SUBSYSTEM(WaveSpawner);
//   OPAAX_REGISTER_WORLD_SUBSYSTEM_ORDERED(CameraFollow, WorldSubsystemOrder::Presentation);
//   OPAAX_REGISTER_GAME_INSTANCE_SUBSYSTEM(ScoreKeeper);
//   OPAAX_REGISTER_RESOURCE(WaveResource);
//   OPAAX_REGISTER_DATA_ASSET(EnemyStats);
//   OPAAX_REGISTER_UI_WIDGET(UIHealthBar);
//   OPAAX_REGISTER_MOVER_MODE(SwimMoveMode, "Swim");
//
// The engine runs every registration when it is constructed, before any world exists. Game
// modules are linked whole (see opaax_add_game), so a registration in any of their files is
// found. A type registered from several translation units is registered once.
// The saved name of a component is its type name without namespace ("Health"): renaming the C++
// type renames it in map files. Use the NAMED variant to keep a stable name.
// =============================================================================
// ================================ END USAGE ==================================
// =============================================================================

namespace Opaax
{
    // =============================================================================
    // What a registration adds. Kinds run in this order, so a type can rely on an earlier kind
    // (a component alias on its component, a subsystem on a resource format).
    // =============================================================================
    enum class EAutoRegistrationKind : Uint8
    {
        ResourceFormat,
        DataAsset,
        Component,
        ComponentAlias,
        Behaviour,
        MoverMode,
        UIWidget,
        GameInstanceSubsystem,
        WorldSubsystem
    };

    // =============================================================================
    // Tick order of world subsystems: lower ticks first, equal orders tick by name.
    // =============================================================================
    namespace WorldSubsystemOrder
    {
        /** Turns input into intent. */
        inline constexpr Int32 Input        = 100;
        /** Behaviours and game rules: before physics, so what they apply is simulated this step. */
        inline constexpr Int32 Gameplay     = 200;
        /** The physics step. */
        inline constexpr Int32 Physics      = 300;
        /** Reacts to this step's poses (the mover). */
        inline constexpr Int32 PostPhysics  = 400;
        /** Anything that does not care. */
        inline constexpr Int32 Default      = 500;
        /** Animation, camera, HUD: reads the final state of the frame. */
        inline constexpr Int32 Presentation = 600;
        /** Debug drawing, last. */
        inline constexpr Int32 Debug        = 700;
    }

    // =============================================================================
    // AutoRegistration — one self-registering type. Each instance links itself into a
    //   program-wide list during static initialization. Created by the OPAAX_REGISTER_* macros.
    // =============================================================================
    struct AutoRegistration
    {
        using FRegister = void (*)(ModuleRegistrar&);

        /**
         * Links the registration into the program-wide list.
         * @param InName A string literal: sorts registrations of the same order and names them in logs
         */
        AutoRegistration(EAutoRegistrationKind InKind, TypeId InType, Int32 InOrder,
                         const char* InName, FRegister InRegister) noexcept;

        EAutoRegistrationKind   Kind;
        TypeId                  Type;
        Int32                   Order;
        const char*             Name;
        FRegister               Register;
        const AutoRegistration* Next = nullptr;
    };

    /**
     * Every registration linked into the program, sorted by kind, then order, then name. A type
     * registered from several translation units appears once.
     */
    TDynArray<const AutoRegistration*> CollectAutoRegistrations();

    /**
     * Registers every collected type through InRegistrar.
     * @return The number of registrations run
     */
    Uint64 RunAutoRegistrations(ModuleRegistrar& InRegistrar);
}

// =============================================================================
// Macros. Each defines a static registration object with a unique name, so it can sit in a
// header (one object per translation unit, collapsed into one registration) or a .cpp.
// =============================================================================
#define OPAAX_INTERNAL_AUTO_REGISTRATION(InKind, InKey, InOrder, InName, InBody)                     \
    static const ::Opaax::AutoRegistration OPAAX_CONCAT(s_OpaaxAutoRegistration_, __COUNTER__)      \
    {                                                                                              \
        InKind, InKey, InOrder, InName,                                                            \
        [](::Opaax::ModuleRegistrar& InRegistrar) { InBody; }                                      \
    }

/** Registers a component, saved under its type name. */
#define OPAAX_REGISTER_COMPONENT(Type)                                                               \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::Component,                      \
        ::Opaax::TypeIdOf<Type>(), 0, #Type, InRegistrar.Components().Register<Type>())

/** Registers a component saved under Name (a string literal that never changes). */
#define OPAAX_REGISTER_NAMED_COMPONENT(Type, Name)                                                   \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::Component,                      \
        ::Opaax::TypeIdOf<Type>(), 0, Name,                                                        \
        InRegistrar.Components().Register<Type>(::Opaax::OpaaxStringID(Name)))

/** A component every entity has (TransformComponent). Registered before the others. */
#define OPAAX_REGISTER_ESSENTIAL_COMPONENT(Type, Name)                                               \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::Component,                      \
        ::Opaax::TypeIdOf<Type>(), -1000, Name,                                                    \
        InRegistrar.Components().RegisterEssential<Type>(::Opaax::OpaaxStringID(Name)))

/** Lets maps saved with OldName load as the component now registered as Name. */
#define OPAAX_REGISTER_COMPONENT_ALIAS(OldName, Name)                                                \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::ComponentAlias,                 \
        ::Opaax::OpaaxHash::Hash64(OldName), 0, OldName,                                           \
        InRegistrar.Components().AddAlias(::Opaax::OpaaxStringID(OldName), ::Opaax::OpaaxStringID(Name)))

/** Registers a behaviour (gameplay logic stored like a component, saved under its type name). */
#define OPAAX_REGISTER_BEHAVIOUR(Type)                                                               \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::Behaviour,                      \
        ::Opaax::TypeIdOf<Type>(), 0, #Type, InRegistrar.Behaviours().Register<Type>())

/** Registers a world subsystem at the Default tick order. */
#define OPAAX_REGISTER_WORLD_SUBSYSTEM(Type)                                                         \
    OPAAX_REGISTER_WORLD_SUBSYSTEM_ORDERED(Type, ::Opaax::WorldSubsystemOrder::Default)

/** Registers a world subsystem at a tick order (WorldSubsystemOrder). */
#define OPAAX_REGISTER_WORLD_SUBSYSTEM_ORDERED(Type, Order)                                          \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::WorldSubsystem,                 \
        ::Opaax::TypeIdOf<Type>(), Order, #Type, InRegistrar.WorldSubsystems().Register<Type>())

/** Registers a world subsystem under a display name, at a tick order. */
#define OPAAX_REGISTER_NAMED_WORLD_SUBSYSTEM(Type, Name, Order)                                      \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::WorldSubsystem,                 \
        ::Opaax::TypeIdOf<Type>(), Order, Name,                                                    \
        InRegistrar.WorldSubsystems().Register<Type>(::Opaax::OpaaxStringID(Name)))

/** Registers a game instance subsystem (one per game session). */
#define OPAAX_REGISTER_GAME_INSTANCE_SUBSYSTEM(Type)                                                 \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::GameInstanceSubsystem,          \
        ::Opaax::TypeIdOf<Type>(), 0, #Type, InRegistrar.GameInstanceSubsystems().Register<Type>())

/** Registers a game instance subsystem under a display name; lower Order is created first. */
#define OPAAX_REGISTER_NAMED_GAME_INSTANCE_SUBSYSTEM(Type, Name, Order)                              \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::GameInstanceSubsystem,          \
        ::Opaax::TypeIdOf<Type>(), Order, Name,                                                    \
        InRegistrar.GameInstanceSubsystems().Register<Type>(::Opaax::OpaaxStringID(Name)))

/** Registers a resource type for the extensions its OPAAX_RESOURCE_FORMAT lists. */
#define OPAAX_REGISTER_RESOURCE(Type)                                                                \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::ResourceFormat,                 \
        ::Opaax::TypeIdOf<Type>(), 0, #Type, InRegistrar.Resources().Register<Type>())

/** Registers a resource type under a display name. */
#define OPAAX_REGISTER_NAMED_RESOURCE(Type, Name)                                                    \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::ResourceFormat,                 \
        ::Opaax::TypeIdOf<Type>(), 0, Name,                                                        \
        InRegistrar.Resources().Register<Type>(::Opaax::OpaaxStringID(Name)))

/** Registers a struct a .opaaxdata file can hold. */
#define OPAAX_REGISTER_DATA_ASSET(Type)                                                              \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::DataAsset,                      \
        ::Opaax::TypeIdOf<Type>(), 0, #Type, InRegistrar.DataAssets().Register<Type>())

/** Registers a widget type for .opaaxui files, saved under its type name. */
#define OPAAX_REGISTER_UI_WIDGET(Type)                                                               \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::UIWidget,                       \
        ::Opaax::TypeIdOf<Type>(), 0, #Type, InRegistrar.UIWidgets().Register<Type>())

/** Registers a widget type saved under Name. */
#define OPAAX_REGISTER_NAMED_UI_WIDGET(Type, Name)                                                   \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::UIWidget,                       \
        ::Opaax::TypeIdOf<Type>(), 0, Name,                                                        \
        InRegistrar.UIWidgets().Register<Type>(::Opaax::OpaaxStringID(Name)))

/** Registers a movement mode under the name .opaaxmovemode files use. */
#define OPAAX_REGISTER_MOVER_MODE(Type, Name)                                                        \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::MoverMode,                      \
        ::Opaax::TypeIdOf<Type>(), 0, Name,                                                        \
        InRegistrar.MoverModes().Register<Type>(::Opaax::OpaaxStringID(Name)))
