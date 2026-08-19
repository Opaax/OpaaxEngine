#pragma once

#include "Application/Services/ILogger.h"
#include "Core/Config/IConfig.h"   // ConfigTypeID — which config is current
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ConfigPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ConfigPanel — every registered config on the left, the current one on the right (the shape
    //   Unreal's Project Settings uses).
    //
    //   THE LIST IS READ LIVE, every frame, and that is the requirement rather than a shortcut:
    //   IConfigSystem::Get<T>() auto-registers on a miss, so a system that reads its config during
    //   FinishStartup — or on frame 500 — registers long after the editor sealed its extensions. A
    //   list built once would miss those silently. Walking it costs nothing while the panel is shut.
    //
    //   READ-ONLY for now. The right pane draws IConfig::ToText(), the same text Save writes, which
    //   is the one way to render a config whose data type this panel cannot name. Per-field widgets
    //   wait on the reflection system; this panel is the shell they land in.
    // =============================================================================
    class ConfigPanel final : public IEditorPanel
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        explicit ConfigPanel(EditorContext& InContext);
        ~ConfigPanel() override = default;

        // =============================================================================
        // Copy delete
        // =============================================================================
        ConfigPanel(const ConfigPanel&)            = delete;
        ConfigPanel& operator=(const ConfigPanel&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /** The list of configs. Sets m_Current when one is clicked. */
        void DrawList();

        /** Name, file and values of InConfig. */
        void DrawCurrent(const IConfig& InConfig);

        /**
         * The config the right pane draws.
         *
         * Resolved through the registry every frame rather than held as a pointer — the id survives
         * anything the registry does, a pointer would not. Falls back to the first registered config,
         * so the pane is never blank while any config exists.
         */
        const IConfig* ResolveCurrent() const;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire — the registry is reached through the context. */
        void Startup()     override {}

        /** Nothing the world's render depends on. */
        void OnPreRender() override {}

        void DrawContents() override;

        /** Nothing to release. */
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 640.f, 420.f } }; }
        //~End IEditorPanel interface

        // =============================================================================
        // Members
        // =============================================================================
    private:
        EditorContext& m_Context;

        /** Width of the list pane. */
        static constexpr float LIST_WIDTH = 160.f;

        // Which config is current, by id rather than by pointer. Zero until something is clicked,
        // which ResolveCurrent reads as "the first one".
        ConfigTypeID m_Current = 0;

        // What the right pane last reported. A pane that resolved nothing draws an empty rectangle,
        // which is indistinguishable from a clean run in a log — so the SUCCESS branch says which
        // config it is showing, once per change (**L15**).
        ConfigTypeID m_Reported = 0;
    };
}
