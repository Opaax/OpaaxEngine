#include "EditorMenu.h"

#include <imgui.h>

namespace Opaax::Editor
{
    EditorMenu::EditorMenu()
    {
        const OpaaxStringID lMyId = ("test");
        TSharedPtr<EditorMenuCategory> lMyCategory{new EditorMenuCategory(lMyId)};
        RegisterMenuCategory(lMyCategory);
    }

    bool EditorMenu::IsCategoryAlreadyRegistered(const OpaaxStringID& InID) const
    {
        for (const auto& lCategory : MenuCategories)
        {
            if (lCategory->GetID() == InID)
            {
                return true;
            }
        }
        
        return false;
    }

    void EditorMenu::RegisterCategoryInternal(TSharedPtr<EditorMenuCategory> InID)
    {
        //Todo: Log 
        MenuCategories.emplace_back(InID);
    }

    void EditorMenu::RegisterMenuCategory(TSharedPtr<EditorMenuCategory> InID)
    {   
        //empty No check
        if (!HasMenuCategories())
        {
            RegisterCategoryInternal(InID);
        }
        //Check already registered
        else
        {
            if (IsCategoryAlreadyRegistered(InID->GetID()))
            {
                //Todo: Log
                return;
            }
            
            RegisterCategoryInternal(InID);
        }
    }

    bool EditorMenu::DrawEditorMenu()
    {
        if (ImGui::BeginMainMenuBar())
        {
            for (const auto& lCategory : MenuCategories)
            {
                const char* lCatAsString = lCategory->GetID().CStr();
                if (ImGui::BeginMenu(lCatAsString))
                {
                    //DrawMenuLevel(lChildren, InDepth + 1);
                    ImGui::EndMenu();
                }
            }
        }
        ImGui::EndMainMenuBar();
        
        return false;
    }
}
