#pragma once

#include <nlohmann/json.hpp>

#include "Application/Services/IAppService.h"
#include "Core/Events/Delegate.h"
#include "Core/Log/Logger.h"
#include "Core/OpaaxTypes.h"
#include "Core/String/OpaaxString.hpp"
#include "Core/String/OpaaxStringID.hpp"
#include "Core/String/OpaaxStringView.hpp"
#include "Platform/Window/DisplayMode.h"
#include "Save/SaveResult.h"

namespace Opaax
{
    class Config;
    class IConfigSystem;
    class ISaveStorage;
    class IWindowManager;
    enum class EWindowMode;
    enum class EMultisampling : Uint8;

    inline constexpr LogCategory LogUserSettings{"UserSettings"};

    /** Passed a section's name ("Keys") when its values changed. */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnUserSectionChanged, OpaaxStringView)

    // =============================================================================
    // IUserSettings — what the player chose in a settings menu, kept between launches: values over the
    //   project's configs (the window, the sound, a game's own config), and what an engine system keeps
    //   for the player in a section of its own. Saved in the "Settings" slot of the player's saves
    //   (ISaveStorage), laid out as a save file, which a person can read and edit:
    //
    //   { "Class": "UserSettings", "ClassVersion": 1, "Version": 1,
    //     "Properties": { "Configs": { "Engine": { "Window": { "Mode": "Fullscreen" },
    //                                              "Audio": { "Buses": { "Music": { "Volume": 0.5 } } } } } } }
    //
    //   Active, the values are over the configs (Config::SetPlayerText) and the engine reads them at
    //   once. A game is active from its start, before its window opens at the player's size; the editor
    //   only while Play runs (Engine::StartGame), so its own window and its Config panel keep the
    //   project's values. A change lasts once saved. Main thread only.
    // =============================================================================
    class IUserSettings : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IUserSettings)

        /** The slot the settings are saved in. A game's saves never use it (SaveGameSubsystem). */
        static constexpr const char* SLOT = "Settings";

        /** The class a settings file names. */
        static constexpr const char* FILE_CLASS = "UserSettings";

        /** The version of a settings file's properties. A file of a higher one is refused. */
        static constexpr Int32 FILE_VERSION = 1;

        /** The section of the file holding the configs' values, by config name. */
        static constexpr const char* CONFIGS_SECTION = "Configs";

        /** True when InSlot is the settings' slot, in any case: on Windows "settings" is the same file. */
        static bool IsSettingsSlot(OpaaxStringView InSlot);

        // =============================================================================
        // The file
        // =============================================================================
    public:
        /**
         * Reads the slot, in place of the values in memory; over the configs at once when active.
         * @return Missing when the player has no settings yet; Corrupt, NewerVersion or WrongClass (logged):
         *   the project's values are read, and Save never writes over a newer game's or another class's file
         */
        virtual ESaveResult Load() = 0;

        /**
         * Writes the values to the slot, whole or not at all.
         * @return NewerVersion or WrongClass when the slot holds a file Load would not read; IoError
         */
        virtual ESaveResult Save() = 0;

        /** True when the values differ from the slot's, as last loaded or saved. */
        virtual bool HasUnsavedChanges() const = 0;

        /** The slot's file, for a person to find. Empty when nothing is saved. */
        virtual OpaaxString GetFilePath() const = 0;

        /** The values as the file holds them: {"Configs": {"Engine": {...}}, and each section}. */
        virtual nlohmann::json GetProperties() const = 0;

        // =============================================================================
        // Over the configs
        // =============================================================================
    public:
        /** Puts the values over the configs, and over each config registered later. */
        virtual void Activate() = 0;

        /** Takes them off: the configs read the project's values alone. The values stay in memory. */
        virtual void Deactivate() = 0;

        /** True between Activate and Deactivate. */
        virtual bool IsActive() const = 0;

        // =============================================================================
        // The configs' values
        // =============================================================================
    public:
        /**
         * Sets values of InConfig (its file's name: "Engine"), JSON shaped like its file and merged with the
         * player's values there: {"Window": {"Mode": "Fullscreen"}}. A null takes a value away, so the
         * project's is read again: {"Window": {"Mode": null}}. An item of a list that has names is set by
         * its name: {"Audio": {"Buses": {"Music": {"Volume": 0.5}}}}. Read by the engine at once when active.
         * @return False, nothing changed, with OutRejected saying why: no such config, not a JSON object,
         *   or a value the config does not have or of the wrong type ("Window.Mod: no such value")
         */
        virtual bool SetConfigValues(OpaaxStringView InConfig, const OpaaxString& InValues,
                                     TDynArray<OpaaxString>* OutRejected = nullptr) = 0;

        /** The player's values for InConfig, shaped like its file: "{}" when there are none. */
        virtual OpaaxString GetConfigValues(OpaaxStringView InConfig) const = 0;

        /** Takes InConfig's values away: the project's are read again. */
        virtual void ResetConfig(OpaaxStringView InConfig) = 0;

        /** Takes every value away, the configs' and the sections'. */
        virtual void Reset() = 0;

        // =============================================================================
        // An engine system's own section
        // =============================================================================
    public:
        /** The section InName ("Keys"), or null when there is none. */
        virtual nlohmann::json GetSection(OpaaxStringView InName) const = 0;

        /**
         * Sets the section InName, kept as given for its system to read back; null removes it.
         * @return False for CONFIGS_SECTION, which SetConfigValues writes
         */
        virtual bool SetSection(OpaaxStringView InName, const nlohmann::json& InValues) = 0;

        /**
         * Fires with a section's name when its values changed (SetSection, Load, Reset), whoever changed
         * them: its system reads it again.
         */
        virtual FOnUserSectionChanged& OnSectionChanged() = 0;

        // =============================================================================
        // The engine's settings — what the engine reads is in the engine config (GameInstanceContext::Config)
        // =============================================================================
    public:
        /** Windowed, borderless (over the whole screen) or fullscreen (at the resolution). */
        bool SetWindowMode(EWindowMode InMode);

        /**
         * The window's size when windowed, the screen's mode closest to it in fullscreen (GetDisplayModes
         * lists them). False for a size of zero.
         */
        bool SetResolution(Uint32 InWidth, Uint32 InHeight);

        /** Waits for the screen's refresh before showing a frame. */
        bool SetVSync(bool bInVSync);

        /** The most frames a second; 0 for no cap. */
        bool SetMaxFrameRate(Uint32 InFramesPerSecond);

        /** The resolutions and refresh rates the window's screen offers in fullscreen, smallest first. */
        virtual TDynArray<DisplayMode> GetDisplayModes() const = 0;

        /** The volume over every sound, 0 to 1. */
        bool SetMasterVolume(float InVolume);

        /** A bus's volume, 0 to 1 ("Music", "SFX"...; "Master" is SetMasterVolume). False for a bus not listed. */
        bool SetBusVolume(OpaaxStringID InBus, float InVolume);

        // =============================================================================
        // The renderer's settings — what the renderer reads is in the Renderer config; the texture filter
        //   stays the project's (art direction)
        // =============================================================================
    public:
        /** Samples per pixel of the world's picture: smooth edges on quads, tiles and shapes. */
        bool SetMultisampling(EMultisampling InMultisampling);

        /** Smooths every edge of the finished world picture (FXAA); the UI stays sharp. */
        bool SetFXAA(bool bInFXAA);

        /**
         * The world's picture against the view: below 1 for a slow GPU, above 1 for smoother edges.
         * False outside MIN_RENDER_SCALE..MAX_RENDER_SCALE (0.5 to 2).
         */
        bool SetRenderScale(float InScale);

        //----- null object ----------------------------------------------------
        /** Keeps nothing: every change is refused. */
        static IUserSettings& Null();
    };

    // =============================================================================
    // UserSettings — the player's settings in a save storage's Settings slot, over a config system's
    //   configs.
    // =============================================================================
    class UserSettings final : public IUserSettings
    {
        // =============================================================================
        // CTORS - DTORS
        // =============================================================================
    public:
        /**
         * Settings in InStorage's Settings slot, over InConfigs' configs, the display modes from InWindows;
         * all three outlive it. Nothing read yet.
         */
        UserSettings(ISaveStorage& InStorage, IConfigSystem& InConfigs, const IWindowManager& InWindows);
        ~UserSettings() override;

        UserSettings(const UserSettings&)            = delete;
        UserSettings& operator=(const UserSettings&) = delete;
        UserSettings(UserSettings&&)                 = delete;
        UserSettings& operator=(UserSettings&&)      = delete;

        // =============================================================================
        // Override
        // =============================================================================
        //~Begin IAppService interface
    public:
        void OnShutdown() override;
        //~End IAppService interface

        //~Begin IUserSettings interface
    public:
        ESaveResult    Load() override;
        ESaveResult    Save() override;
        bool           HasUnsavedChanges() const override { return m_Properties != m_Saved; }
        OpaaxString    GetFilePath() const override;
        nlohmann::json GetProperties() const override { return m_Properties; }

        void Activate() override;
        void Deactivate() override;
        bool IsActive() const override { return m_bActive; }

        bool        SetConfigValues(OpaaxStringView InConfig, const OpaaxString& InValues,
                                    TDynArray<OpaaxString>* OutRejected = nullptr) override;
        OpaaxString GetConfigValues(OpaaxStringView InConfig) const override;
        void        ResetConfig(OpaaxStringView InConfig) override;
        void        Reset() override;

        nlohmann::json         GetSection(OpaaxStringView InName) const override;
        bool                   SetSection(OpaaxStringView InName, const nlohmann::json& InValues) override;
        FOnUserSectionChanged& OnSectionChanged() override { return m_OnSectionChanged; }

        TDynArray<DisplayMode> GetDisplayModes() const override;
        //~End IUserSettings interface

        // =============================================================================
        // Internal
        // =============================================================================
    private:
        /** ApplyTo every registered config. */
        void ApplyToAll();

        /** Puts the player's values for InConfig over it; those it does not take are dropped, with a warning. */
        void ApplyTo(Config& InConfig);

        /** Takes the player's values off every config. */
        void ClearConfigs();

        /** The values kept for the config InName, or null. */
        nlohmann::json FindConfigValues(const OpaaxString& InName) const;

        /** Keeps InValues for the config InName; an empty object takes its values away. */
        void StoreConfigValues(const OpaaxString& InName, const nlohmann::json& InValues);

        /** A config registered while active gets its values. */
        void HandleConfigAdded(Config& InConfig);

        /** Stops following the config system. */
        void Unbind();

        /** Tells each section that differs between InBefore and the values now (CONFIGS_SECTION aside). */
        void BroadcastSectionChanges(const nlohmann::json& InBefore);

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ISaveStorage*         m_Storage = nullptr;
        IConfigSystem*        m_Configs = nullptr;   // null once unbound
        const IWindowManager* m_Windows = nullptr;

        nlohmann::json m_Properties = nlohmann::json::object();   // the file's properties
        nlohmann::json m_Saved      = nlohmann::json::object();   // as last loaded or saved

        ESaveResult m_LoadResult = ESaveResult::Missing;
        bool        m_bActive    = false;

        FOnUserSectionChanged m_OnSectionChanged;
    };
}
