#include "Automation/AutomationTransports.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <system_error>

namespace Opaax
{
    namespace
    {
        constexpr const char* REQUEST_SUFFIX  = ".request.json";
        constexpr const char* RESPONSE_SUFFIX = ".response.json";

        bool EndsWith(const std::string& InText, const std::string& InSuffix)
        {
            return InText.size() >= InSuffix.size()
                && InText.compare(InText.size() - InSuffix.size(), InSuffix.size(), InSuffix) == 0;
        }

        bool ReadText(const std::filesystem::path& InPath, std::string& OutText)
        {
            std::ifstream lFile(InPath, std::ios::binary);
            if (!lFile)
            {
                return false;
            }
            std::ostringstream lStream;
            lStream << lFile.rdbuf();
            OutText = lStream.str();
            return true;
        }

        /** Writes beside the target, then renames: a reader never sees half a file. */
        bool WriteWhole(const std::filesystem::path& InPath, const std::string& InText)
        {
            std::filesystem::path lTemp = InPath;
            lTemp += ".tmp";
            {
                std::ofstream lFile(lTemp, std::ios::binary | std::ios::trunc);
                if (!lFile)
                {
                    return false;
                }
                lFile << InText;
            }

            std::error_code lError;
            std::filesystem::rename(lTemp, InPath, lError);
            return !lError;
        }
    }

    // =========================================================================
    // AutomationInbox
    // =========================================================================
    AutomationInbox::AutomationInbox(std::filesystem::path InFolder)
        : m_Folder(Move(InFolder))
    {
    }

    bool AutomationInbox::Open()
    {
        std::error_code lError;
        std::filesystem::create_directories(m_Folder, lError);
        if (lError || !std::filesystem::is_directory(m_Folder))
        {
            OPAAX_LOG(LogAutomation, Error, "Cannot use '{}' as the automation inbox: {}", m_Folder.string(), lError.message());
            return false;
        }

        OPAAX_LOG(LogAutomation, Info, "Automation inbox: drop <name>{} files into '{}'", REQUEST_SUFFIX, m_Folder.string());
        return true;
    }

    Uint32 AutomationInbox::Poll(AutomationRunner& InRunner)
    {
        std::error_code lError;
        TDynArray<std::filesystem::path> lFiles;
        for (std::filesystem::directory_iterator lIt(m_Folder, lError), lEnd; !lError && lIt != lEnd; lIt.increment(lError))
        {
            if (lIt->is_regular_file() && EndsWith(lIt->path().filename().string(), REQUEST_SUFFIX))
            {
                lFiles.push_back(lIt->path());
            }
        }

        // Name order: an agent numbers its files to keep them in order.
        std::sort(lFiles.begin(), lFiles.end());

        Uint32 lQueued = 0;
        for (const std::filesystem::path& lPath : lFiles)
        {
            const std::string lName = lPath.filename().string();
            const std::string lId   = lName.substr(0, lName.size() - std::string(REQUEST_SUFFIX).size());

            std::string lText;
            if (!ReadText(lPath, lText))
            {
                continue;   // still being written: next frame
            }
            std::filesystem::remove(lPath, lError);

            // The file's name is the request's id, so the answer's name is known in advance.
            AutomationRequest lRequest;
            std::string       lParseError;
            const nlohmann::json lJson = nlohmann::json::parse(lText, nullptr, /*allow_exceptions*/ false);
            if (lJson.is_discarded() || !ParseAutomationRequest(lJson, lRequest, lParseError))
            {
                AutomationResponse lResponse;
                lResponse.Id    = lId;
                lResponse.Error = lJson.is_discarded() ? "not valid JSON" : lParseError;
                Write(lResponse);
                continue;
            }

            lRequest.Id = lId;
            m_Pending.insert(lId);
            InRunner.Enqueue(Move(lRequest));
            ++lQueued;
        }

        return lQueued;
    }

    void AutomationInbox::Answer(const AutomationResponse& InResponse)
    {
        if (m_Pending.erase(InResponse.Id) == 0)
        {
            return;
        }
        Write(InResponse);
    }

    void AutomationInbox::Write(const AutomationResponse& InResponse) const
    {
        const std::filesystem::path lPath = m_Folder / (InResponse.Id + RESPONSE_SUFFIX);
        if (!WriteWhole(lPath, ToJson(InResponse).dump(4)))
        {
            OPAAX_LOG(LogAutomation, Error, "Cannot write the answer '{}'", lPath.string());
        }
    }

    // =========================================================================
    // AutomationScript
    // =========================================================================
    bool AutomationScript::Load(const std::filesystem::path& InScript, const std::filesystem::path& InOutput,
                                AutomationRunner& InRunner)
    {
        m_Script = InScript;
        m_Output = InOutput;

        std::string lText;
        if (!ReadText(InScript, lText))
        {
            OPAAX_LOG(LogAutomation, Error, "Cannot read the automation script '{}'", InScript.string());
            return false;
        }

        TDynArray<AutomationRequest> lRequests;
        std::string                  lError;
        if (!ParseAutomationScript(lText, lRequests, lError))
        {
            OPAAX_LOG(LogAutomation, Error, "Automation script '{}': {}", InScript.string(), lError);
            return false;
        }

        for (AutomationRequest& lRequest : lRequests)
        {
            m_Pending.insert(lRequest.Id);
            InRunner.Enqueue(Move(lRequest));
        }

        m_bLoaded = true;
        OPAAX_LOG(LogAutomation, Info, "Automation script '{}': {} request(s), answers in '{}'",
                  InScript.string(), lRequests.size(), m_Output.string());
        WriteOutput();
        return true;
    }

    void AutomationScript::Answer(const AutomationResponse& InResponse)
    {
        if (m_Pending.erase(InResponse.Id) == 0)
        {
            return;
        }

        m_Responses.push_back(ToJson(InResponse));
        if (!InResponse.bOk)
        {
            ++m_Failed;
        }
        WriteOutput();
    }

    std::filesystem::path AutomationScript::DefaultOutput(const std::filesystem::path& InScript)
    {
        std::filesystem::path lOutput = InScript;
        lOutput.replace_extension(".out.json");
        return lOutput;
    }

    void AutomationScript::WriteOutput() const
    {
        const nlohmann::json lJson{ { "script", m_Script.generic_string() },
                                    { "done", IsDone() },
                                    { "failed", m_Failed },
                                    { "responses", m_Responses } };
        if (!WriteWhole(m_Output, lJson.dump(4)))
        {
            OPAAX_LOG(LogAutomation, Error, "Cannot write the automation answers '{}'", m_Output.string());
        }
    }
}
