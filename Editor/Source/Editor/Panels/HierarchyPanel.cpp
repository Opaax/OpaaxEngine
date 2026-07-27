#include "Editor/Panels/HierarchyPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/EditorSelection.h"

#include "World/WorldManager.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    HierarchyPanel::HierarchyPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    HierarchyPanel::~HierarchyPanel() = default;

    void HierarchyPanel::Draw()
    {
        ImGui::SetNextWindowSize(ImVec2(260.f, 400.f), ImGuiCond_FirstUseEver);
        ImGui::Begin(m_Title.CStr());

        World* lWorld = m_Context.Worlds.GetActiveWorld();
        if (lWorld == nullptr)
        {
            ImGui::TextDisabled("No active world.");
            ImGui::End();
            return;
        }

        // Compare by handle: Entity is a value handle, so the selected one is a COPY of the row's entity,
        // never the same object. The handle is the identity.
        const EntityID lSelected = m_Context.Selection.Get().GetHandle();
        Uint64         lCount    = 0;

        lWorld->Each<EntityMeta>([&](EntityID InId, EntityMeta& InMeta)
        {
            ++lCount;

            // Names are a debug label and may repeat; the handle is what makes each row's ImGui ID unique.
            ImGui::PushID(static_cast<int>(static_cast<Uint32>(InId)));
            if (ImGui::Selectable(InMeta.Name.CStr(), InId == lSelected))
            {
                m_Context.Selection.Select(Entity{InId, lWorld});

                // Discrete (a click), so no spam — and it is the only observable signal that selection
                // actually moved, until the Inspector (M2b) renders it.
                OPAAX_LOG(LogHierarchyPanel, Info, "Hierarchy selected '{}'", InMeta.Name.CStr())
            }
            ImGui::PopID();
        });

        if (lCount == 0)
        {
            ImGui::TextDisabled("World is empty.");
        }
        else if (!m_bListLogged)
        {
            OPAAX_LOG(LogHierarchyPanel, Info, "Hierarchy listing {} entities from world '{}'", lCount, lWorld->GetName().CStr())
            m_bListLogged = true;
        }

        ImGui::End();
    }
}
