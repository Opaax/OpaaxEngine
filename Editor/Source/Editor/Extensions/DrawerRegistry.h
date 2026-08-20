#pragma once

#include "Core/Config/IConfig.h"                // the second subject
#include "Core/OpaaxTypes.h"                    // TFunction, TDynArray, Uint64
#include "World/Entity/Entity.h"                // Entity::TryGet — the component resolver
#include "Engine/Modules/ModuleRegistrar.h"     // DeriveTypeLeafName — a section's label
#include "Editor/Properties/PropertyDrawers.h"  // the built-in widgets, so every call site has them

#include <imgui.h>

namespace Opaax
{
    OPAAX_LOG_CATEGORY(DrawerRegistry);
}

namespace Opaax::Editor
{
    // =============================================================================
    // TDrawerResolver<TSubject, TTarget> — "given one of these, is this drawer applicable, and what
    //   does it draw?". DECLARED, NEVER DEFINED: a subject nobody taught the registry about is a
    //   compile error, not an empty list.
    //
    //   It is the ONE customization point that makes a single registry serve components and configs
    //   — and whatever comes next is one more specialization rather than a third registry.
    //
    //   Three members, and each earns its place:
    //     DrawableType   what the properties/drawer actually operate on (a component IS the target;
    //                    a config HOLDS its data)
    //     Resolve        null when this entry does not apply to that subject
    //     bDrawsSection  whether the entry frames itself with a header. The Inspector stacks many
    //                    components in one panel, so each needs its own; the Config panel already
    //                    names the config above the fields, so a second header would be noise.
    // =============================================================================
    template<typename TSubject, typename TTarget>
    struct TDrawerResolver;

    /** Components: entt answers "does this entity have one?" — the Inspector never asks the reverse. */
    template<typename TTarget>
    struct TDrawerResolver<Entity, TTarget>
    {
        using DrawableType = TTarget;

        static constexpr bool bDrawsSection = true;

        static DrawableType* Resolve(Entity& InSubject) { return InSubject.TryGet<TTarget>(); }
    };

    /** Configs: a config knows its own type id, so the check is an integer compare, not a cast. */
    template<typename TTarget>
    struct TDrawerResolver<IConfig, TTarget>
    {
        using DrawableType = typename TTarget::DataType;

        static constexpr bool bDrawsSection = false;

        static DrawableType* Resolve(IConfig& InSubject)
        {
            return InSubject.GetConfigTypeID() == TTarget::StaticTypeID()
                       ? &static_cast<TTarget&>(InSubject).GetData()
                       : nullptr;
        }
    };

    // =============================================================================
    // TDrawerRegistry — the storage behind Drawers() and ConfigDrawers() (Editor.md D10). ONE class
    //   for both, because they were the same thing twice: a list of "are you applicable, and if so
    //   draw yourself" closures over some subject.
    //
    //   THE DESIGN, in one line: registration erases <TTarget, TDrawer> into a uniform
    //   bool(TSubject&) closure that self-checks through TDrawerResolver.
    //
    //   That inversion is why the Inspector needs no entt introspection. It never asks "what
    //   components does this entity have?" — an answer entt cannot give in typed form without a type
    //   registry. It asks every registered drawer "are you applicable?", and each one answers for
    //   itself. Registration order is display order. The property list (I15) does not change that: a
    //   type describes ITS OWN fields, and nothing ever asks a subject what it holds.
    //
    //   TWO FORMS, one call, for both subjects. Register<TTarget, TDrawer>() is hand-written UI;
    //   Register<TTarget>() is the DEFAULT drawer folded from the type's own property list
    //   (OPAAX_PROPERTIES). The generic form exists because a component that is two floats should not
    //   need a file of its own; the custom form stays for fields that need judgment — a tag picker,
    //   a curve.
    //
    //   DUCK-TYPED contract, checked at instantiation, with NO base class (D7 declines OOP/virtual
    //   drawers). A TDrawer must be default-constructible and callable as void Draw(DrawableType&);
    //   its body may live in a .cpp, since the closure only needs to CALL it.
    //
    //   Registration STORES ONLY; nothing is constructed. It must: RegisterExtensions runs at the
    //   OnModulesRegistered seam, before any world exists.
    // =============================================================================
    template<typename TSubject>
    class TDrawerRegistry
    {
        // =============================================================================
        // Functions
        // =============================================================================
    public:
        template<typename TTarget, typename TDrawer>
        void Register()
        {
            // Needed even here, where the drawer owns its own presentation: the ID SCOPE is not
            // presentation (see the note on the generic form below).
            const OpaaxStringID lName = DeriveTypeLeafName<typename TDrawerResolver<TSubject, TTarget>::DrawableType>();

            m_Entries.emplace_back(
                [lName](TSubject& InSubject) -> bool
                {
                    using Resolver = TDrawerResolver<TSubject, TTarget>;

                    typename Resolver::DrawableType* lDrawable = Resolver::Resolve(InSubject);
                    if (lDrawable == nullptr)
                    {
                        return false;
                    }

                    ImGui::PushID(lName.CStr());
                    TDrawer lDrawer;
                    lDrawer.Draw(*lDrawable);
                    ImGui::PopID();

                    return true;
                });
        }

        /**
         * The DEFAULT drawer, built from what the type says its fields are (CReflected).
         *
         * The one-argument form of the call above, so choosing between "generic" and "my own UI" is
         * one template argument rather than a second route. A field type nobody wrote a
         * TPropertyDrawer for fails to compile HERE, naming the type.
         */
        template<typename TTarget>
        requires CReflected<typename TDrawerResolver<TSubject, TTarget>::DrawableType>
        void Register()
        {
            using Resolver = TDrawerResolver<TSubject, TTarget>;

            // Derived once, at registration: DeriveTypeLeafName is the same function that produced a
            // component's key in a .opaaxmap, so the Inspector header and the on-disk name are one
            // naming rule rather than two that can drift.
            const OpaaxStringID lName = DeriveTypeLeafName<typename Resolver::DrawableType>();

            OPAAX_LOG(LogDrawerRegistry, Info, "Generic drawer: {} ({} properties)",
                      lName, PropertyCount<typename Resolver::DrawableType>());

            m_Entries.emplace_back(
                [lName](TSubject& InSubject) -> bool
                {
                    typename Resolver::DrawableType* lDrawable = Resolver::Resolve(InSubject);
                    if (lDrawable == nullptr)
                    {
                        return false;
                    }

                    // EVERY entry draws inside its own ID scope, keyed by the type it draws.
                    //
                    // An ImGui widget's identity is its LABEL, and a label here is a field name — so
                    // two drawables sharing a field name in one window are one id, twice. That is not
                    // an edge case: Dummy and Sprite both have Position, Size and Color by design,
                    // and the day an entity carries both, ImGui reports conflicting IDs and the two
                    // widgets fight over the active-item state.
                    //
                    // The scope belongs HERE rather than in each TPropertyDrawer: a drawer sees one
                    // field and cannot know what else the window holds, while an entry is exactly the
                    // boundary between two independently-authored types. Keyed by the type NAME, not
                    // by the registration index, so a stored open/closed header state survives
                    // someone registering another drawer before it.
                    ImGui::PushID(lName.CStr());

                    if constexpr (Resolver::bDrawsSection)
                    {
                        if (ImGui::CollapsingHeader(lName.CStr(), ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            DrawProperties(*lDrawable);
                        }
                    }
                    else
                    {
                        DrawProperties(*lDrawable);
                    }

                    ImGui::PopID();

                    return true;
                });
        }

        /**
         * Draw the first applicable entry.
         *
         * @return false when nothing applied — which is what lets a caller tell "no drawer for this"
         *   from "drew nothing", and is how the Config panel decides to fall back to its json view.
         */
        bool DrawFirst(TSubject& InSubject) const
        {
            for (const TFunction<bool(TSubject&)>& lEntry : m_Entries)
            {
                if (lEntry && lEntry(InSubject)) { return true; }
            }

            return false;
        }

        // =============================================================================
        // Get - Set
    public:
        /** The registered drawers in registration order (= display order). */
        const TDynArray<TFunction<bool(TSubject&)>>& Entries() const noexcept { return m_Entries; }

        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Entries.size()); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TFunction<bool(TSubject&)>> m_Entries;
    };

    // Explicit at the call site, as the routes read: Drawers() / ConfigDrawers().
    using ComponentDrawerRegistry = TDrawerRegistry<Entity>;
    using ConfigDrawerRegistry    = TDrawerRegistry<IConfig>;
}
