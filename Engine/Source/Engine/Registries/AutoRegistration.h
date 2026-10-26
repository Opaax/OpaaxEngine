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
//   OPAAX_REGISTER_OBJECT(EnemyStats);      // any Object class: assets, move modes, subsystems, widgets...
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
    // (a class alias on its class, a component alias on its component).
    // =============================================================================
    enum class EAutoRegistrationKind : Uint8
    {
        ObjectClass,
        ObjectClassAlias,
        Component,
        ComponentAlias,
        Behaviour
    };

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

/**
 * Registers a behaviour: its class, and its storage on entities (saved like a component, under its
 * class name).
 */
#define OPAAX_REGISTER_BEHAVIOUR(Type)                                                               \
    static_assert(::Opaax::CObject<Type>,                                                            \
                  "A behaviour declares its class: OPAAX_OBJECT(" #Type ", Opaax::Behaviour) first in its body."); \
    OPAAX_REGISTER_OBJECT(Type);                                                                     \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::Behaviour,                      \
        ::Opaax::TypeIdOf<Type>(), 0, #Type, InRegistrar.Behaviours().Register<Type>())

/** Registers an Object class under its C++ name (no namespace), the name its files use. */
#define OPAAX_REGISTER_OBJECT(Type)                                                                  \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::ObjectClass,                    \
        ::Opaax::TypeIdOf<Type>(), 0, #Type, InRegistrar.ObjectClasses().Register<Type>())

/** Lets files that name OldName load as the class now registered as Name (after a rename). */
#define OPAAX_REGISTER_OBJECT_ALIAS(OldName, Name)                                                   \
    OPAAX_INTERNAL_AUTO_REGISTRATION(::Opaax::EAutoRegistrationKind::ObjectClassAlias,               \
        ::Opaax::OpaaxHash::Hash64(OldName), 0, OldName,                                           \
        InRegistrar.ObjectClasses().AddAlias(::Opaax::OpaaxStringID(OldName), ::Opaax::OpaaxStringID(Name)))
