#include "Editor/Properties/NativeComponentDrawers.h"

#include <cstdio>   // snprintf — IEditorWidgets::TextDisabled takes finished text, not a format

#include "Editor/Properties/PropertyDrawers.h"   // DrawProperties, and the specializations it folds over
#include "Editor/UI/IEditorWidgets.h"

// The camera's button is a DISPATCH, so it needs the registry the context carries — and the panel
// only for its id.
#include "Editor/EditorContext.h"
#include "Editor/Extensions/EditorExtensionRegistrar.h"
#include "Editor/Commands/EditorCommandRegistry.h"
#include "Editor/Commands/EditorNativeCommands.h"        // PanelIdParams
#include "Editor/Commands/EditorNativeCommandsTags.hpp"
#include "Editor/Panels/CameraPreviewPanel.h"

#include "Engine/Modules/ModuleRegistrar.h"      // DeriveTypeLeafName — the header's name, and the map key's

#include "World/Components/CameraComponent.h"
#include "World/Components/PrefabInstanceComponent.h"
#include "World/Components/SpriteAnimatorComponent.h"
#include "World/Components/SpriteComponent.h"
#include "World/Components/TextComponent.h"

namespace Opaax::Editor::NativeComponentDrawers
{
    namespace
    {
        /**
         * Which of two exclusive references is in effect, said out loud — and, when both are set,
         * which one is being ignored.
         *
         * The SECOND is the winner, matching how every one of these is documented: "set, it WINS".
         */
        void DrawActiveSource(IEditorWidgets& InWidgets,
                              const char* InLoserLabel,  const bool bInLoserSet,
                              const char* InWinnerLabel, const bool bInWinnerSet)
        {
            const char* lActive = bInWinnerSet ? InWinnerLabel
                                : bInLoserSet  ? InLoserLabel
                                               : "None";

            InWidgets.LabelText("Source", lActive);

            // ONLY when both are set. One field filled and the other empty is the ordinary case and
            // needs no sentence; a line that always shows is a line nobody reads.
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

        /** The header the generic drawer would have drawn, since the custom form does not. */
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

        // The component's own fields first, through the same fold the generic drawer uses — so a
        // field added to CameraComponent shows up here without this file being touched.
        DrawProperties(InWidgets, InCamera);

        // THE SAME VERB the Window menu invokes, reached by the tag rather than by touching
        // EditorPanels: the button and the menu entry cannot drift apart, and a key binding would
        // be a third front-end on the same one.
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
            // Visible rather than blank — **MP11**'s rule: a state the app can produce (rename the
            // `.opaaxprefab`, move it) has to be seen before anything can repair it.
            InWidgets.TextDisabled("Broken link - this entity came from a prefab that cannot be named");
            return;
        }

        // LabelText, never an input: read-only is the design, not a missing feature. See the header.
        InWidgets.LabelText("Prefab", InInstance.Prefab.Path.CStr());

        char lBuffer[64];

        std::snprintf(lBuffer, sizeof(lBuffer), "%s", InInstance.InstanceId.ToString().CStr());
        InWidgets.LabelText("Instance", lBuffer);

        std::snprintf(lBuffer, sizeof(lBuffer), "%s", InInstance.TemplateGuid.ToString().CStr());
        InWidgets.LabelText("Template", lBuffer);
    }
}
