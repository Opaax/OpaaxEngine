#include "MoverDrawer.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#include "ECS/Components/MoverComponent.h"
#include "Core/Systems/Movement/MoverModeRegistry.h"

namespace Opaax::Editor
{
    bool MoverDrawer::HasComponent(World& InWorld, EntityID InEntity) const
    {
        return InWorld.GetComponent<ECS::MoverComponent>(InEntity) != nullptr;
    }

    void MoverDrawer::Draw(World& InWorld, EntityID InEntity)
    {
        auto* lMover = InWorld.GetComponent<ECS::MoverComponent>(InEntity);
        if (!lMover) { return; }

        const bool bOpen = ImGui::CollapsingHeader("Mover", ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetStyle().ItemSpacing.x);
        if (ImGui::SmallButton("X##RemoveMover"))
        {
            InWorld.RemoveComponent<ECS::MoverComponent>(InEntity);
            return;
        }

        if (!bOpen) { return; }

        // --- Collision proxy ---
        static const char* lShapeNames[] = { "Capsule", "Circle" };
        int lShape = static_cast<int>(lMover->Shape);
        if (ImGui::Combo("Shape", &lShape, lShapeNames, IM_ARRAYSIZE(lShapeNames)))
        {
            lMover->Shape = static_cast<EMoverShape>(lShape);
        }

        if (lMover->Shape == EMoverShape::Capsule)
        {
            ImGui::DragFloat("Height", &lMover->Height, 1.f, 0.f, 0.f, "%.0f");
        }
        ImGui::DragFloat("Radius",        &lMover->Radius,           1.f, 0.f, 0.f, "%.0f");
        ImGui::DragFloat("Max Slope (deg)", &lMover->MaxSlopeAngleDeg, 1.f, 0.f, 89.f, "%.0f");

        // --- Supported modes: the authored subset this mover may use (add from the global catalog) ---
        ImGui::SeparatorText("Modes");
        {
            // List supported modes, each with a remove button — but keep at least one.
            OpaaxStringID lToRemove;   // deferred so we don't mutate the list mid-iteration
            for (const OpaaxStringID& lId : lMover->Modes)
            {
                ImGui::PushID(static_cast<int>(lId.GetId()));
                ImGui::BulletText("%s", lId.ToString().CStr());
                if (lMover->Modes.size() > 1)
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) { lToRemove = lId; }
                }
                ImGui::PopID();
            }
            if (lToRemove.IsValid()) { lMover->RemoveMode(lToRemove); }

            // Add-mode picker — registry modes this mover doesn't support yet.
            TDynArray<OpaaxStringID> lAddable;
            for (const OpaaxStringID& lId : MoverModeRegistry::GetModeIds())
            {
                if (!lMover->SupportsMode(lId)) { lAddable.push_back(lId); }
            }
            if (!lAddable.empty())
            {
                TDynArray<OpaaxString> lNames;   // own strings (reserve so .CStr() stays valid)
                lNames.reserve(lAddable.size());
                for (const OpaaxStringID& lId : lAddable) { lNames.push_back(lId.ToString()); }
                TDynArray<const char*> lLabels;
                lLabels.reserve(lNames.size());
                for (const OpaaxString& lName : lNames) { lLabels.push_back(lName.CStr()); }

                int lPick = -1;
                if (ImGui::Combo("Add Mode", &lPick, lLabels.data(), static_cast<int>(lLabels.size()))
                    && lPick >= 0 && lPick < static_cast<int>(lAddable.size()))
                {
                    lMover->AddMode(lAddable[lPick]);
                }
            }
        }

        // --- Active mode: only among the supported modes (authoring; live switch = QueueNextMode) ---
        {
            TDynArray<OpaaxString> lNames;
            lNames.reserve(lMover->Modes.size());
            for (const OpaaxStringID& lId : lMover->Modes) { lNames.push_back(lId.ToString()); }
            TDynArray<const char*> lLabels;
            lLabels.reserve(lNames.size());
            int lCurrent = -1;
            for (size_t i = 0; i < lNames.size(); ++i)
            {
                lLabels.push_back(lNames[i].CStr());
                if (lMover->Modes[i] == lMover->ModeId) { lCurrent = static_cast<int>(i); }
            }

            if (!lLabels.empty()
                && ImGui::Combo("Active Mode", &lCurrent, lLabels.data(), static_cast<int>(lLabels.size()))
                && lCurrent >= 0 && lCurrent < static_cast<int>(lMover->Modes.size()))
            {
                lMover->ModeId = lMover->Modes[lCurrent];
            }
        }

        ImGui::SeparatorText("Movement Params");
        ImGui::DragFloat("Gravity",      &lMover->Params.Gravity,      1.f,   0.f, 0.f, "%.0f");
        ImGui::DragFloat("Max Speed",    &lMover->Params.MaxSpeed,     1.f,   0.f, 0.f, "%.0f");
        ImGui::DragFloat("Acceleration", &lMover->Params.Acceleration, 0.1f,  0.f, 0.f, "%.1f");
        ImGui::DragFloat("Friction",     &lMover->Params.Friction,     0.1f,  0.f, 0.f, "%.1f");
        ImGui::DragFloat("Stop Speed",   &lMover->Params.StopSpeed,    1.f,   0.f, 0.f, "%.0f");
        ImGui::DragFloat("Min Speed",    &lMover->Params.MinSpeed,     1.f,   0.f, 0.f, "%.0f");
        ImGui::DragFloat("Air Steer",    &lMover->Params.AirSteer,     0.01f, 0.f, 1.f, "%.2f");
        ImGui::DragFloat("Jump Speed",   &lMover->Params.JumpSpeed,    1.f,   0.f, 0.f, "%.0f");
    }

    void MoverDrawer::Add(World& InWorld, EntityID InEntity)
    {
        InWorld.AddComponent<ECS::MoverComponent>(InEntity);
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
