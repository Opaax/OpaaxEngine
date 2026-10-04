#pragma once

#include <nlohmann/json.hpp>

#include "Editor/Panels/IEditorPanel.h"

namespace Opaax::Editor
{
    struct EditorContext;

    // =============================================================================
    // DataAssetPanel — edits the open .opaaxdata, whatever its type: its fields are drawn from the
    //   registered struct's property list, with undo and Save.
    // =============================================================================
    class DataAssetPanel final : public IEditorPanel
    {
    public:
        OPAAX_EDITOR_PANEL_NAME(Data Asset);

        explicit DataAssetPanel(EditorContext& InContext);
        ~DataAssetPanel() override;

        DataAssetPanel(const DataAssetPanel&)            = delete;
        DataAssetPanel& operator=(const DataAssetPanel&) = delete;

    private:
        /** Name, type, dirty marker and Save. */
        void DrawHeader();

        /** The fields, with undo, committed through DataAssetOps. */
        void DrawFields();

        /** An unregistered type: the raw data, read-only. */
        void DrawUnknownType();

    public:
        //~Begin IEditorPanel interface
        void Startup()     override {}
        void OnPreRender() override {}
        void DrawContents() override;
        void Shutdown()    override {}

        PanelWindowStyle GetWindowStyle() const override { return { { 380.f, 400.f } }; }
        //~End IEditorPanel interface

    private:
        EditorContext& m_Context;

        // The open edit gesture: the value as it was when the first field became active.
        nlohmann::json m_GestureBefore;
        bool           m_bGestureOpen   = false;
        bool           m_bWasItemActive = false;
    };
}
