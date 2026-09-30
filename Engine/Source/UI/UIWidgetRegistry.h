#pragma once

#include "Core/Log/Logger.h"
#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxStringID.hpp"
#include "UI/UIWidget.h"

namespace Opaax
{
    inline constexpr LogCategory LogUIWidgetRegistry{"UIWidgetRegistry"};

    // =============================================================================
    // UIWidgetRegistry — the widget types a .opaaxui can use, and how to create them.
    //   Widgets serialize themselves (SaveFields/LoadFields). An unknown type is a skipped node
    //   (one warning per name), not a failed file.
    // =============================================================================
    class OPAAX_API UIWidgetRegistry
    {
        // =============================================================================
        // Registration
        // =============================================================================
    public:
        /**
         * Registers T under InName (the name in files).
         * @return False if InName is empty or already used (dropped with a warning)
         */
        template<typename T>
        bool Register(const OpaaxStringID InName)
        {
            return RegisterFactory(InName, []() -> TUniquePtr<UIWidget> { return MakeUnique<T>(); });
        }

        /** Called when the first document opens. After this, Register refuses. Safe to call twice. */
        void Seal();

        // =============================================================================
        // Build
        // =============================================================================
    public:
        /** A new widget of type InName, or null if not registered. */
        TUniquePtr<UIWidget> Create(OpaaxStringID InName) const;

        /** Whether InName is registered. */
        bool IsRegistered(OpaaxStringID InName) const noexcept;

        /** Every registered name, in order (the panel's Add menu). */
        const TDynArray<OpaaxStringID>& GetNames() const noexcept { return m_Names; }

        Uint64 Count() const noexcept { return m_Names.size(); }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        using FWidgetFactory = TFunction<TUniquePtr<UIWidget>()>;

        /** Stores a factory (keeps the template free of the map type). */
        bool RegisterFactory(OpaaxStringID InName, FWidgetFactory InFactory);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<OpaaxStringID> m_Names;
        TDynArray<FWidgetFactory> m_Factories;   // same order as m_Names
        bool                      m_bSealed = false;
    };
}
