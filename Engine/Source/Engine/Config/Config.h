#pragma once

#include "Core/Events/Delegate.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/ObjectMacros.h"

namespace Opaax
{
    class IPropertyVisitor;

    // No payload: a subscriber already holds &GetData() and re-reads the fields it uses.
    DECLARE_MULTICAST_DELEGATE(FOnConfigChanged)

    // =============================================================================
    // Config — a block of project settings saved in <ProjectRoot>/Configs/<FileName>, like Unreal's
    //   developer settings: an Object class. IConfigSystem makes one of each class, named after its
    //   file; the editor's Config panel lists them all and draws their values. A concrete config
    //   derives from TConfig<TData> (Engine/Config/TConfig.h) and registers with OPAAX_REGISTER_OBJECT,
    //   so the engine makes it at startup; an editor config is made by its first Get instead.
    // =============================================================================
    class Config : public Object
    {
        OPAAX_ABSTRACT_OBJECT(Config, Object)

        // =========================================================================
        // Functions
        // =========================================================================
    public:
        /** File name inside the project's Configs/ folder ("Engine.config"). */
        virtual const char* FileName() const = 0;

        /** The name a config of InFileName gets: the file name without its last extension ("Engine"). */
        static OpaaxStringID NameFromFile(const char* InFileName);

        /** The current values as text, as Save writes them. */
        virtual OpaaxString ToText() const = 0;

        /**
         * Loads the values from InAbsPath. A missing file is written with the defaults.
         * @return False if the file exists but could not be read (the defaults stay)
         */
        virtual bool Load(const OpaaxString& InAbsPath) = 0;

        /** Writes the values to InAbsPath. */
        virtual bool Save(const OpaaxString& InAbsPath) = 0;

        /**
         * Writes the values to the file of the last Load.
         * @return False if Load was never called
         */
        virtual bool Save() = 0;

        /**
         * Visits the values, for the editor to draw them.
         * @return False when they have no property list (the editor shows their text instead)
         */
        virtual bool VisitValues(IPropertyVisitor& InVisitor) = 0;

        /** Sets every value back to its default. Neither saved nor announced. */
        virtual void ResetToDefaults() = 0;

        /**
         * Merges InValues, JSON shaped like the file ({"Time": {"FixedRate": 30}}), into the values,
         * without saving: what a test or an agent changes for one run. Announced when it applies.
         * @return False with OutError, nothing changed, when InValues is not JSON, names a value the
         *   config does not have, or gives one of the wrong type
         */
        virtual bool MergeText(const OpaaxString& InValues, OpaaxString& OutError) = 0;

        /**
         * Fired after the values changed. Subscribe with AddMember(this, ...) and call RemoveAll(this)
         * before the subscriber is destroyed. Main thread only.
         */
        FOnConfigChanged& OnChanged() noexcept { return m_OnChanged; }

        /** Called by whoever changed the values (the Config panel, Load). */
        void NotifyChanged() { m_OnChanged.Broadcast(); }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        FOnConfigChanged m_OnChanged;
    };
}
