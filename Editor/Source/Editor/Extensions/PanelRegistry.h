#pragma once

#include <concepts>

#include "Core/OpaaxTypes.h"                // TUniquePtr, TDynArray, TFunction, Uint64, Move
#include "Editor/Panels/IEditorPanel.h"     // the factory needs the complete type
#include "Editor/Panels/PanelDesc.h"

namespace Opaax::Editor
{
    struct EditorContext;

    /**
     * Builds one panel from the context. Deferred: panels register before any EditorContext exists.
     */
    using FPanelFactory = TFunction<TUniquePtr<IEditorPanel>(EditorContext&)>;

    /** What Register<T> accepts: a panel constructed from the context only. */
    template<typename T>
    concept CEditorPanel = std::derived_from<T, IEditorPanel> && std::constructible_from<T, EditorContext&>;

    // =============================================================================
    // PanelEntry — one registered panel: its description and the factory that builds it.
    // =============================================================================
    struct PanelEntry
    {
        PanelDesc     Desc;
        FPanelFactory Factory;
    };

    // =============================================================================
    // PanelRegistry — the storage behind EditorExtensionRegistrar::Panels(). Editor panels and game
    //   panels register the same way. Registration only stores; EditorPanels builds them all, in
    //   order, from EditorService::Initialize.
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
            m_Entries.emplace_back(
                Move(InDesc),
                [](EditorContext& InContext) -> TUniquePtr<IEditorPanel> { return MakeUnique<T>(InContext); });
        }

        // =============================================================================
        // Get - Set
    public:
        /** @return The registered panels, in registration (construction) order. */
        const TDynArray<PanelEntry>& Entries() const noexcept { return m_Entries; }

        /** @return How many panels were registered. */
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
