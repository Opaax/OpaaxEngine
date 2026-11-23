#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Tag/OpaaxTag.h"
#include "Engine/Tags/TagDefinition.h"
#include "Engine/Tags/TagRegistry.h"

namespace Opaax::Editor
{
    // =============================================================================
    // What the editor's tag fields offer, without ImGui (inline: the tests take header-only editor code).
    // =============================================================================

    /** One row of a tag picker: a registered tag under the field's root, indented by its depth. */
    struct TagPickerRow
    {
        OpaaxTag    Tag;
        OpaaxString Label;     // its last name, indented two spaces a level below the root
        OpaaxString Comment;
    };

    /** How many dotted names InTag has ("UI.Layer.Menu": 3). */
    inline Uint32 TagDepth(const OpaaxTag InTag)
    {
        Uint32 lDepth = 0;
        for (OpaaxTag lNode = InTag; lNode.IsValid(); lNode = lNode.GetParent())
        {
            ++lDepth;
        }
        return lDepth;
    }

    /** The rows a picker offers for a field rooted at InRoot (every tag with none), in name order. */
    inline TDynArray<TagPickerRow> TagPickerRows(const TagRegistry& InTags, const OpaaxTag InRoot)
    {
        const Uint32 lRootDepth = TagDepth(InRoot);

        TDynArray<TagPickerRow> lRows;
        for (const TagRegistry::Entry& lEntry : InTags.Under(InRoot))
        {
            OpaaxString lLabel;
            for (Uint32 i = lRootDepth + 1; i < TagDepth(lEntry.Tag); ++i)
            {
                lLabel += "  ";
            }
            lLabel += OpaaxString(lEntry.Tag.GetLeafName());

            lRows.push_back(TagPickerRow{ lEntry.Tag, Move(lLabel), lEntry.Comment });
        }
        return lRows;
    }

    /** Whether InTag is set but not registered: shown as a warning, with a way to register it. */
    inline bool IsTagUnregistered(const TagRegistry& InTags, const OpaaxTag InTag)
    {
        return InTag.IsValid() && !InTags.IsRegistered(InTag);
    }

    /** InExisting with InTag added at its end (Tags.config's list after "Register"); as it is when listed. */
    inline TDynArray<TagDefinition> WithTagListed(const TDynArray<TagDefinition>& InExisting, const OpaaxTag InTag)
    {
        TDynArray<TagDefinition> lTags = InExisting;
        if (!InTag.IsValid())
        {
            return lTags;
        }
        for (const TagDefinition& lDefinition : lTags)
        {
            if (lDefinition.Tag == InTag)
            {
                return lTags;
            }
        }
        lTags.push_back(TagDefinition{ InTag, OpaaxString() });
        return lTags;
    }

    /** The editor's tag registry (the engine's). */
    const TagRegistry* GetEditorTagRegistry();

    /** Lists InTag in the project's Tags.config and saves it: the registry and every picker follow. */
    bool RegisterTagInConfig(OpaaxTag InTag);
}
