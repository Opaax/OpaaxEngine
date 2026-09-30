#pragma once

#include "Core/Log/Logger.h"
#include "Core/Config/IConfig.h"          // ConfigTypeID
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/ConfigChangeTracker.h"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax
{
    OPAAX_LOG_CATEGORY(ConfigPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ConfigPanel — every registered config on the left, the current one on the right (like Unreal's
    //   Project Settings). The list is read every frame (configs can register at any time).
    //   The right side uses a registered drawer, or shows IConfig::ToText() for unknown configs.
    //   Dirty is derived from the text.
    // =============================================================================
    class ConfigPanel final : public IEditorPanel
    {
        // =============================================================================
        // Statics
        // =============================================================================
    public:
        OPAAX_EDITOR_PANEL_NAME(Config);
        
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
        /** The config list. Sets m_Current on click. */
        void DrawList();

        /** InConfig's name, file and values. */
        void DrawCurrent(IConfig& InConfig);

        /** The Save button. */
        void DrawSaveBar(IConfig& InConfig, const OpaaxString& InCurrentText);

        /**
         * The config shown on the right, looked up by id every frame. Falls back to the first one.
         */
        IConfig* ResolveCurrent() const;

        // =============================================================================
        // Override
        // =============================================================================
    public:
        //~Begin IEditorPanel interface
        /** Nothing to acquire. */
        void Startup()     override {}

        /** Nothing the world render depends on. */
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

        // The current config's id. Zero until something is clicked (then the first one is shown).
        ConfigTypeID m_Current = 0;

        // What the right side last showed, logged once per change.
        ConfigTypeID m_Reported = 0;

        // The config's text when selected or last saved (the dirty baseline).
        OpaaxString m_Baseline;

        // Decides when an edit is announced (once committed).
        ConfigChangeTracker m_ChangeTracker;
    };
}
