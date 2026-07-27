#include "Editor/Panels/InspectorPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorSelection.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"

#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    InspectorPanel::InspectorPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    InspectorPanel::~InspectorPanel() = default;

    void InspectorPanel::Draw()
    {
        ImGui::SetNextWindowSize(ImVec2(320.f, 400.f), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_Title.CStr());

        // A local COPY of the handle: Entity is a value type, and copying it once keeps the whole draw
        // reading one consistent selection even if a drawer were to change it.
        Entity lSelected = m_Context.Selection.Get();
        if (!lSelected.IsValid())
        {
            ImGui::TextDisabled("Nothing selected.");
            ImGui::End();
            return;
        }

        if (const EntityMeta* lMeta = lSelected.TryGet<EntityMeta>())
        {
            ImGui::Text("%s", lMeta->Name.CStr());
        }
        ImGui::Separator();

        // Ask every registered drawer whether it applies, rather than asking the entity what it has —
        // the inversion that keeps this panel ignorant of every component type (DrawerRegistry).
        bool lAnyDrawn = false;
        for (const DrawerEntry& lEntry : m_Context.Extensions.Drawers().Entries())
        {
            if (lEntry.Invoke && lEntry.Invoke(lSelected))
            {
                lAnyDrawn = true;
            }
        }

        if (!lAnyDrawn)
        {
            ImGui::TextDisabled("No drawable components.");
        }

        ImGui::End();
    }
}
