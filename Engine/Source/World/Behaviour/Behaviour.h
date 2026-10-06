#pragma once

#include <concepts>

#include <nlohmann/json.hpp>

#include "Core/EngineAPI.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/Reflection/OpaaxProperty.h"
#include "Core/Reflection/TypeInfo.h"
#include "Engine/Reflection/PropertyJson.h"

namespace Opaax
{
    inline constexpr LogCategory LogBehaviour{"Behaviour"};

    // =============================================================================
    // Behaviour — gameplay logic on an entity: a class with its own fields and lifecycle hooks.
    //   Stored and saved like a component: only the fields listed in OPAAX_PROPERTIES are saved.
    //   Register with Behaviours().Register<T>(). One of each type per entity.
    //
    //     struct Coin : Opaax::Behaviour
    //     {
    //         Int32 Value = 1;
    //         OPAAX_PROPERTIES(Coin, OPAAX_PROP(Value))
    //
    //         void OnUpdate(float InDeltaTime) override { ... }
    //     };
    // =============================================================================
    class Behaviour
    {
        // =============================================================================
        // Storage
        // =============================================================================
    public:
        /** Instances never move in storage, so a `this` held elsewhere stays valid. */
        static constexpr bool in_place_delete = true;

        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        virtual ~Behaviour() = default;

    protected:
        // Protected: a behaviour cannot be copied or moved as a plain Behaviour (no slicing).
        Behaviour()                                = default;
        Behaviour(const Behaviour&)                = default;
        Behaviour(Behaviour&&) noexcept            = default;
        Behaviour& operator=(const Behaviour&)     = default;
        Behaviour& operator=(Behaviour&&) noexcept = default;

        // =============================================================================
        // Lifecycle
        // =============================================================================
    public:
        /** Once, before the first update and the first event. */
        virtual void OnStart() {}

        /** Every frame. */
        virtual void OnUpdate(float /*InDeltaTime*/) {}

        /** Every fixed step, with physics. */
        virtual void OnFixedUpdate(float /*InFixedDeltaTime*/) {}

        /** Once, when the behaviour or its entity ends. Only if OnStart ran. */
        virtual void OnDestroy() {}
    };

    /**
     * A type that can be registered as a behaviour.
     */
    template<typename T>
    concept CBehaviour = std::derived_from<T, Behaviour> && std::default_initializable<T>;

    // =============================================================================
    // JSON — found for any behaviour type (its base is in Opaax). Written from its property list;
    //   a behaviour with no OPAAX_PROPERTIES saves as an empty object.
    // =============================================================================
    template<typename T>
    requires std::derived_from<T, Behaviour>
    void to_json(nlohmann::json& OutJson, const T& InBehaviour)
    {
        if constexpr (CReflected<T>) { OutJson = PropertiesToJson(InBehaviour); }
        else                         { OutJson = nlohmann::json::object(); }
    }

    /**
     * A missing field keeps its default. A wrong-typed field keeps it too, with a warning.
     * Throws if InJson is not an object (the map loader then skips the behaviour, with a warning).
     */
    template<typename T>
    requires std::derived_from<T, Behaviour>
    void from_json(const nlohmann::json& InJson, T& OutBehaviour)
    {
        if (!InJson.is_object())
        {
            throw nlohmann::json::type_error::create(302, "a behaviour's data must be an object", &InJson);
        }

        if constexpr (CReflected<T>)
        {
            TDynArray<OpaaxString> lBadFields;
            PropertiesFromJson(InJson, OutBehaviour, &lBadFields);

            for (const OpaaxString& lField : lBadFields)
            {
                OPAAX_LOG(LogBehaviour, Warn, "'{}': field '{}' has the wrong type in the file, kept its default",
                          DeriveTypeLeafName<T>(), lField.CStr());
            }
        }
    }
}
