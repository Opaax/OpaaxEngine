#include "Editor/Panels/InspectorPanel.h"

#include <cstring>   // memcpy — the name field's edit buffer

#include "Editor/EditorContext.h"
#include "Editor/Commands/EditorNativeCommands.h"       // the params the Inspector's verbs carry (⑤)
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Undo/EditorUndo.h"                     // the step a field edit records (⑤)
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
        // A local COPY of the handle: Entity is a value type, and copying it once keeps the whole draw
        // reading one consistent selection even if a drawer were to change it.
        Entity lSelected = m_Context.Selection.Get();
        if (!lSelected.IsValid())
        {
            ImGui::TextDisabled("Nothing selected.");
            return;
        }

        DrawNameField(lSelected);

        // SAY which one is being edited when there are several. This panel draws the PRIMARY only —
        // multi-edit is its own slice, because a TPropertyDrawer sees one T& and not N — and an
        // unexplained "I selected three and one appeared" reads as a bug rather than as a boundary.
        // Naming it costs a line and is the difference (L15, applied to UI).
        if (const Uint64 lCount = m_Context.Selection.Count(); lCount > 1)
        {
            ImGui::TextDisabled("%llu selected - editing the last picked",
                                static_cast<unsigned long long>(lCount));
        }

        ImGui::Separator();

        // Ask every registered drawer whether it applies, rather than asking the entity what it has —
        // the inversion that keeps this panel ignorant of every component type (DrawerRegistry).
        // Every applicable drawer, not just the first — an entity carries several components.
        bool lAnyDrawn = false;
        for (const TFunction<bool(Entity&, IEditorWidgets&)>& lEntry : m_Context.Extensions.Drawers().Entries())
        {
            if (lEntry && lEntry(lSelected, m_Context.Widgets))
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

        // THE ONE MUTATION THE WORLD CANNOT SEE. A drawer receives a raw TComponent& (DrawerRegistry)
        // and writes straight through it, so no World method and no entt signal observes a field
        // edit — and the dirty check is now gated on World::GetRevision().
        //
        // Asked of IMGUI rather than of the drawer: a `bool Draw()` contract would let one forgetful
        // drawer report CLEAN WHILE DIRTY, which fails silently and permanently. This over-reports
        // instead (any active widget anywhere costs one extra capture) and can never under-report.
        //
        // The trailing frame matters: a Checkbox commits on RELEASE, and ImGui has already cleared
        // ActiveId by the time this line runs on that frame.
        const bool lItemActive = ImGui::IsAnyItemActive();

        if (lItemActive || m_bWasItemActive)
        {
            if (World* lWorld = lSelected.GetWorld()) { lWorld->MarkChanged(); }
        }

        // ⑤ — THE EDIT GESTURE'S TWO EDGES, and the whole of the property step.
        //
        // The RISING edge is read AFTER the drawers ran, and that is correct rather than lucky:
        // ImGui zeroes the drag accumulator on the frame an item is activated and trickles the
        // click and the first move into different frames, so a Drag* has not written yet; InputText
        // has only taken focus; a Checkbox commits on release. So the values captured here are the
        // pre-edit ones.
        //
        // GLOBAL is the point, not a compromise: a resource dragged from the Browser holds ActiveId
        // over there, so the bracket opens before the drop and closes on it.
        if (lItemActive && !m_bWasItemActive)
        {
            m_Edit.Begin(m_Context, lSelected);
        }
        else if (!lItemActive && m_bWasItemActive)
        {
            if (m_Edit.End(m_Context)) { m_Context.Undo.Record(Move(m_Edit)); }

            // Closed either way — a step left holding entries would fold the next edit into it.
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

        // Refresh from the entity only while the field is NOT being typed into — otherwise every
        // frame would overwrite the keystrokes with the committed value.
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

        // Committed on Enter OR on losing focus, so clicking away keeps what was typed rather than
        // silently discarding it — the one behaviour a name field must not get wrong.
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

        // QUEUED and applied after the popup closes, exactly as Add is: removing mid-draw mutates
        // the registry this pass is reading.
        const IComponentEntry* lChosen = nullptr;

        if (ImGui::BeginPopup("RemoveComponentPopup"))
        {
            Uint64 lOffered = 0;

            lTypes.ForEach([&](const IComponentEntry& InEntry)
            {
                // Essential types are never offered — the transform is what picking, the icons and
                // both render joins stand on. Remove refuses one anyway; this keeps the menu honest.
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

        // The verb is QUEUED and applied after the popup closes: emplacing mid-draw would mutate the
        // registry this very pass is reading — the shape the Hierarchy's context menu already needed.
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

            // Distinct from "no entry matched": every type is already on this entity.
            if (lOffered == 0) { ImGui::TextDisabled("Nothing left to add."); }

            ImGui::EndPopup();
        }

        if (lChosen != nullptr)
        {
            // BY TAG, like every other edit: the verb (and its MarkChanged and its log) moved into
            // EntityOps, and the dispatch is what records the step. These two popups were the last
            // place a panel reached entt directly (**SEL6**).
            m_Context.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_ADD_COMPONENT, m_Context,
                                                    ComponentTypeParams{ lChosen->GetName() });
        }
    }
}
