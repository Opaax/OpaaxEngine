#pragma once

#include "IAppService.h"
#include "Core/OpaaxString.hpp"

namespace Opaax
{
    class IPaths;

    // =============================================================================
    // ProjectIdentity — the non-derivable metadata stored in the .opaaxproj.
    // Plain data, produced by ParseProjectIdentity so parsing stays pure + testable.
    // No filesystem paths live here — IPaths owns the layout (see decision 5).
    // =============================================================================
    struct ProjectIdentity
    {
        OpaaxString Name;          // display name
        OpaaxString Id;            // stable project id (uuid); "" if absent
        OpaaxString EngineVersion; // engine compat tag; "" if absent
        OpaaxString StartupLevel;  // asset-relative scene; falls back to legacy "defaultScene"
    };

    namespace Opaax_Project_Identity
    {
        inline const char* PROJECT_NAME_KEY                     = "name";
        inline const char* PROJECT_ID_KEY                       = "id";
        inline const char* PROJECT_ENGINE_VERSION_KEY           = "engineVersion";
        inline const char* PROJECT_STARTUP_LEVEL_KEY            = "startupScene";
        inline const char* PROJECT_STARTUP_LEVEL_KEY_DEFAULT    = "defaultScene";
    }

    // Pure, tolerant parser — bad JSON or missing fields yield empty values, never throws.
    //   InJsonText : the raw .opaaxproj contents.
    OPAAX_API ProjectIdentity ParseProjectIdentity(const OpaaxString& InJsonText);

    // =============================================================================
    // IProjectManager — identity of the active project, read from <ProjectRoot>/<Name>.opaaxproj.
    //
    // "What is this project" (name/id/version/startup scene). "Where is it" is IPaths'
    // job — the two never overlap. Boots after Paths, before ConfigSystem.
    // =============================================================================
    class OPAAX_API IProjectManager : public IAppService
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

        //----- null object ----------------------------------------------------
        static IProjectManager& Null();
    };

    // =============================================================================
    // ProjectManager — reads + parses Paths.ProjectFile() at construction.
    //   Missing/unreadable file => empty identity (tolerant, never crashes).
    // =============================================================================
    class OPAAX_API ProjectManager final : public IProjectManager
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

        // =============================================================================
        // Members
        // =============================================================================
    private:
        ProjectIdentity m_Identity;
    };
}
