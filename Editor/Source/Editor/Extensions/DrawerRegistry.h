#pragma once

#include "Core/OpaaxTypes.h"                    // TFunction, TDynArray, Uint64
#include "World/Entity/Entity.h"                // Entity::TryGet — the closure's self-check
#include "Engine/Modules/ModuleRegistrar.h"     // DeriveTypeLeafName — the generic header's label
#include "Editor/Properties/PropertyDrawers.h"  // the built-in widgets, so every call site has them

#include <imgui.h>

namespace Opaax
{
    OPAAX_LOG_CATEGORY(DrawerRegistry);
}

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
    //   whatever presentation the entry needs — a hand-written drawer's own CollapsingHeader, or the
    //   generic form's derived one — is closed over at registration, so the STORAGE never grows a
    //   display name and the M0 call-site shape Register<TComponent, TDrawer>() stays unchanged
    //   (MR1: the call-site API was final, only the body becomes real).
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
    //   That inversion is why the Inspector needs no entt introspection. It never asks "what components
    //   does this entity have?" — an answer entt cannot give in typed form without a type registry. It
    //   asks every registered drawer "are you applicable?", and each one answers for itself.
    //   Registration order is display order. The property list (I15) does not change that: a component
    //   describes ITS OWN fields, and nothing ever asks an entity what it holds.
    //
    //   TWO FORMS, one call. Register<TComponent, TDrawer>() is the hand-written UI;
    //   Register<TComponent>() is the DEFAULT drawer folded from the component's own property list
    //   (OPAAX_PROPERTIES). The generic form exists because a component that is two floats should not
    //   need a file of its own; the custom form stays for fields that need judgment — a tag picker,
    //   a curve. Same storage, same self-check, same display order.
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

        /**
         * The DEFAULT drawer, built from what the component says its fields are (CReflected).
         *
         * The one-argument form of the call above, so choosing between "generic" and "my own UI" is
         * one template argument rather than a second route. A field type nobody wrote a
         * TPropertyDrawer for fails to compile HERE, naming the type.
         */
        template<CReflected TComponent>
        void Register()
        {
            // Derived once, at registration: DeriveTypeLeafName is the same function that produced
            // this component's key in a .opaaxmap, so the Inspector header and the on-disk name are
            // one naming rule rather than two that can drift.
            const OpaaxStringID lName = DeriveTypeLeafName<TComponent>();

            OPAAX_LOG(LogDrawerRegistry, Info, "Generic drawer: {} ({} properties)",
                      lName, PropertyCount<TComponent>());

            m_Entries.emplace_back(
                [lName](Entity& InEntity) -> bool
                {
                    TComponent* lComp = InEntity.TryGet<TComponent>();
                    if (lComp == nullptr)
                    {
                        return false;
                    }

                    // The header is the generic drawer's job because a hand-written one owns its
                    // own (that is why the registry needs no display name for those).
                    if (ImGui::CollapsingHeader(lName.CStr(), ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        DrawProperties(*lComp);
                    }

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
