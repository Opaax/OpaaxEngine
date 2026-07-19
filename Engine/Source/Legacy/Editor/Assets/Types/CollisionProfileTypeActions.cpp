#include "CollisionProfileTypeActions.h"

#if OPAAX_WITH_EDITOR

#include <imgui.h>

#include "Assets/AssetRegistry.h"
#include "Physics/Collision/CollisionProfile.h"

namespace Opaax::Editor
{
    void CollisionProfileTypeActions::Load(OpaaxStringID InID)
    {
        AssetRegistry::Load<CollisionProfile>(InID);
    }

    void CollisionProfileTypeActions::Reload(OpaaxStringID InID)
    {
        AssetRegistry::Reload<CollisionProfile>(InID);
    }

    void CollisionProfileTypeActions::DrawPreview(OpaaxStringID InID, IEditorUIBackend& InUIBackend)
    {
        const auto lHandle = AssetRegistry::Load<CollisionProfile>(InID);
        const CollisionProfile* lProfile = lHandle.Get();
        if (!lProfile)
        {
            ImGui::TextDisabled("Profile failed to load.");
            return;
        }

        // Read-only summary only — this runs inside a tooltip/drag source (non-interactive).
        const OpaaxString lChannel = ToStringID(lProfile->GetChannel()).ToString();
        ImGui::TextDisabled("Channel: %s", lChannel.CStr());
        for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
        {
            const ECollisionChannel lCh   = static_cast<ECollisionChannel>(i);
            const OpaaxString       lName = g_CollisionChannelIDs[i].ToString();
            ImGui::TextDisabled("  %-13s %s", lName.CStr(), ToString(lProfile->GetResponse(lCh)));
        }
    }

    void CollisionProfileTypeActions::DrawEditor(OpaaxStringID InID, IEditorUIBackend& InUIBackend)
    {
        const auto lHandle = AssetRegistry::Load<CollisionProfile>(InID);
        CollisionProfile* lProfile = lHandle.Get();
        if (!lProfile)
        {
            ImGui::TextDisabled("Profile failed to load.");
            return;
        }

        const OpaaxString lIDStr = InID.ToString();
        ImGui::TextDisabled("Collision Profile: %s", lIDStr.CStr());
        ImGui::Separator();

        // Object channel — what this profile's collider *is*.
        const OpaaxString lCurrentChannel = ToStringID(lProfile->GetChannel()).ToString();
        if (ImGui::BeginCombo("Object Channel", lCurrentChannel.CStr()))
        {
            for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
            {
                const bool        bSelected = (i == static_cast<Uint8>(lProfile->GetChannel()));
                const OpaaxString lName     = g_CollisionChannelIDs[i].ToString();
                if (ImGui::Selectable(lName.CStr(), bSelected))
                {
                    lProfile->SetChannel(static_cast<ECollisionChannel>(i));
                }
                if (bSelected) { ImGui::SetItemDefaultFocus(); }
            }
            ImGui::EndCombo();
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Response to each channel:");

        // Per-channel response matrix (one row per channel, three-state combo).
        static const char* lResponseNames[] = { "Ignore", "Overlap", "Block" };
        if (ImGui::BeginTable("CollisionResponses", 2,
                              ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp))
        {
            for (Uint8 i = 0; i < static_cast<Uint8>(ECollisionChannel::Count); ++i)
            {
                const ECollisionChannel lChannel = static_cast<ECollisionChannel>(i);
                const OpaaxString       lName    = g_CollisionChannelIDs[i].ToString();

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(lName.CStr());

                ImGui::TableSetColumnIndex(1);
                int lResponse = static_cast<int>(lProfile->GetResponse(lChannel));
                ImGui::PushID(i);
                ImGui::SetNextItemWidth(-1.f);
                if (ImGui::Combo("##resp", &lResponse, lResponseNames, IM_ARRAYSIZE(lResponseNames)))
                {
                    lProfile->SetResponse(lChannel, static_cast<ECollisionResponse>(lResponse));
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        ImGui::Separator();
        if (ImGui::Button("Save Profile"))
        {
            lProfile->Save();
        }
    }

} // namespace Opaax::Editor

#endif // OPAAX_WITH_EDITOR
