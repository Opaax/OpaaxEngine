#pragma once

#include <concepts>

#include "Core/OpaaxTypes.h"                // TUniquePtr, TDynArray, TFunction, Uint64, Move
#include "Editor/Panels/IEditorPanel.h"     // the factory's return type must be complete
#include "Editor/Panels/PanelDesc.h"

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * Builds one panel from the context (D3 — the panel receives its dependencies by ctor, never the
     * locator). Deferred on purpose: registration happens at OnModulesRegistered, before Engine::Startup,
     * so no EditorContext exists yet — the factory is what carries the intent across that gap.
     */
    using FPanelFactory = TFunction<TUniquePtr<IEditorPanel>(EditorContext&)>;

    /** What Register<T> accepts: a panel built from nothing but the context. */
    template<typename T>
    concept CEditorPanel = std::derived_from<T, IEditorPanel> && std::constructible_from<T, EditorContext&>;

    // =============================================================================
    // PanelEntry — one registered panel: its description + the factory that builds it.
    // =============================================================================
    struct PanelEntry
    {
        PanelDesc     Desc;
        FPanelFactory Factory;
    };

    // =============================================================================
    // PanelRegistry — the real storage behind EditorExtensionRegistrar::Panels() (Editor.md D10).
    //   Native editor panels and game panels register through the exact same call, so there is no
    //   privileged path for engine-side panels — the property M2a exists to prove.
    //
    //   Registration STORES ONLY; nothing is constructed here. EditorPanels runs every factory once,
    //   in registration order, from EditorService::Initialize, where the context finally exists.
    // =============================================================================
    class PanelRegistry
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        template<CEditorPanel T>
        void Register(PanelDesc InDesc)
        {
            m_Entries.push_back(PanelEntry{
                Move(InDesc),
                [](EditorContext& InContext) -> TUniquePtr<IEditorPanel> { return MakeUnique<T>(InContext); }
            });
        }

        // =============================================================================
        // Get - Set
    public:
        /** @return The registered panels in registration order (construction order). */
        const TDynArray<PanelEntry>& Entries() const noexcept { return m_Entries; }

        /** @return How many panels were registered — what the seal log reports. */
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
