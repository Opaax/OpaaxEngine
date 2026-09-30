#pragma once

namespace Opaax
{
    struct SpriteComponent;
    struct SpriteAnimatorComponent;
    struct TextComponent;
    struct CameraComponent;
    struct PrefabInstanceComponent;
    class  Entity;
}

namespace Opaax::Editor
{
    class IEditorWidgets;
    struct EditorContext;

    // =============================================================================
    // The editor's drawers for the components that can reference a resource two ways (sprite:
    //   Texture or Sheet; animator: Library or ClipAsset; text: Family or Face). One reference wins
    //   when set; these drawers say which one is in effect. The choice is derived from the data (the
    //   same rule the renderer uses), not stored.
    //   Each draws its own CollapsingHeader. Uses IEditorWidgets only.
    // =============================================================================
    namespace NativeComponentDrawers
    {
        /** Texture vs Sheet: the Sheet wins, and the Frame belongs to it. */
        struct SpriteComponentDrawer
        {
            void Draw(IEditorWidgets& InWidgets, SpriteComponent& InSprite);
        };

        /** Library + Clip name vs ClipAsset: the Library wins. */
        struct SpriteAnimatorComponentDrawer
        {
            void Draw(IEditorWidgets& InWidgets, SpriteAnimatorComponent& InAnimator);
        };

        /** Font family vs a bare Face: the family wins, and the Style belongs to it. */
        struct TextComponentDrawer
        {
            void Draw(IEditorWidgets& InWidgets, TextComponent& InText);
        };

        /**
         * The camera's own fields, then a button that opens the camera preview. Takes the context only to
         * dispatch that command (the Window menu's). The subject is unused.
         */
        struct CameraComponentDrawer
        {
            void Draw(IEditorWidgets& InWidgets, CameraComponent& InCamera, Entity& InEntity, EditorContext& InContext);
        };

        /**
         * Which prefab this entity came from. Read-only: the component is identity, and editing a guid
         * would silently break the link. A broken link (renamed or moved prefab) is shown as such.
         */
        struct PrefabInstanceComponentDrawer
        {
            void Draw(IEditorWidgets& InWidgets, PrefabInstanceComponent& InInstance);
        };
    }
}
