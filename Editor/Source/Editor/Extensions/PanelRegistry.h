#pragma once

#include "Core/OpaaxTypes.h"                // TUniquePtr, TDynArray, TFunction, Uint64, Move
#include "Core/String/OpaaxStringID.hpp"    // OpaaxStringID — interned panel identity
#include "Editor/Panels/IEditorPanel.h"     // the factory's return type must be complete

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * Builds one panel from the context (D3 — the panel receives its dependencies by ctor, never the
     * locator). Deferred on purpose: registration happens at OnModulesRegistered, before Engine::Startup,
     * so no EditorContext exists yet — the factory is what carries the intent across that gap.
     */
    using FPanelFactory = TFunction<TUniquePtr<IEditorPanel>(EditorContext&)>;

    // =============================================================================
    // PanelEntry — one registered panel: its interned identity + the factory that builds it. The Id is
    //   interned at registration so a registered panel and its instance share one identity
    //   (IEditorPanel::GetPanelID), which is what makes menu->panel routing an integer compare (M5).
    // =============================================================================
    struct PanelEntry
    {
        OpaaxStringID Id;
        FPanelFactory Factory;
    };

    // =============================================================================
    // PanelRegistry — the real storage behind EditorExtensionRegistrar::Panels() (Editor.md D10),
    //   replacing the M0 counts-only EditorRoute for this one channel. Native editor panels and game
    //   panels register through the exact same call, so there is no privileged path for engine-side
    //   panels — the property M2a exists to prove.
    //
    //   Registration STORES ONLY; nothing is constructed here. EditorService runs every factory once, in
    //   registration order, from Initialize() (PostEngineStartup), where the context finally exists.
    // =============================================================================
    class PanelRegistry
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        void Register(const char* InName, FPanelFactory InFactory)
        {
            m_Entries.push_back(PanelEntry{ OpaaxStringID(InName), Move(InFactory) });
        }

        // =============================================================================
        // Get - Set
    public:
        /** @return The registered panels in registration order (construction order). */
        const TDynArray<PanelEntry>& Entries() const noexcept { return m_Entries; }

        /** @return How many panels were registered — same signature EditorRoute had, so the seal log is unchanged. */
        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Entries.size()); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<PanelEntry> m_Entries;
    };
}
