#pragma once

#include <nlohmann/json.hpp>

#include "Core/Log/Logger.h"
#include "Core/Maths/MathTypes.h"   // Vector2F
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Editor/Assets/AssetDocument.hpp"
#include "Editor/Assets/IAssetEditor.h"
#include "Editor/Undo/EditorUndo.h"

namespace Opaax
{
    class ObjectClass;
}

namespace Opaax::Editor
{
    struct EditorContext;
    class IEditorGui;

    OPAAX_LOG_CATEGORY(AssetEditors);

    // =============================================================================
    // AssetEditors — the open assets, each in a dockable window of its own: a header (name, class,
    //   dirty mark, Save), then its class's editor (the property list unless the class registered
    //   another). Each asset has its own undo history; Ctrl+S, Ctrl+Z and Ctrl+Y act on the focused
    //   one. Owned by EditorService, reached through EditorContext; menus, the Resource Browser and
    //   automation all go through the same calls.
    // =============================================================================
    class AssetEditors
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        AssetEditors();
        ~AssetEditors();

        AssetEditors(const AssetEditors&)            = delete;
        AssetEditors& operator=(const AssetEditors&) = delete;

        /** Gives the context the editors are built from. Called once it exists. */
        void Bind(EditorContext& InContext) noexcept { m_Context = &InContext; }

        // =============================================================================
        // Documents
        // =============================================================================
    public:
        /**
         * Opens InAbsPath in a window, or brings forward the window that has it.
         * @param OutError Why it did not open (optional)
         */
        bool Open(const OpaaxString& InAbsPath, OpaaxString* OutError = nullptr);

        /**
         * Writes a new asset of InClass at InAbsPath, every field at its default, and opens it. Refuses a
         * file that exists or lies outside the project's assets.
         */
        bool Create(const OpaaxString& InAbsPath, const ObjectClass& InClass, OpaaxString* OutError = nullptr);

        /**
         * Closes InAbsPath's window. Unsaved changes are kept unless the user agrees to lose them, or
         * bInDiscard is true (a script has nobody to ask).
         * @return True when it is closed (or was not open)
         */
        bool Close(const OpaaxString& InAbsPath, bool bInDiscard = false);

        /** Closes every window without asking (the editor is shutting down). */
        void CloseAll();

        /** Writes InAbsPath and reloads it, so a running game reads the new values. */
        bool Save(const OpaaxString& InAbsPath);

        /** Saves every open asset with unsaved changes. @return How many were written */
        Uint32 SaveAll();

        /** Steps InAbsPath's own history. */
        void Undo(const OpaaxString& InAbsPath);
        void Redo(const OpaaxString& InAbsPath);

        /**
         * Replaces InAbsPath's values as one undo step (a script's edit), then runs its PostEditChange.
         * @return False when it is not open, or a value has the wrong type (nothing changed)
         */
        bool Edit(const OpaaxString& InAbsPath, const nlohmann::json& InProperties, OpaaxString* OutError = nullptr);

        /** Applies values from InAbsPath's history (undo and redo steps). */
        void ApplyFromHistory(const OpaaxString& InAbsPath, const nlohmann::json& InProperties);

        // =============================================================================
        // Get
        // =============================================================================
    public:
        /** The open asset at InAbsPath, or null. */
        AssetDocument* Find(const OpaaxString& InAbsPath) const;

        /** InAbsPath's history, or null when it is not open. */
        const EditorUndo* FindHistory(const OpaaxString& InAbsPath) const;

        /** The asset whose window had the keyboard focus during the last Draw, or null. */
        AssetDocument* GetFocused() const;

        /** Every open asset, in the order they were opened. */
        TDynArray<AssetDocument*> GetOpen() const;

        bool HasUnsaved() const;

        // =============================================================================
        // Drawing
        // =============================================================================
    public:
        /** Every open asset's window. Part of the UI pass, after the panels. */
        void Draw(IEditorGui& InGui);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        struct OpenAsset
        {
            TUniquePtr<AssetDocument> Document;
            TUniquePtr<IAssetEditor>  Editor;
            EditorUndo                History;

            /** The values as of the last recorded change: what a new change is compared against. */
            nlohmann::json            Recorded;

            /** The window's corner and dock node (0: floating) as last drawn: a new window opens from them. */
            Vector2F                  WindowPos;
            Uint32                    DockId = 0;

            bool                      bPlaced    = false;
            bool                      bFocusNext = true;
            bool                      bWantClose = false;
        };

        OpenAsset* FindOpen(const OpaaxString& InAbsPath) const;

        /** Adds InDocument's window, with its class's editor. */
        void AddWindow(TUniquePtr<AssetDocument> InDocument);

        /** Records what changed in InOpen since the last recorded step, once no widget is active. */
        void RecordChanges(OpenAsset& InOpen);

        /**
         * Places InNew's window before its first frame: a tab of the newest docked asset window, else below
         * and right of the newest floating one, else centred. A window with saved settings keeps them.
         */
        void PlaceNewWindow(const OpenAsset& InNew, Vector2F InSize) const;

        void DrawWindow(IEditorGui& InGui, OpenAsset& InOpen);
        void DrawHeader(OpenAsset& InOpen);

        TDynArray<TUniquePtr<OpenAsset>> m_Open;
        OpaaxString                      m_FocusedPath;
        EditorContext*                   m_Context = nullptr;
    };
}
