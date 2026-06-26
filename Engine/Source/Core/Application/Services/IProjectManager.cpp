#include "IProjectManager.h"
#include "IPaths.h"

#include "Core/Log/OpaaxLog.h"

#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

namespace Opaax
{
    namespace
    {
        OpaaxString ReadFileText(const OpaaxString& InPath)
        {
            std::ifstream lFile(InPath.CStr());
            if (!lFile.is_open()) { return OpaaxString(); }

            std::stringstream lBuffer;
            lBuffer << lFile.rdbuf();
            return OpaaxString(lBuffer.str().c_str());
        }

        // =====================================================================
        // NullProjectManager — empty identity.
        // =====================================================================
        class NullProjectManager final : public IProjectManager
        {
        public:
            bool        IsNull()        const noexcept override { return true; }
            OpaaxString Name()          const override { return OpaaxString(); }
            OpaaxString Id()            const override { return OpaaxString(); }
            OpaaxString EngineVersion() const override { return OpaaxString(); }
            OpaaxString StartupLevel()  const override { return OpaaxString(); }
        };
    }

    // =========================================================================
    // Pure parser — tolerant: bad JSON / missing fields => empty values.
    // =========================================================================
    ProjectIdentity ParseProjectIdentity(const OpaaxString& InJsonText)
    {
        ProjectIdentity lOut;
        if (InJsonText.IsEmpty()) { return lOut; }

        nlohmann::json lRoot;
        try
        {
            lRoot = nlohmann::json::parse(InJsonText.CStr());
        }
        catch (const nlohmann::json::parse_error&)
        {
            return lOut; // tolerant — empty identity on malformed JSON
        }

        const auto lReadString = [&lRoot](const char* InKey) -> OpaaxString
        {
            if (lRoot.contains(InKey) && lRoot[InKey].is_string())
            {
                return OpaaxString(lRoot[InKey].get<std::string>().c_str());
            }
            return OpaaxString();
        };

        lOut.Name          = lReadString(Opaax_Project_Identity::PROJECT_NAME_KEY);
        lOut.Id            = lReadString(Opaax_Project_Identity::PROJECT_ID_KEY);
        lOut.EngineVersion = lReadString(Opaax_Project_Identity::PROJECT_ENGINE_VERSION_KEY);
        lOut.StartupLevel  = lReadString(Opaax_Project_Identity::PROJECT_STARTUP_LEVEL_KEY);

        // Legacy fallback — older .opaaxproj files store the scene as "defaultScene".
        if (lOut.StartupLevel.IsEmpty())
        {
            lOut.StartupLevel = lReadString(Opaax_Project_Identity::PROJECT_STARTUP_LEVEL_KEY_DEFAULT);
        }
        return lOut;
    }

    // =========================================================================
    // Type tag + null object (out-of-line — one instance across the DLL/exe line).
    // =========================================================================
    ServiceTypeID IProjectManager::StaticTypeID() noexcept
    {
        static const int s_Tag = 0;
        return reinterpret_cast<ServiceTypeID>(&s_Tag);
    }

    IProjectManager& IProjectManager::Null()
    {
        static NullProjectManager s_Null;
        return s_Null;
    }

    // =========================================================================
    // ProjectManager
    // =========================================================================
    ProjectManager::ProjectManager(const IPaths& InPaths)
    {
        const OpaaxString lFile = InPaths.ProjectFile();
        m_Identity = ParseProjectIdentity(ReadFileText(lFile));

        OPAAX_CORE_INFO("ProjectManager: '{0}' (name='{1}')", lFile.CStr(), m_Identity.Name.CStr());
    }
}
