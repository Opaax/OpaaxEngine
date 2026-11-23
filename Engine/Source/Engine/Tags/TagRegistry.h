#pragma once

#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/Tag/OpaaxTag.h"
#include "Engine/Tags/TagDefinition.h"

namespace Opaax
{
    /** Where a registered tag comes from. */
    enum class ETagSource : Uint8
    {
        Native,   // declared in code (OPAAX_REGISTER_TAG)
        Config,   // listed in the project's Tags.config
        Implied   // the parent of a registered tag ("UI.Layer" of "UI.Layer.Menu")
    };

    inline const char* ToString(const ETagSource InSource) noexcept
    {
        switch (InSource)
        {
            case ETagSource::Native: return "Native";
            case ETagSource::Config: return "Config";
            case ETagSource::Implied: return "Implied";
        }
        return "?";
    }

    // =============================================================================
    // TagRegistry — the tags a project uses, like Unreal's gameplay tag table: the ones code declares
    //   and the ones Tags.config lists, each with what it is for, and their parents. Systems name their
    //   data by tags (UI layers, sound buses, an entity's tags); the editor's pickers offer the
    //   registered ones under a root. A tag that is not registered still works: the systems warn.
    // =============================================================================
    class TagRegistry
    {
    public:
        struct Entry
        {
            OpaaxTag    Tag;
            OpaaxString Comment;
            ETagSource  Source = ETagSource::Native;
        };

        /** A tag declared in code. Ignored once sealed; a second declaration keeps the first comment. */
        void AddNative(OpaaxTag InTag, const OpaaxString& InComment);

        /** No more native tags (the modules are registered). The config's tags still change. */
        void Seal() noexcept { m_bSealed = true; }

        /** The project's tags (Tags.config), in place of the ones given before. A native tag listed again stays native. */
        void SetConfigTags(const TDynArray<TagDefinition>& InTags);

        /** Whether InTag is declared, listed, or the parent of one that is. */
        bool IsRegistered(OpaaxTag InTag) const { return Find(InTag) != nullptr; }

        /** InTag's entry, or null when it is not registered. */
        const Entry* Find(OpaaxTag InTag) const noexcept;

        /** The registered tags under InRoot (InRoot itself not included), sorted by name; every tag with no root. */
        TDynArray<Entry> Under(OpaaxTag InRoot) const;

        /** Every registered tag, sorted by name. */
        const TDynArray<Entry>& GetAll() const noexcept { return m_All; }

        /** Changes whenever the registered tags do (a picker or a system knows to read them again). */
        Uint64 GetRevision() const noexcept { return m_Revision; }

    private:
        /** m_All from the native and the config tags, with their parents, sorted. */
        void Rebuild();

        TDynArray<Entry> m_Native;
        TDynArray<Entry> m_Config;
        TDynArray<Entry> m_All;
        Uint64           m_Revision = 0;
        bool             m_bSealed  = false;
    };
}
