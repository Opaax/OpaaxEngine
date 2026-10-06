#pragma once

#include "IAppService.h"
#include "Core/String/OpaaxString.hpp"

namespace Opaax
{
    class IPaths;

    // =============================================================================
    // ProjectIdentity — project metadata stored in the .opaaxproj. Paths live in IPaths.
    // =============================================================================
    struct ProjectIdentity
    {
        OpaaxString Name;          // display name
        OpaaxString Id;            // stable project id (uuid); "" if absent
        OpaaxString EngineVersion; // engine compat tag; "" if absent
        OpaaxString StartupLevel;  // asset-relative level
        OpaaxString LoadingScreen; // asset-relative .opaaxui shown while a level loads; "" = black screen
        float       LoadingScreenMinSeconds = 0.f; // minimum time the loading screen stays up
        float       UIReferenceHeight = 1080.f;    // UI canvas height, in pixels, that HUDs are designed for
    };

    namespace Opaax_Project_Identity
    {
        inline const char* PROJECT_NAME_KEY                     = "name";
        inline const char* PROJECT_ID_KEY                       = "id";
        inline const char* PROJECT_ENGINE_VERSION_KEY           = "engineVersion";
        // Read in order, first non-empty wins. The last two are old keys, kept for old projects.
        inline const char* PROJECT_STARTUP_LEVEL_KEY            = "startupLevel";
        inline const char* PROJECT_STARTUP_LEVEL_KEY_LEGACY     = "startupScene";
        inline const char* PROJECT_STARTUP_LEVEL_KEY_DEFAULT    = "defaultScene";
        inline const char* PROJECT_LOADING_SCREEN_KEY           = "loadingScreen";
        inline const char* PROJECT_LOADING_SCREEN_MIN_SECONDS_KEY = "loadingScreenMinSeconds";
        inline const char* PROJECT_UI_REFERENCE_HEIGHT_KEY       = "uiReferenceHeight";
    }

    // Parses the .opaaxproj text. Bad JSON or missing fields give empty values; never throws.
    ProjectIdentity ParseProjectIdentity(const OpaaxString& InJsonText);

    // =============================================================================
    // IProjectManager — metadata of the active project, read from <ProjectRoot>/<Name>.opaaxproj.
    // =============================================================================
    class IProjectManager : public IAppService
    {
        // =============================================================================
        // Base Implementation
        // =============================================================================
    public:
        OPAAX_SERVICE_TYPE(IProjectManager)

        virtual OpaaxString Name()          const = 0;
        virtual OpaaxString Id()            const = 0;
        virtual OpaaxString EngineVersion() const = 0;
        virtual OpaaxString StartupLevel()  const = 0;
        virtual OpaaxString LoadingScreen() const = 0;
        virtual float       LoadingScreenMinSeconds() const = 0;
        virtual float       UIReferenceHeight() const = 0;

        //----- null object ----------------------------------------------------
        static IProjectManager& Null();
    };

    // =============================================================================
    // ProjectManager — reads the project file at construction. Missing file gives empty values.
    // =============================================================================
    class ProjectManager final : public IProjectManager
    {
        // =============================================================================
        // CTOR
        // =============================================================================
    public:
        explicit ProjectManager(const IPaths& InPaths);

        // =============================================================================
        // Functions
        // =============================================================================
    public:
        OpaaxString Name()          const override { return m_Identity.Name; }
        OpaaxString Id()            const override { return m_Identity.Id; }
        OpaaxString EngineVersion() const override { return m_Identity.EngineVersion; }
        OpaaxString StartupLevel()  const override { return m_Identity.StartupLevel; }
        OpaaxString LoadingScreen() const override { return m_Identity.LoadingScreen; }
        float       LoadingScreenMinSeconds() const override { return m_Identity.LoadingScreenMinSeconds; }
        float       UIReferenceHeight() const override { return m_Identity.UIReferenceHeight; }

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ProjectIdentity m_Identity;
    };
}
