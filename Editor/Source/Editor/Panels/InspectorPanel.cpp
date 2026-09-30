#include "Editor/Panels/InspectorPanel.h"

#include <cstring>   // memcpy

#include "Editor/EditorContext.h"
#include "Editor/Commands/EditorNativeCommands.h"
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Undo/EditorUndo.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"

#include "Application/Services/IEngine.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

#include <imgui.h>

using namespace Opaax;

namespace Opaax::Editor
{
    InspectorPanel::InspectorPanel(EditorContext& InContext)
        : m_Context(InContext)
    {
    }

    InspectorPanel::~InspectorPanel() = default;

    void InspectorPanel::DrawContents()
    {
        // A copy of the handle, so the whole draw uses one selection.
        Entity lSelected = m_Context.Selection.Get();
        if (!lSelected.IsValid())
        {
            ImGui::TextDisabled("Nothing selected.");
            return;
        }

        DrawNameField(lSelected);

        // Say which entity is edited when several are selected (only the primary is shown).
        if (const Uint64 lCount = m_Context.Selection.Count(); lCount > 1)
        {
            ImGui::TextDisabled("%llu selected - editing the last picked",
                                static_cast<unsigned long long>(lCount));
        }

        ImGui::Separator();

        // Ask every registered drawer whether it applies (the panel knows no component type).
        // Every applicable drawer, not only the first.
        bool lAnyDrawn = false;
        for (const TFunction<bool(Entity&, IEditorWidgets&, EditorContext&)>& lEntry : m_Context.Extensions.Drawers().Entries())
        {
            if (lEntry && lEntry(lSelected, m_Context.Widgets, m_Context))
            {
                lAnyDrawn = true;
            }
        }

        if (!lAnyDrawn)
        {
            ImGui::TextDisabled("No drawable components.");
        }

        DrawAddComponent(lSelected);
        DrawRemoveComponent(lSelected);

        // A drawer writes a component directly, which the world cannot see; the dirty check is gated on
        // World::GetRevision(), so mark the world changed. Asked of ImGui (not the drawer), so it can
        // only over-report. The frame after also counts: a Checkbox commits on release.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive || m_bWasItemActive)
        {
            if (World* lWorld = lSelected.GetWorld()) { lWorld->MarkChanged(); }
        }

        // The undo step's two edges. The rising edge is read after the drawers ran: on activation
        // frame nothing is written yet, so the captured values are the pre-edit ones.
        // Global on purpose: a resource dragged from the Browser holds ActiveId there, so the step
        // opens before the drop and closes on it.
        if (lItemActive && !m_bWasItemActive)
        {
            m_Edit.Begin(m_Context, lSelected, EUndoWorld::Active);
        }
        else if (!lItemActive && m_bWasItemActive)
        {
            if (m_Edit.End(m_Context)) { m_Context.Undo.Record(Move(m_Edit)); }

            // Closed either way, so the next edit is not folded into it.
            m_Edit = EntityComponentsEdit{};
        }

        m_bWasItemActive = lItemActive;
    }

    void InspectorPanel::DrawNameField(Entity& InEntity)
    {
        EntityMeta* lMeta = InEntity.TryGet<EntityMeta>();
        if (lMeta == nullptr)
        {
            return;
        }

        // Refresh from the entity only while not typing (it would overwrite the keystrokes).
        if (!ImGui::IsItemActive())
        {
            const OpaaxString& lName = lMeta->Name;
            const Uint32       lLen  = lName.GetLength() < sizeof(m_NameBuffer) - 1
                                     ? lName.GetLength()
                                     : static_cast<Uint32>(sizeof(m_NameBuffer) - 1);

            std::memcpy(m_NameBuffer, lName.CStr(), lLen);
            m_NameBuffer[lLen] = '\0';
        }

        ImGui::SetNextItemWidth(-1.f);

        // Committed on Enter or when focus is lost (clicking away keeps the typed text).
        const bool lEnter = ImGui::InputText("##EntityName", m_NameBuffer, sizeof(m_NameBuffer),
                                             ImGuiInputTextFlags_EnterReturnsTrue);

        if (lEnter || ImGui::IsItemDeactivatedAfterEdit())
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_RENAME_SELECTED, m_Context,
                                                    EntityNameParams{ OpaaxString(m_NameBuffer) });
        }
    }

    void InspectorPanel::DrawRemoveComponent(Entity& InEntity)
    {
        const ComponentRegistry& lTypes    = m_Context.Engine.GetRegistries().Components();
        EntityRegistry&          lEntities = InEntity.GetWorld()->GetRegistry();
        const EntityID           lHandle   = InEntity.GetHandle();

        ImGui::SameLine();

        if (ImGui::Button("Remove Component"))
        {
            ImGui::OpenPopup("RemoveComponentPopup");
        }

        // Queued and applied after the popup closes (removing mid-draw would change what this pass reads).
        const IComponentEntry* lChosen = nullptr;

        if (ImGui::BeginPopup("RemoveComponentPopup"))
        {
            Uint64 lOffered = 0;

            lTypes.ForEach([&](const IComponentEntry& InEntry)
            {
                // Essential types are never offered (Remove would refuse them anyway).
                if (InEntry.IsEssential() || !InEntry.Has(lEntities, lHandle)) { return; }

                ++lOffered;
                if (ImGui::MenuItem(InEntry.GetName().CStr())) { lChosen = &InEntry; }
            });

            if (lOffered == 0) { ImGui::TextDisabled("Nothing to remove."); }

            ImGui::EndPopup();
        }

        if (lChosen != nullptr)
        {
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_REMOVE_COMPONENT, m_Context,
                                                    ComponentTypeParams{ lChosen->GetName() });
        }
    }

    void InspectorPanel::DrawAddComponent(Entity& InEntity)
    {
        const ComponentRegistry& lTypes    = m_Context.Engine.GetRegistries().Components();
        EntityRegistry&          lEntities = InEntity.GetWorld()->GetRegistry();
        const EntityID           lHandle   = InEntity.GetHandle();

        ImGui::Separator();

        if (ImGui::Button("Add Component"))
        {
            ImGui::OpenPopup("AddComponentPopup");
        }

        // Queued and applied after the popup closes (adding mid-draw would change what this pass reads).
        const IComponentEntry* lChosen = nullptr;

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            Uint64 lOffered = 0;

            lTypes.ForEach([&](const IComponentEntry& InEntry)
            {
                if (InEntry.Has(lEntities, lHandle)) { return; }

                ++lOffered;
                if (ImGui::MenuItem(InEntry.GetName().CStr())) { lChosen = &InEntry; }
            });

            // Different from "no match": every type is already on this entity.
            if (lOffered == 0) { ImGui::TextDisabled("Nothing left to add."); }

            ImGui::EndPopup();
        }

        if (lChosen != nullptr)
        {
            // By tag, like every edit: the dispatch records the undo step.
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_ADD_COMPONENT, m_Context,
                                                    ComponentTypeParams{ lChosen->GetName() });
        }
    }
}
