#pragma once

#include "Core/Config/IConfig.h"                // the second subject
#include "Core/OpaaxTypes.h"                    // TFunction, TDynArray, Uint64
#include "World/Entity/Entity.h"                // Entity::TryGet
#include "UI/UIWidget.h"                        // the third subject
#include "Engine/Modules/ModuleRegistrar.h"     // DeriveTypeLeafName (a section's label)
#include "Editor/Properties/PropertyDrawers.h"  // the built-in widgets

#include "Editor/UI/IEditorWidgets.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(DrawerRegistry);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // CContextDrawer — a drawer that also wants the subject it belongs to and the editor context
    //   (typically to dispatch a command). Optional and detected, so plain drawers are unchanged.
    // =============================================================================
    template<typename TDrawer, typename TDrawable, typename TSubject>
    concept CContextDrawer = requires(TDrawer InDrawer, IEditorWidgets& InWidgets, TDrawable& InDrawable,
                                      TSubject& InSubject, EditorContext& InContext)
    {
        InDrawer.Draw(InWidgets, InDrawable, InSubject, InContext);
    };

    // =============================================================================
    // TDrawerResolver<TSubject, TTarget> — whether a drawer applies to a subject, and what it draws.
    //   Declared, never defined: an unknown subject is a compile error. One specialization per subject
    //   (components, configs, UI widgets) lets a single registry serve them all.
    //     DrawableType   what the drawer operates on (a component is the target; a config holds its data)
    //     Resolve        null when the entry does not apply to that subject
    //     bDrawsSection  whether the entry draws its own header (the Inspector stacks many components;
    //                    the Config panel already names the config)
    // =============================================================================
    template<typename TSubject, typename TTarget>
    struct TDrawerResolver;

    /** Components: entt answers whether the entity has one. */
    template<typename TTarget>
    struct TDrawerResolver<Entity, TTarget>
    {
        using DrawableType = TTarget;

        static constexpr bool bDrawsSection = true;

        static DrawableType* Resolve(Entity& InSubject) { return InSubject.TryGet<TTarget>(); }
    };

    /**
     * UI widgets: a widget arrives as a base reference, so the check is a cast. No section header:
     * the panel already names the type above the fields.
     */
    template<typename TTarget>
    struct TDrawerResolver<UIWidget, TTarget>
    {
        using DrawableType = TTarget;

        static constexpr bool bDrawsSection = false;

        static DrawableType* Resolve(UIWidget& InSubject) { return dynamic_cast<TTarget*>(&InSubject); }
    };

    /** Configs: a config knows its own type id, so the check is an integer compare. */
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
    // TDrawerRegistry — the storage behind Drawers(), ConfigDrawers() and WidgetDrawers().
    //   Registration wraps <TTarget, TDrawer> into a bool(TSubject&) closure that checks itself through
    //   TDrawerResolver. So the Inspector never asks an entity what components it has: it asks every
    //   drawer whether it applies. Registration order is display order.
    //   Register<TTarget, TDrawer>() uses hand-written UI; Register<TTarget>() builds a default drawer
    //   from the type's property list (OPAAX_PROPERTIES).
    //   A TDrawer needs no base class: default-constructible with
    //   void Draw(IEditorWidgets&, DrawableType&). Example: EditorImguiConfigDrawer.
    //   Registration only stores; nothing is constructed (it runs before any world exists).
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
            // The ID scope is needed even here (see the generic form below).
            const OpaaxStringID lName = DeriveTypeLeafName<typename TDrawerResolver<TSubject, TTarget>::DrawableType>();

            m_TargetTypeIds.emplace_back(TypeIdOf<TTarget>());
            m_Entries.emplace_back(
                [lName](TSubject& InSubject, IEditorWidgets& InWidgets, EditorContext& InContext) -> bool
                {
                    using Resolver = TDrawerResolver<TSubject, TTarget>;

                    typename Resolver::DrawableType* lDrawable = Resolver::Resolve(InSubject);
                    if (lDrawable == nullptr)
                    {
                        return false;
                    }

                    InWidgets.PushId(lName.CStr());
                    TDrawer lDrawer;

                    // The wider form only when the drawer asks for it.
                    if constexpr (CContextDrawer<TDrawer, typename Resolver::DrawableType, TSubject>)
                    {
                        lDrawer.Draw(InWidgets, *lDrawable, InSubject, InContext);
                    }
                    else
                    {
                        lDrawer.Draw(InWidgets, *lDrawable);
                    }

                    InWidgets.PopId();

                    return true;
                });
        }

        /**
         * The default drawer, built from the type's own property list (CReflected). A field type with no
         * TPropertyDrawer fails to compile here, naming the type.
         */
        template<typename TTarget>
        requires CReflected<typename TDrawerResolver<TSubject, TTarget>::DrawableType>
        void Register()
        {
            using Resolver = TDrawerResolver<TSubject, TTarget>;

            // Derived once: DeriveTypeLeafName also names the component in a .opaaxmap, so the Inspector
            // header and the saved name match.
            const OpaaxStringID lName = DeriveTypeLeafName<typename Resolver::DrawableType>();

            m_TargetTypeIds.emplace_back(TypeIdOf<TTarget>());
            m_Entries.emplace_back(
                [lName](TSubject& InSubject, IEditorWidgets& InWidgets, EditorContext&) -> bool
                {
                    typename Resolver::DrawableType* lDrawable = Resolver::Resolve(InSubject);
                    if (lDrawable == nullptr)
                    {
                        return false;
                    }

                    InWidgets.PushId(lName.CStr());

                    if constexpr (Resolver::bDrawsSection)
                    {
                        if (InWidgets.CollapsingHeader(lName.CStr()))
                        {
                            DrawProperties(InWidgets, *lDrawable);
                        }
                    }
                    else
                    {
                        DrawProperties(InWidgets, *lDrawable);
                    }

                    InWidgets.PopId();

                    return true;
                });
        }

        /**
         * An entry built without the target's C++ type (e.g. from a type-erased registry entry).
         * @param InTargetTypeId TypeIdOf the target, so HasTarget knows it is covered
         */
        void RegisterErased(TypeId InTargetTypeId, TFunction<bool(TSubject&, IEditorWidgets&, EditorContext&)> InEntry)
        {
            m_TargetTypeIds.emplace_back(InTargetTypeId);
            m_Entries.emplace_back(Move(InEntry));
        }

        /** Whether a drawer is already registered for this target type. */
        bool HasTarget(const TypeId InTargetTypeId) const noexcept
        {
            for (const TypeId lId : m_TargetTypeIds)
            {
                if (lId == InTargetTypeId) { return true; }
            }
            return false;
        }

        /**
         * Draws the first applicable entry.
         * @return False when nothing applied (the Config panel then falls back to its json view)
         */
        bool DrawFirst(TSubject& InSubject, IEditorWidgets& InWidgets, EditorContext& InContext) const
        {
            for (const TFunction<bool(TSubject&, IEditorWidgets&, EditorContext&)>& lEntry : m_Entries)
            {
                if (lEntry && lEntry(InSubject, InWidgets, InContext)) { return true; }
            }

            return false;
        }

        // =============================================================================
        // Get - Set
    public:
        /** The registered drawers, in registration (display) order. */
        const TDynArray<TFunction<bool(TSubject&, IEditorWidgets&, EditorContext&)>>& Entries() const noexcept { return m_Entries; }

        Uint64 Count() const noexcept { return static_cast<Uint64>(m_Entries.size()); }
        // End Get - Set
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        TDynArray<TFunction<bool(TSubject&, IEditorWidgets&, EditorContext&)>> m_Entries;

        /** Parallel to m_Entries: the target type each entry draws. */
        TDynArray<TypeId> m_TargetTypeIds;
    };

    // Named per use: Drawers() / ConfigDrawers().
    using ComponentDrawerRegistry = TDrawerRegistry<Entity>;
    using UIWidgetDrawerRegistry  = TDrawerRegistry<UIWidget>;
    using ConfigDrawerRegistry    = TDrawerRegistry<IConfig>;
}
