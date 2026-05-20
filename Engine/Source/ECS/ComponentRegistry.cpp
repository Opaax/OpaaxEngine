#include "ComponentRegistry.h"

#if OPAAX_WITH_EDITOR
#include <imgui.h>
#endif

namespace Opaax
{
    const IComponentEntry* ComponentRegistry::FindByName(OpaaxStringID InName)
    {
        for (const auto& lEntry : GetAll())
        {
            if (lEntry->GetName() == InName) { return lEntry.get(); }
        }
        return nullptr;
    }

    const IComponentEntry* ComponentRegistry::FindByTypeId(entt::id_type InTypeId)
    {
        for (const auto& lEntry : GetAll())
        {
            if (lEntry->GetTypeId() == InTypeId) { return lEntry.get(); }
        }
        return nullptr;
    }

    IComponentEntry* ComponentRegistry::FindByTypeIdMutable(entt::id_type InTypeId)
    {
        for (const auto& lEntry : GetAll())
        {
            if (lEntry->GetTypeId() == InTypeId) { return lEntry.get(); }
        }
        return nullptr;
    }

#if OPAAX_WITH_EDITOR
    // ---------------------------------------------------------------------------
    // Default drawer — internal helper used when an entry has no CustomDrawer.
    // Walks the component's Serialize() json read-only.
    // ---------------------------------------------------------------------------
    namespace
    {
        void RenderJsonRecursive(const char* InLabel, const json& InValue)
        {
            if (InValue.is_object())
            {
                if (ImGui::TreeNodeEx(InLabel, ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (auto it = InValue.begin(); it != InValue.end(); ++it)
                    {
                        RenderJsonRecursive(it.key().c_str(), it.value());
                    }
                    ImGui::TreePop();
                }
            }
            else if (InValue.is_array())
            {
                if (ImGui::TreeNodeEx(InLabel, ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (size_t i = 0; i < InValue.size(); ++i)
                    {
                        char lIdx[16];
                        std::snprintf(lIdx, sizeof(lIdx), "[%zu]", i);
                        RenderJsonRecursive(lIdx, InValue[i]);
                    }
                    ImGui::TreePop();
                }
            }
            else if (InValue.is_number_float())
            {
                ImGui::Text("%s: %.4f", InLabel, InValue.get<double>());
            }
            else if (InValue.is_number_integer())
            {
                ImGui::Text("%s: %lld", InLabel, static_cast<long long>(InValue.get<int64_t>()));
            }
            else if (InValue.is_boolean())
            {
                ImGui::Text("%s: %s", InLabel, InValue.get<bool>() ? "true" : "false");
            }
            else if (InValue.is_string())
            {
                ImGui::Text("%s: %s", InLabel, InValue.get<std::string>().c_str());
            }
            else if (InValue.is_null())
            {
                ImGui::TextDisabled("%s: null", InLabel);
            }
        }

        void DrawDefault(const IComponentEntry& InEntry, World& InWorld, EntityID InEntity)
        {
            // Bind ToString()'s temporary to a named local so .CStr() points into
            // live storage across the ImGui call (Lesson 11).
            const OpaaxString lName = InEntry.GetName().ToString();
            if (!ImGui::CollapsingHeader(lName.CStr(), ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            const json lJson = InEntry.Save(InWorld, InEntity);
            if (lJson.empty())
            {
                ImGui::TextDisabled("(no serialized fields)");
                return;
            }

            for (auto it = lJson.begin(); it != lJson.end(); ++it)
            {
                RenderJsonRecursive(it.key().c_str(), it.value());
            }
        }
    }

    void ComponentRegistry::DrawAll(World& InWorld, EntityID InEntity)
    {
        for (const auto& lEntry : GetAll())
        {
            if (!lEntry->Has(InWorld, InEntity)) { continue; }

            if (Editor::IComponentDrawer* lDrawer = lEntry->GetCustomDrawer())
            {
                lDrawer->Draw(InWorld, InEntity);
            }
            else
            {
                DrawDefault(*lEntry, InWorld, InEntity);
            }
        }
    }

    void ComponentRegistry::DrawAddComponentMenu(World& InWorld, EntityID InEntity)
    {
        for (const auto& lEntry : GetAll())
        {
            if (!lEntry->ShouldShowInAddMenu()) { continue; }
            if (lEntry->Has(InWorld, InEntity)) { continue; }

            // Bind ToString()'s temporary to a named local (Lesson 11).
            const OpaaxString lName = lEntry->GetName().ToString();
            if (ImGui::MenuItem(lName.CStr()))
            {
                lEntry->Add(InWorld, InEntity);
                ImGui::CloseCurrentPopup();
            }
        }
    }
#endif // OPAAX_WITH_EDITOR
}
