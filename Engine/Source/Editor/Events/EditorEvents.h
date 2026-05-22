#pragma once
#include <utility>

#include "Core/Event/OpaaxEvent.hpp"
#include "Core/OpaaxString.hpp"
#include "Core/OpaaxStringID.hpp"
#include "ECS/OpaaxEntity.hpp"

// =============================================================================
// EditorEvents.h
//
// All editor-side event classes dispatched through EditorEventBus.
// Editor-only — game-layer code must NOT publish or subscribe to these
// (call sites are gated by OPAAX_WITH_EDITOR).
//
// Each class uses the existing OPAAX_EVENT_CLASS_TYPE / CATEGORY macros so it
// can also flow through the stack-local OpaaxEventDispatcher if a panel needs
// classic single-dispatch handling.
// =============================================================================

namespace Opaax
{
    // =============================================================================
    // Selection
    // =============================================================================

    /**
     * @class OnEntitySelectedEvent
     * Fired when the user picks an entity in the editor (e.g. HierarchyPanel click).
     * Subscribers cache the entity; the bus does no retention.
     */
    class OPAAX_API OnEntitySelectedEvent final : public OpaaxEvent
    {
    public:
        explicit OnEntitySelectedEvent(EntityID InEntity) noexcept
            : m_Entity(InEntity)
        {}

        FORCEINLINE EntityID GetEntity() const noexcept { return m_Entity; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::EntitySelected)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Editor)

    private:
        EntityID m_Entity;
    };

    // =============================================================================
    // Scene
    // =============================================================================

    /**
     * @class OnSceneSavedEvent
     * Fired after a successful scene write (Save As path). Payload is the absolute
     * file path so consumers (e.g. AssetBrowserPanel) can scope a rescan.
     */
    class OPAAX_API OnSceneSavedEvent final : public OpaaxEvent
    {
    public:
        explicit OnSceneSavedEvent(OpaaxString InPath) noexcept
            : m_Path(std::move(InPath))
        {}

        FORCEINLINE const OpaaxString& GetPath() const noexcept { return m_Path; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::SceneSaved)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Editor)

    private:
        OpaaxString m_Path;
    };

    /**
     * @class OnNewSceneEvent
     * Fired when the active scene is replaced (New / Open / Replace). No payload —
     * subscribers re-fetch from SceneManager at receipt. Gated by OPAAX_WITH_EDITOR
     * at the SceneManager publish site so shipped game binaries don't carry the call.
     */
    class OPAAX_API OnNewSceneEvent final : public OpaaxEvent
    {
    public:
        OnNewSceneEvent() noexcept = default;

        OPAAX_EVENT_CLASS_TYPE(EEventType::NewScene)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Editor)
    };

    // =============================================================================
    // Asset
    // =============================================================================

    /**
     * @class OnAssetImportedEvent
     * Fired when a new asset enters the registry (import action). AssetID is the
     * canonical logical ID; Path is the on-disk location for browsers/inspectors.
     */
    class OPAAX_API OnAssetImportedEvent final : public OpaaxEvent
    {
    public:
        OnAssetImportedEvent(OpaaxStringID InAssetID, OpaaxString InPath) noexcept
            : m_AssetID(InAssetID), m_Path(std::move(InPath))
        {}

        FORCEINLINE OpaaxStringID         GetAssetID() const noexcept { return m_AssetID; }
        FORCEINLINE const OpaaxString&    GetPath()    const noexcept { return m_Path; }

        OPAAX_EVENT_CLASS_TYPE(EEventType::AssetImported)
        OPAAX_EVENT_CLASS_CATEGORY(EEventCategory_Editor)

    private:
        OpaaxStringID m_AssetID;
        OpaaxString   m_Path;
    };
}
