#pragma once

#include <functional>

#include "Scene.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "Core/OpaaxTypes.h"

namespace Opaax
{
    /**
     * @class SceneFactory
     *
     * Name→constructor registry for Scene subclasses. Lets engine systems that
     * need to rebuild the scene stack (PIE Stop, save-file restore) instantiate
     * the correct Scene-derived type without RTTI or a switch-on-name.
     *
     * Engine registers the base Scene under "Untitled" in CoreEngineApp::Initialize.
     * Games register their scene types from their OnInitialize override.
     */
    class OPAAX_API SceneFactory
    {
        // =============================================================================
        // Type
        // =============================================================================
    public:
        using Factory = std::function<UniquePtr<Scene>()>;

        // =============================================================================
        // Registration
        // =============================================================================
    public:
        /**
         * Register a Scene subclass under InName. T must be default-constructible.
         * Re-registering the same name is a no-op + warn — first registration wins.
         */
        template<typename T>
        static void Register(const char* InName)
        {
            static_assert(std::is_base_of_v<Scene, T>,
                "SceneFactory::Register — T must derive from Scene");

            Register(InName, []() -> UniquePtr<Scene> { return MakeUnique<T>(); });
        }

        static void Register(const char* InName, Factory InFactory);

        // =============================================================================
        // Lookup
        // =============================================================================
    public:
        /**
         * Instantiate a Scene by its registered name. Returns nullptr if no
         * factory was registered for InName.
         */
        static UniquePtr<Scene> Create(const OpaaxStringID& InName);

        /**
         * True if a factory is registered for InName.
         */
        static bool Has(const OpaaxStringID& InName);

        /**
         * Drop every registered factory. Called from CoreEngineApp::Shutdown so
         * captured lambdas don't outlive engine teardown.
         */
        static void Clear();

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct Entry
        {
            OpaaxStringID Name;
            Factory       Make;
        };

        static TDynArray<Entry>& Registry();
    };
}
