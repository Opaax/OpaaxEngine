#pragma once

#include "Core/Events/Delegate.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Object/ObjectMacros.h"

namespace Opaax
{
    class IPropertyVisitor;

    inline constexpr LogCategory LogConfig{"Config"};

    // No payload: a subscriber already holds &GetData() and re-reads the fields it uses.
    DECLARE_MULTICAST_DELEGATE(FOnConfigChanged)

    // =============================================================================
    // Config — a block of project settings saved in <ProjectRoot>/Configs/<FileName>, like Unreal's
    //   developer settings: an Object class. IConfigSystem makes one of each class, named after its
    //   file; the editor's Config panel lists them all and draws their values. A concrete config
    //   derives from TConfig<TData> (Engine/Config/TConfig.h) and registers with OPAAX_REGISTER_OBJECT,
    //   so the engine makes it at startup; an editor config is made by its first Get instead.
    //
    //   Two layers: the project's values (the file, what the Config panel edits) and the player's over
    //   them (what a player chose in a settings menu, kept in the player's own folder). The engine
    //   reads the project's values with the player's over them.
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

        /** The project's values as text, as Save writes them: the player's are not in it. */
        virtual OpaaxString ToText() const = 0;

        /** What the engine reads as text: the project's values with the player's over them. */
        virtual OpaaxString ToEffectiveText() const { return ToText(); }

        /**
         * Loads the project's values from InAbsPath. A missing file is written with the defaults.
         * @return False if the file exists but could not be read (the defaults stay)
         */
        virtual bool Load(const OpaaxString& InAbsPath) = 0;

        /** Writes the project's values to InAbsPath. */
        virtual bool Save(const OpaaxString& InAbsPath) = 0;

        /**
         * Writes the values to the file of the last Load.
         * @return False if Load was never called
         */
        virtual bool Save() = 0;

        /**
         * Visits the project's values, for the editor to draw and edit them. Call NotifyChanged after an edit.
         * @return False when they have no property list (the editor shows their text instead)
         */
        virtual bool VisitValues(IPropertyVisitor& InVisitor) = 0;

        /** Sets every project value back to its default. Neither saved nor announced. */
        virtual void ResetToDefaults() = 0;

        /**
         * Merges InValues, JSON shaped like the file ({"Time": {"FixedRate": 30}}), into the project's
         * values, without saving: what a test or an agent changes for one run. Announced when it applies.
         * A value the player set still wins over it.
         * @return False with OutError, nothing changed, when InValues is not JSON, names a value the
         *   config does not have, or gives one of the wrong type
         */
        virtual bool MergeText(const OpaaxString& InValues, OpaaxString& OutError) = 0;

        // =========================================================================
        // The player's values — over the project's (TConfig has them; a config of its own has none)
        // =========================================================================

        /**
         * Sets the player's values, JSON shaped like the file ({"Window": {"Mode": "Fullscreen"}}); "{}"
         * removes them. An item of a list that has names is set by its name: {"Audio": {"Buses":
         * {"Music": {"Volume": 0.5}}}}. A value the config does not have, or of the wrong type, is left
         * out and named in OutRejected ("Window.Mod: no such value"). Announced when what the engine
         * reads changed.
         * @return False when InValues is not a JSON object, or the config takes no player's values:
         *   nothing changed
         */
        virtual bool SetPlayerText(const OpaaxString& InValues, TDynArray<OpaaxString>* OutRejected = nullptr)
        {
            (void)InValues;
            (void)OutRejected;
            return false;
        }

        /** The player's values, as SetPlayerText takes them: "{}" when there are none. */
        virtual OpaaxString GetPlayerText() const { return OpaaxString("{}"); }

        /** True when the player has values over this config. */
        virtual bool HasPlayerValues() const { return false; }

        // =========================================================================
        // Changes
        // =========================================================================

        /**
         * Fired after the values the engine reads changed. Subscribe with AddMember(this, ...) and call
         * RemoveAll(this) before the subscriber is destroyed. Main thread only.
         */
        FOnConfigChanged& OnChanged() noexcept { return m_OnChanged; }

        /**
         * Called by whoever changed the project's values (the Config panel, Load): what the engine reads
         * is brought up to date, then announced.
         */
        void NotifyChanged()
        {
            RefreshValues();
            Announce();
        }

    protected:
        /** Brings what the engine reads up to date with the project's values and the player's. */
        virtual void RefreshValues() {}

        /** Tells the subscribers the values changed. */
        void Announce() { m_OnChanged.Broadcast(); }

        // =========================================================================
        // Members
        // =========================================================================
    private:
        FOnConfigChanged m_OnChanged;
    };
}
