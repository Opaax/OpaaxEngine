#pragma once

namespace Opaax
{
    struct SpriteComponent;
    struct SpriteAnimatorComponent;
    struct TextComponent;
}

namespace Opaax::Editor
{
    class IEditorWidgets;

    // =============================================================================
    // The editor's drawers for the three components that name a resource TWO WAYS.
    //
    //   `Docs/TODO.txt` asks for this on the sprite ("Texture Type: Texture, Sheet") and on the
    //   animator ("Anim type: Clip, AnimAsset"); ⑥ S4's TextComponent made it three (Font, Face).
    //   All three share one rule — one reference WINS when it is set — and the generic drawer showed
    //   both fields with no hint which was in effect, so a sprite with a Sheet looked as though its
    //   Texture mattered.
    //
    //   THE SOURCE IS DERIVED, NEVER STORED, and that is a deliberate limit rather than a shortcut.
    //   A stored "which type" enum is what Unity's Draw Mode is, and it would be the better model —
    //   but a component that gained one would need a DEFAULT, and no static default is right: a map
    //   already carrying a Sheet with the field defaulting to Texture would silently stop drawing it
    //   at load. That is a migration, not a field. So the drawer reads the same rule the renderer
    //   reads (**TX1**) and says what it finds, which cannot disagree with the frame.
    //
    //   Each drawer draws its own CollapsingHeader: the registry's custom form deliberately does not
    //   frame the drawer, unlike the generic one.
    //
    //   These are CONTENTS, so they could call ImGui — but they do not need to. Everything here is
    //   IEditorWidgets, which keeps them on the portable side of **MR2h** for free.
    // =============================================================================
    namespace NativeComponentDrawers
    {
        /** Texture vs Sheet — the Sheet wins, and the Frame belongs to it. */
        struct SpriteComponentDrawer
        {
            void Draw(IEditorWidgets& InWidgets, SpriteComponent& InSprite);
        };

        /** Library + Clip name vs ClipAsset — the Library wins. */
        struct SpriteAnimatorComponentDrawer
        {
            void Draw(IEditorWidgets& InWidgets, SpriteAnimatorComponent& InAnimator);
        };

        /** Font family vs a bare Face — the family wins, and the Style belongs to it. */
        struct TextComponentDrawer
        {
            void Draw(IEditorWidgets& InWidgets, TextComponent& InText);
        };
    }
}
