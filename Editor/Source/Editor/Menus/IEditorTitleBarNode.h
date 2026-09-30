#pragma once

#include "Core/Log/Logger.h"   // OPAAX_LOG_CATEGORY
#include "Core/OpaaxTypes.h"                // TFunction, Move
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"    // interned identity

namespace Opaax::Editor
{
    OPAAX_LOG_CATEGORY(EditorMenu);

    struct EditorContext;
    class EditorTitleBarCategory;
    class EditorTitleBarCommandNode;

    /**
     * A condition checked at draw time (enabled, checked). Empty means always. Evaluated every frame.
     */
    using FMenuPredicate = TFunction<bool(const EditorContext&)>;

    /**
     * A label computed at draw time, for an entry whose text depends on state ("Undo Move"). Only
     * built while its menu is open.
     */
    using FMenuLabel = TFunction<OpaaxString(const EditorContext&)>;

    /**
     * @class IEditorTitleBarNode
     *
     * One node of the menu bar: a category, a command or a separator.
     * Its identity is an OpaaxStringID, which is also its label. A command node may display a
     * different text through FMenuLabel; lookups and logs still use the id.
     * The full path ("File/Save Map") is built once at construction, for the log.
     */
    class IEditorTitleBarNode
    {
        // =============================================================================
        // Ctor - Dtor
        // =============================================================================
    public:
        IEditorTitleBarNode(const OpaaxStringID InID, const OpaaxString& InParentPath)
            : m_ID(InID)
            , m_Path(InParentPath.IsEmpty() ? OpaaxString(InID.CStr()) : InParentPath + "/" + InID.CStr())
        {
        }

        virtual ~IEditorTitleBarNode() = default;

        // =============================================================================
        // Copy - Move Delete
        // =============================================================================

        // Owned by exactly one parent through a TUniquePtr: not copyable.
        IEditorTitleBarNode(const IEditorTitleBarNode&)            = delete;
        IEditorTitleBarNode& operator=(const IEditorTitleBarNode&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Draws this node through InContext.Gui. Const: the tree is sealed before drawing.
         */
        virtual void Draw(EditorContext& InContext) const = 0;

        /** @return How many command nodes this node counts, itself and below. */
        virtual Uint64 CountCommands() const noexcept = 0;

        /**
         * Narrows to a concrete kind for the get-or-create lookups; null when it is not one.
         */
        virtual EditorTitleBarCategory*    AsCategory() noexcept { return nullptr; }
        virtual EditorTitleBarCommandNode* AsCommand()  noexcept { return nullptr; }

        // =============================================================================
        // Getter

        OpaaxStringID      GetID() const noexcept { return m_ID; }
        const char*        GetLabel() const { return m_ID.CStr(); }
        const OpaaxString& GetPath() const noexcept { return m_Path; }

        // End Getter
        // =============================================================================

        // =============================================================================
        // Members
        // =============================================================================
    private:
        OpaaxStringID m_ID;
        OpaaxString   m_Path;
    };
}
