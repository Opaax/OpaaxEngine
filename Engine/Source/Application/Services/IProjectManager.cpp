#include "IProjectManager.h"
#include "IPaths.h"

#include "Core/IO/FileIO.h"

#include <nlohmann/json.hpp>


namespace Opaax
{
    namespace
    {
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
            OpaaxString LoadingScreen() const override { return OpaaxString(); }
            float       LoadingScreenMinSeconds() const override { return 0.f; }
            float       UIReferenceHeight() const override { return 1080.f; }
        };
    }

    namespace KEProjCFG = Opaax_Project_Identity;
    
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

        // Scene-era fallbacks, newest first — older .opaaxproj files predate the World > Level >
        // Map vocabulary (X4) and store this as "startupScene" or, older still, "defaultScene".
        if (lOut.StartupLevel.IsEmpty())
        {
            lOut.StartupLevel = lReadString(Opaax_Project_Identity::PROJECT_STARTUP_LEVEL_KEY_LEGACY);
        }
        if (lOut.StartupLevel.IsEmpty())
        {
            lOut.StartupLevel = lReadString(Opaax_Project_Identity::PROJECT_STARTUP_LEVEL_KEY_DEFAULT);
        }

        lOut.LoadingScreen = lReadString(Opaax_Project_Identity::PROJECT_LOADING_SCREEN_KEY);

        // A number; anything else (absent, a string) keeps the default — tolerant, like every key here.
        const auto lReadNumber = [&lRoot](const char* InKey, float& InOutValue)
        {
            if (lRoot.contains(InKey) && lRoot[InKey].is_number())
            {
                InOutValue = lRoot[InKey].get<float>();
            }
        };

        lReadNumber(Opaax_Project_Identity::PROJECT_LOADING_SCREEN_MIN_SECONDS_KEY, lOut.LoadingScreenMinSeconds);
        lReadNumber(Opaax_Project_Identity::PROJECT_UI_REFERENCE_HEIGHT_KEY, lOut.UIReferenceHeight);

        // A height of nothing is not a canvas; the default stands in and says so.
        if (lOut.UIReferenceHeight <= 0.f)
        {
            lOut.UIReferenceHeight = 1080.f;
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
        m_Identity = ParseProjectIdentity(FileIO::ReadAllText(lFile));
    }
}
