#include "Editor/Properties/NativeComponentDrawers.h"

#include <cstdio>   // snprintf — IEditorWidgets::TextDisabled takes finished text, not a format

#include "Editor/Properties/PropertyDrawers.h"   // DrawProperties, and the specializations it folds over
#include "Editor/UI/IEditorWidgets.h"

#include "Engine/Modules/ModuleRegistrar.h"      // DeriveTypeLeafName — the header's name, and the map key's

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
}
