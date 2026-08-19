#pragma once

#include "Core/OpaaxTypes.h"        // TFunction, TDynArray, Uint64
#include "World/Entity/Entity.h"    // Entity::TryGet — the closure's self-check

namespace Opaax::Editor
{
    /**
     * Draws one component of one entity, if that entity actually has it.
     * @return true when the component was present and drawn — lets a caller tell "nothing applied" from
     *   "nothing registered", which is what the Inspector's empty state needs.
     */
    using FDrawerInvoke = TFunction<bool(Entity&)>;

    // =============================================================================
    // DrawerEntry — one registered component drawer, fully type-erased. Deliberately just the closure:
    //   the drawer owns its own presentation (its own CollapsingHeader/label), so nothing here needs a
    //   display name — which is what keeps the M0 call-site shape Register<TComponent, TDrawer>()
    //   unchanged (MR1: the call-site API was final, only the body becomes real).
    // =============================================================================
    struct DrawerEntry
    {
        FDrawerInvoke Invoke;
    };

    // =============================================================================
    // DrawerRegistry — real storage behind EditorExtensionRegistrar::Drawers() (Editor.md D10),
    //   replacing the counts-only EditorRoute for that channel (M2b), as PanelRegistry did in M2a.
    //
    //   THE DESIGN, in one line: registration erases <TComponent, TDrawer> into a uniform
    //   bool(Entity&) closure that self-checks with TryGet<TComponent>().
    //
    //   That inversion is why the Inspector needs NO component reflection and no entt introspection. It
    //   never asks "what components does this entity have?" — an answer entt cannot give in typed form
    //   without a type registry. It asks every registered drawer "are you applicable?", and each one
    //   answers for itself. Registration order is display order.
    //
    //   DUCK-TYPED contract, checked at instantiation, with NO base class (D7 declines OOP/virtual
    //   component drawers). A TDrawer must be:
    //       - default-constructible (it is a stateless strategy, built per draw)
    //       - callable as  void Draw(TComponent&)
    //   The body may live in a .cpp — the closure only needs to CALL it, so it resolves at link time.
    //
    //   Registration STORES ONLY; nothing is constructed. It must: RegisterExtensions runs at the
    //   OnModulesRegistered seam, before Engine::Startup, so there is no world and no entity yet.
    // =============================================================================
    class DrawerRegistry
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        template<typename TComponent, typename TDrawer>
        void Register()
        {
            m_Entries.emplace_back(
                [](Entity& InEntity) -> bool
                {
                    TComponent* lComp = InEntity.TryGet<TComponent>();
                    if (lComp == nullptr)
                    {
                        return false;
                    }

                    TDrawer lDrawer;
                    lDrawer.Draw(*lComp);
                    return true;
                });
        }

        // =============================================================================
        // Get - Set
    public:
        /** @return The registered drawers in registration order (= display order). */
        const TDynArray<DrawerEntry>& Entries() const noexcept { return m_Entries; }

        /** @return How many drawers were registered — same signature EditorRoute had, so the seal log is unchanged. */
        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Entries.size()); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<DrawerEntry> m_Entries;
    };
}
