#include "Editor/Imgui/Configs/EditorImguiConfigDrawer.h"

#include "Editor/Properties/PropertyDrawers.h"   // TPropertyDrawer<LinearColor>, which DrawField resolves
#include "Editor/UI/IEditorWidgets.h"

namespace Opaax::Editor
{
    void EditorImguiConfigDrawer::Draw(IEditorWidgets& InWidgets, EditorImguiConfigData& InData)
    {
        if (InWidgets.BeginTreeNode("Text"))
        {
            if (InWidgets.BeginTreeNode("Color"))
            {
                DrawField(InWidgets, "Text Normal", InData.TextColor);
                DrawField(InWidgets, "Text Disabled", InData.TextDisabledColor);
                
                InWidgets.EndTreeNode();
            }
            
            InWidgets.EndTreeNode();
        }
        
        InWidgets.Separator();

        if (InWidgets.BeginTreeNode("Window"))
        {
            if (InWidgets.BeginTreeNode("Framing & Spacing"))
            {
                InWidgets.DragFloat("Window Padding X", &InData.WindowPadding.x, 1, 0, 20);
                InWidgets.DragFloat("Window Padding Y", &InData.WindowPadding.y, 1, 0, 20);
            
                InWidgets.EndTreeNode();
            }
            
            InWidgets.Separator();
            
            if (InWidgets.BeginTreeNode("Color"))
            {
                DrawField(InWidgets, "Background", InData.WindowBackground);
            
                InWidgets.EndTreeNode();
            }
            

            InWidgets.EndTreeNode();
        }

        InWidgets.Separator();
        
        if (InWidgets.Button("Reset to Defaults", 0.f, "Restore every field to its built-in default."))
        {
            InData = EditorImguiConfigData{};
        }
    }
}
