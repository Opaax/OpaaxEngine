#pragma once

#include "Core/Log/Logger.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Panels/ConfigChangeTracker.h"
#include "Editor/Panels/IEditorPanel.h"

namespace Opaax
{
    class Config;
    class ObjectClass;

    OPAAX_LOG_CATEGORY(ConfigPanel);
}

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // ConfigPanel — every registered config on the left, the current one on the right (like Unreal's
    //   Project Settings). The list is read every frame (configs can register at any time).
    //   The right side draws the config's values from their properties, or shows its text when they
    //   have none. Dirty is derived from the text.
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
        void DrawCurrent(Config& InConfig);

        /** The Save and Reset to Defaults buttons. */
        void DrawSaveBar(Config& InConfig, const OpaaxString& InCurrentText);

        /**
         * The config shown on the right, looked up by class every frame. Falls back to the first one.
         */
        Config* ResolveCurrent() const;

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

        // The current config's class. Null until something is clicked (then the first one is shown).
        const ObjectClass* m_Current = nullptr;

        // What the right side last showed, logged once per change.
        const Config* m_Reported = nullptr;

        // The config's text when selected or last saved (the dirty baseline).
        OpaaxString m_Baseline;

        // Decides when an edit is announced (once committed).
        ConfigChangeTracker m_ChangeTracker;
    };
}
