#include "Editor/Panels/DataAssetPanel.h"

#include <imgui.h>

#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Operation/DataAssetOperations.h"
#include "Editor/Properties/WidgetPropertyVisitor.h"
#include "Editor/Resources/Types/DataAsset/EditorDataAssetDocument.h"
#include "Engine/Subsystems/Resources/Types/DataAsset/DataAsset.h"

namespace Opaax::Editor
{
    DataAssetPanel::DataAssetPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    DataAssetPanel::~DataAssetPanel() = default;

    void DataAssetPanel::DrawContents()
    {
        if (!m_Context.DataAssetDocument.IsOpen())
        {
            ImGui::TextDisabled("No data asset open.");
            ImGui::TextDisabled("Double-click a .opaaxdata in the Resource Browser.");
            return;
        }

        DrawHeader();
        ImGui::Separator();

        if (m_Context.DataAssetDocument.IsTypeKnown())
        {
            DrawFields();
        }
        else
        {
            DrawUnknownType();
        }
    }

    void DataAssetPanel::DrawHeader()
    {
        const EditorDataAssetDocument& lDocument = m_Context.DataAssetDocument;
        const bool                     bDirty    = lDocument.IsDirty();

        ImGui::Text("%s%s", lDocument.FileName().CStr(), bDirty ? " *" : "");
        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 46.f);

        ImGui::BeginDisabled(!bDirty || !lDocument.IsTypeKnown());
        if (ImGui::SmallButton("Save"))
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_SAVE_DATA_ASSET, m_Context);
        }
        ImGui::EndDisabled();

        ImGui::TextDisabled("%s", lDocument.TypeName().IsValid() ? lDocument.TypeName().CStr() : "(no type)");
    }

    void DataAssetPanel::DrawFields()
    {
        EditorDataAssetDocument& lDocument = m_Context.DataAssetDocument;

        // Taken before drawing: the drawers write straight into the value.
        const nlohmann::json lBeforeThisFrame = lDocument.CurrentJson();

        WidgetPropertyVisitor lVisitor(m_Context.Widgets);
        lDocument.GetObject()->Visit(lVisitor);

        // Like the Inspector: an undo step opens when an item becomes active and closes when none is.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive && !m_bWasItemActive)
        {
            m_GestureBefore = lBeforeThisFrame;
            m_bGestureOpen  = true;
        }
        else if (!lItemActive && m_bWasItemActive && m_bGestureOpen)
        {
            DataAssetOps::CommitEdit(m_Context, m_GestureBefore);
            m_GestureBefore = nlohmann::json();
            m_bGestureOpen  = false;
        }

        m_bWasItemActive = lItemActive;
    }

    void DataAssetPanel::DrawUnknownType()
    {
        ImGui::TextWrapped("No module registered '%s', so this asset is read-only. Register the struct with "
                           "DataAssets().Register<T>() in the game module.",
                           m_Context.DataAssetDocument.TypeName().CStr());
        ImGui::Separator();
        ImGui::TextUnformatted(m_Context.DataAssetDocument.CurrentJson().dump(4).c_str());
    }
}
