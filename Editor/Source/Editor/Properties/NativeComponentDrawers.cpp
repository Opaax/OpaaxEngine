#include "Editor/Properties/NativeComponentDrawers.h"

#include <cstdio>   // snprintf

#include "Editor/Properties/PropertyDrawers.h"   // DrawProperties
#include "Editor/UI/IEditorWidgets.h"

// The camera's button dispatches a command, so it needs the context's registry.
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommands.h"        // PanelIdParams
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Panels/CameraPreviewPanel.h"

#include "Engine/Registries/ModuleRegistrar.h"      // DeriveTypeLeafName

#include "Renderer/Camera/CameraComponent.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "Animation/SpriteAnimatorComponent.h"
#include "Renderer/Components/SpriteComponent.h"
#include "Renderer/Components/TextComponent.h"

namespace Opaax::Editor::NativeComponentDrawers
{
    namespace
    {
        /**
         * Says which of two exclusive references is in effect, and which one is ignored when both are set.
         * The second one wins.
         */
        void DrawActiveSource(IEditorWidgets& InWidgets,
                              const char* InLoserLabel,  const bool bInLoserSet,
                              const char* InWinnerLabel, const bool bInWinnerSet)
        {
            const char* lActive = bInWinnerSet ? InWinnerLabel
                                : bInLoserSet  ? InLoserLabel
                                               : "None";

            InWidgets.LabelText("Source", lActive);

            // Only when both are set (one filled is the normal case).
            if (bInWinnerSet && bInLoserSet)
            {
                char lNote[160];
                std::snprintf(lNote, sizeof(lNote), "%s is ignored while %s is set. Clear %s to use it.",
                              InLoserLabel, InWinnerLabel, InWinnerLabel);

                InWidgets.TextDisabled(lNote);
            }
            else if (!bInWinnerSet && !bInLoserSet)
            {
                char lNote[160];
                std::snprintf(lNote, sizeof(lNote), "Nothing to draw — set %s or %s.",
                              InWinnerLabel, InLoserLabel);

                InWidgets.TextDisabled(lNote);
            }
        }

        /** The header the generic drawer would have drawn. */
        template<typename TComponent>
        bool BeginComponent(IEditorWidgets& InWidgets)
        {
            return InWidgets.CollapsingHeader(DeriveTypeLeafName<TComponent>().CStr());
        }
    }

    void SpriteComponentDrawer::Draw(IEditorWidgets& InWidgets, SpriteComponent& InSprite)
    {
        if (!BeginComponent<SpriteComponent>(InWidgets)) { return; }

        DrawActiveSource(InWidgets, "Texture", !InSprite.Texture.IsEmpty(),
                                    "Sheet",   !InSprite.Sheet.IsEmpty());

        DrawProperties(InWidgets, InSprite);
    }

    void SpriteAnimatorComponentDrawer::Draw(IEditorWidgets& InWidgets, SpriteAnimatorComponent& InAnimator)
    {
        if (!BeginComponent<SpriteAnimatorComponent>(InWidgets)) { return; }

        DrawActiveSource(InWidgets, "Clip Asset", !InAnimator.ClipAsset.IsEmpty(),
                                    "Library",    !InAnimator.Library.IsEmpty());

        DrawProperties(InWidgets, InAnimator);
    }

    void TextComponentDrawer::Draw(IEditorWidgets& InWidgets, TextComponent& InText)
    {
        if (!BeginComponent<TextComponent>(InWidgets)) { return; }

        DrawActiveSource(InWidgets, "Face", !InText.Face.IsEmpty(),
                                    "Font", !InText.Font.IsEmpty());

        DrawProperties(InWidgets, InText);
    }

    void CameraComponentDrawer::Draw(IEditorWidgets& InWidgets, CameraComponent& InCamera,
                                     Entity& /*InEntity*/, EditorContext& InContext)
    {
        if (!BeginComponent<CameraComponent>(InWidgets)) { return; }

        // The component's own fields first, through the generic fold (new fields show up automatically).
        DrawProperties(InWidgets, InCamera);

        // The same command as the Window menu, dispatched by tag.
        if (InWidgets.Button("Open Preview", 0.f, "Show what this camera frames, in its own panel"))
        {
            InContext.Extensions.Commands().Execute(Tags::EDITOR_COMMAND_TOGGLE_PANEL, InContext,
                                                    PanelIdParams{ CameraPreviewPanel::PanelID() });
        }
    }

    void PrefabInstanceComponentDrawer::Draw(IEditorWidgets& InWidgets,
                                             PrefabInstanceComponent& InInstance)
    {
        if (!BeginComponent<PrefabInstanceComponent>(InWidgets)) { return; }

        if (!InInstance.IsLinked())
        {
            // Shown rather than blank: a renamed or moved prefab must be visible to be fixed.
            InWidgets.TextDisabled("Broken link - this entity came from a prefab that cannot be named");
            return;
        }

        // Read-only by design (see the header).
        InWidgets.LabelText("Prefab", InInstance.Prefab.Path.CStr());

        char lBuffer[64];

        std::snprintf(lBuffer, sizeof(lBuffer), "%s", InInstance.InstanceId.ToString().CStr());
        InWidgets.LabelText("Instance", lBuffer);

        std::snprintf(lBuffer, sizeof(lBuffer), "%s", InInstance.TemplateGuid.ToString().CStr());
        InWidgets.LabelText("Template", lBuffer);
    }
}
