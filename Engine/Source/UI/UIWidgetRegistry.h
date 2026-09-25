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
    // UIWidgetRegistry — the widget TYPES a `.opaaxui` may name, and how to build one.
    //
    //   `ComponentRegistry`'s shape one module over, and much smaller: a widget serializes ITSELF
    //   through the SaveFields/LoadFields virtuals (**UI12**), so all this has to answer is
    //   "given the name written in the file, make me an empty one of those".
    //
    //   AN UNKNOWN TAG IS A SKIPPED NODE, NOT A FAILED PARSE. A file written by a build that knows
    //   a widget type this one does not must still open — the author loses that node, not the
    //   screen. One warning per unknown name, never one per node.
    // =============================================================================
    class OPAAX_API UIWidgetRegistry
    {
        // =============================================================================
        // Registration
        // =============================================================================
    public:
        /**
         * Make T buildable under InName — the string the file carries.
         *
         * @return false when InName is empty or already taken; the registration is then dropped
         *   with a warning rather than silently shadowing the first.
         */
        template<typename T>
        bool Register(const OpaaxStringID InName)
        {
            return RegisterFactory(InName, []() -> TUniquePtr<UIWidget> { return MakeUnique<T>(); });
        }

        /** Idempotent. Called once the first document opens; after it, Register refuses. */
        void Seal();

        // =============================================================================
        // Build
        // =============================================================================
    public:
        /** A fresh widget of the type InName names, or null when nothing registered under it. */
        TUniquePtr<UIWidget> Create(OpaaxStringID InName) const;

        /** Whether InName is buildable — what a file reader asks before warning. */
        bool IsRegistered(OpaaxStringID InName) const noexcept;

        /** Every registered name, in registration order — the panel's Add menu. */
        const TDynArray<OpaaxStringID>& GetNames() const noexcept { return m_Names; }

        Uint64 Count() const noexcept { return m_Names.size(); }

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        using FWidgetFactory = TFunction<TUniquePtr<UIWidget>()>;

        /** Out-of-line sink for Register<T> — keeps the template body free of the map's type. */
        bool RegisterFactory(OpaaxStringID InName, FWidgetFactory InFactory);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<OpaaxStringID> m_Names;
        TDynArray<FWidgetFactory> m_Factories;   // parallel to m_Names; a handful of entries, so a scan is right
        bool                      m_bSealed = false;
    };
}
