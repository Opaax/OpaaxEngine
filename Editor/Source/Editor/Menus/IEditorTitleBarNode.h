#pragma once

#include "Application/Services/ILogger.h"   // OPAAX_LOG_CATEGORY — shared by every node's .cpp
#include "Core/OpaaxTypes.h"                // TFunction, Move
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"    // interned identity — dedupe is an integer compare

namespace Opaax::Editor
{
    OPAAX_LOG_CATEGORY(EditorMenu);

    struct EditorContext;
    class EditorTitleBarCategory;
    class EditorTitleBarCommandNode;

    /**
     * A live question asked at DRAW time — is this node enabled, is it shown checked.
     *
     * Optional everywhere: an EMPTY predicate means "always", which is what keeps the ordinary
     * entry a single AddCommand line. Evaluated every frame, so a node greys itself the moment the
     * editor's state changes rather than at some refresh point somebody has to remember.
     */
    using FMenuPredicate = TFunction<bool(const EditorContext&)>;

    /**
     * @class IEditorTitleBarNode
     *
     * One node of the menu bar: a category, a command or a separator.
     *
     * Identity is an OpaaxStringID and it doubles as the LABEL, so a category cannot be looked up
     * by one name and drawn under another. CStr() into the intern pool is free and valid for the
     * life of the process (I2), which is what the label passed to IEditorGui points into.
     *
     * The full slash path ("File/Save Map") is built ONCE at construction from the parent's, and is
     * what the invocation log names — it is the only place in the editor that still speaks paths,
     * and it is a read-out rather than the storage it used to be.
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

        // A node is owned by exactly one parent, through a TUniquePtr, and hands out references
        // that outlive the call — copying one would be a second owner of the same entry.
        IEditorTitleBarNode(const IEditorTitleBarNode&)            = delete;
        IEditorTitleBarNode& operator=(const IEditorTitleBarNode&) = delete;

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        /**
         * Emit this node, through InContext.Gui — a node names no UI backend.
         *
         * CONST for the reason EditorCommandRegistry::Execute is: the tree is built before the
         * extension registrar seals, and drawing must not be able to add to it afterwards.
         */
        virtual void Draw(EditorContext& InContext) const = 0;

        /** @return How many COMMAND nodes this node accounts for, itself and below. */
        virtual Uint64 CountCommands() const noexcept = 0;

        /**
         * Narrow to a concrete kind, for the get-or-create lookups — null when this is not one.
         *
         * A pair of one-line virtuals rather than a dynamic_cast: the lookup asks "is the child
         * under this id the kind I am about to extend?", and the node itself is the only thing
         * that has to know the answer.
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
