#include "Editor/Panels/InspectorPanel.h"

#include <cstring>   // memcpy — the name field's edit buffer

#include "Editor/EditorContext.h"
#include "Editor/Operation/EditorSelection.hpp"
#include "Editor/Operation/EntityOps.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"

#include "Application/Services/IEngine.h"
#include "Application/Services/ILogger.h"
#include "Engine/Registries/EngineRegistries.h"
#include "World/Components/ComponentRegistry.h"
#include "World/Entity/Entity.h"
#include "World/Entity/EntityMeta.h"
#include "World/World.h"

#include <imgui.h>

using namespace Opaax;

namespace
{
    constexpr LogCategory LogInspector{"InspectorPanel"};
}

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
        for (const TFunction<bool(Entity&, IEditorGui&)>& lEntry : m_Context.Extensions.Drawers().Entries())
        {
            if (lEntry && lEntry(lSelected, m_Context.Gui))
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
            EntityOps::Rename(m_Context, InEntity, OpaaxString(m_NameBuffer));
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
            const OpaaxString lName = InEntity.Get<EntityMeta>().Name;

            if (lChosen->Remove(lEntities, lHandle))
            {
                if (World* lWorld = InEntity.GetWorld()) { lWorld->MarkChanged(); }

                OPAAX_LOG(LogInspector, Info, "Removed component '{}' from entity '{}'",
                          lChosen->GetName().CStr(), lName.CStr());
            }
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
            lChosen->Add(lEntities, lHandle);

            // Explicit rather than left to the ImGui check in Draw: emplacing a component IS a
            // content change, and it should not depend on a popup item still counting as active.
            if (World* lWorld = InEntity.GetWorld()) { lWorld->MarkChanged(); }

            OPAAX_LOG(LogInspector, Info, "Added component '{}' to entity '{}'",
                lChosen->GetName().CStr(), InEntity.Get<EntityMeta>().Name.CStr());
        }
    }
}
