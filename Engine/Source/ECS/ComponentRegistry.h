#pragma once

#include <Entt/entt/single_include/entt/entt.hpp>

#include "Core/EngineAPI.h"
#include "Core/OpaaxTypes.h"
#include "Core/OpaaxStringID.hpp"
#include "Core/Component/OpaaxComponent.h"
#include "Core/Container/TPolymorphicList.hpp"
#include "Core/Log/OpaaxLog.h"
#include "World/World.h"

#if OPAAX_WITH_EDITOR
#include "Editor/Inspector/IComponentDrawer.h"
#endif

namespace Opaax
{
    // =============================================================================
    // IComponentEntry
    //
    // Type-erased view of one registered component. Each ComponentEntry<T> below
    // implements this with template-baked Has/Add/Save/Load calls. The base lets
    // ComponentRegistry store every type in a single TPolymorphicList without
    // dragging templates into the storage container.
    // =============================================================================
    class OPAAX_API IComponentEntry
    {
    public:
        virtual ~IComponentEntry() = default;

        virtual OpaaxStringID GetName()      const = 0;
        virtual entt::id_type GetTypeId()    const = 0;
        virtual bool          ShouldShowInAddMenu() const = 0;

        virtual bool Has (const World& InWorld, EntityID InEntity) const                = 0;
        virtual void Add (World& InWorld, EntityID InEntity)       const                = 0;
        virtual json Save(const World& InWorld, EntityID InEntity) const                = 0;
        virtual void Load(World& InWorld, EntityID InEntity, const json& InJson) const  = 0;

#if OPAAX_WITH_EDITOR
        // Optional per-type custom drawer. When null, ComponentRegistry::DrawAll
        // falls back to a generic read-only renderer that walks the component's
        // Serialize() json.
        virtual Editor::IComponentDrawer* GetCustomDrawer() const                       = 0;
        virtual void                      SetCustomDrawer(UniquePtr<Editor::IComponentDrawer> InDrawer) = 0;
#endif
    };

    // =============================================================================
    // ComponentEntry<T>
    //
    // Template-baked concrete entry. Has/Add/Save/Load wire to T's
    // OpaaxComponentBase virtuals.
    // =============================================================================
    template<typename T>
    class ComponentEntry final : public IComponentEntry
    {
        static_assert(std::is_base_of_v<OpaaxComponentBase, T>,
            "ComponentEntry<T> — T must derive from OpaaxComponentBase");

    public:
        ComponentEntry(const char* InName, bool bShowInAddMenu)
            : m_Name(InName), m_bShowInAddMenu(bShowInAddMenu)
        {}

        OpaaxStringID GetName()             const override { return m_Name; }
        entt::id_type GetTypeId()           const override { return entt::type_hash<T>::value(); }
        bool          ShouldShowInAddMenu() const override { return m_bShowInAddMenu; }

        bool Has(const World& InWorld, EntityID InEntity) const override
        {
            return InWorld.HasComponent<T>(InEntity);
        }

        void Add(World& InWorld, EntityID InEntity) const override
        {
            if (!InWorld.HasComponent<T>(InEntity))
            {
                InWorld.AddComponent<T>(InEntity);
            }
        }

        json Save(const World& InWorld, EntityID InEntity) const override
        {
            const T* lComp = InWorld.GetComponent<T>(InEntity);
            return lComp ? lComp->Serialize() : json{};
        }

        void Load(World& InWorld, EntityID InEntity, const json& InJson) const override
        {
            T& lComp = InWorld.HasComponent<T>(InEntity)
                ? *InWorld.GetComponent<T>(InEntity)
                : InWorld.AddComponent<T>(InEntity);
            lComp.Deserialize(InJson);
        }

#if OPAAX_WITH_EDITOR
        Editor::IComponentDrawer* GetCustomDrawer() const override { return m_CustomDrawer.get(); }

        void SetCustomDrawer(UniquePtr<Editor::IComponentDrawer> InDrawer) override
        {
            m_CustomDrawer = std::move(InDrawer);
        }
#endif

    private:
        OpaaxStringID m_Name;
        bool          m_bShowInAddMenu;

#if OPAAX_WITH_EDITOR
        UniquePtr<Editor::IComponentDrawer> m_CustomDrawer;
#endif
    };

    // =============================================================================
    // ComponentRegistry
    //
    // Single source of truth for every component type the engine knows about.
    // Replaces both the hardcoded SceneSerializer type list and the editor's
    // ComponentDrawerRegistry. Engine registers built-ins in CoreEngineApp::Initialize;
    // games register their own in their OnInitialize.
    //
    // Inherits TPolymorphicList<IComponentEntry> for storage + Clear/GetAll.
    //
    // The same registry powers:
    //   - SceneSerializer (save/load on disk + PIE snapshot/restore)
    //   - Inspector panel ("Add Component" menu + drawer dispatch)
    //
    // Each registered type T must derive from OpaaxComponentBase (compile-time check).
    // =============================================================================
    class OPAAX_API ComponentRegistry : public TPolymorphicList<IComponentEntry>
    {
        // =============================================================================
        // Registration
        // =============================================================================
    public:
        /**
         * Register a component type. Has/Add/Save/Load are auto-generated from T's
         * OpaaxComponentBase virtuals. Re-registering the same type is a no-op + warn.
         */
        template<typename T>
        static void Register(const char* InName, bool bShowInAddMenu = true)
        {
            static_assert(std::is_base_of_v<OpaaxComponentBase, T>,
                "ComponentRegistry::Register — T must derive from OpaaxComponentBase");

            const entt::id_type lTypeId = entt::type_hash<T>::value();
            if (FindByTypeId(lTypeId) != nullptr)
            {
                OPAAX_CORE_WARN("ComponentRegistry::Register — '{}' already registered, skipping.", InName);
                return;
            }

            TPolymorphicList<IComponentEntry>::Register(
                MakeUnique<ComponentEntry<T>>(InName, bShowInAddMenu));

            OPAAX_CORE_TRACE("ComponentRegistry: registered '{}'.", InName);
        }

#if OPAAX_WITH_EDITOR
        /**
         * Attach a custom editor drawer to a previously-registered type. Warns and
         * skips if T isn't registered yet (registration must precede drawer attach).
         */
        template<typename T>
        static void RegisterDrawer(UniquePtr<Editor::IComponentDrawer> InDrawer)
        {
            const entt::id_type lTypeId = entt::type_hash<T>::value();
            IComponentEntry* lEntry = FindByTypeIdMutable(lTypeId);
            if (!lEntry)
            {
                OPAAX_CORE_WARN("ComponentRegistry::RegisterDrawer — type not registered, drawer dropped.");
                return;
            }
            lEntry->SetCustomDrawer(std::move(InDrawer));
        }
#endif

        // =============================================================================
        // Lookup / iteration
        // =============================================================================
    public:
        static const IComponentEntry* FindByName  (OpaaxStringID InName);
        static const IComponentEntry* FindByTypeId(entt::id_type InTypeId);
        static IComponentEntry*       FindByTypeIdMutable(entt::id_type InTypeId);

        // Iterates in registration order (engine built-ins first, game extensions after).
        // Used by SceneSerializer (save) and Inspector (drawer dispatch).
        template<typename TFn>
        static void ForEach(TFn&& InFn)
        {
            for (const auto& lEntry : GetAll())
            {
                InFn(*lEntry);
            }
        }

        // =============================================================================
        // Editor drawer dispatch
        // =============================================================================
#if OPAAX_WITH_EDITOR
    public:
        // Iterates registered components; for each present on InEntity, dispatches
        // to its CustomDrawer if set, else renders a default read-only json view.
        static void DrawAll(World& InWorld, EntityID InEntity);

        // Iterates registered components; renders a MenuItem for each that allows
        // Add (ShouldShowInAddMenu == true) and isn't already on InEntity.
        static void DrawAddComponentMenu(World& InWorld, EntityID InEntity);
#endif

        // Clear() / GetAll() / Register() inherited from TPolymorphicList.
    };
}
