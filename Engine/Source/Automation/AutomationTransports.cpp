#include "Automation/AutomationTransports.h"

#include "Core/Algo/Algo.h"
#include "Core/IO/FileIO.h"
#include "Core/IO/FileSystem.h"
#include "Core/String/OpaaxPathString.h"

namespace Opaax
{
    namespace
    {
        constexpr const char* REQUEST_SUFFIX  = ".request.json";
        constexpr const char* RESPONSE_SUFFIX = ".response.json";

        /** The file's bytes as they are. False when it cannot be read (still being written). */
        bool ReadText(const OpaaxString& InPath, OpaaxString& OutText)
        {
            TDynArray<Uint8> lBytes;
            if (!FileIO::ReadAllBytes(InPath, lBytes))
            {
                return false;
            }
            OutText = OpaaxString(reinterpret_cast<const char*>(lBytes.data()), static_cast<Uint32>(lBytes.size()));
            return true;
        }

        /** Writes beside the target, then renames: a reader never sees half a file. */
        bool WriteWhole(const OpaaxString& InPath, const OpaaxString& InText)
        {
            const OpaaxString lTemp = InPath + ".tmp";
            return FileIO::WriteAllText(lTemp, InText) && FileSystem::Rename(lTemp, InPath);
        }
    }

    // =========================================================================
    // AutomationInbox
    // =========================================================================
    AutomationInbox::AutomationInbox(OpaaxString InFolder)
        : m_Folder(Move(InFolder))
    {
    }

    bool AutomationInbox::Open()
    {
        if (!FileSystem::CreateDirectories(m_Folder))
        {
            OPAAX_LOG(LogAutomation, Error, "Cannot use '{}' as the automation inbox: it cannot be created", m_Folder);
            return false;
        }

        OPAAX_LOG(LogAutomation, Info, "Automation inbox: drop <name>{} files into '{}'", REQUEST_SUFFIX, m_Folder);
        return true;
    }

    Uint32 AutomationInbox::Poll(AutomationRunner& InRunner)
    {
        TDynArray<FileSystem::Entry> lEntries;
        FileSystem::ListDirectory(m_Folder, lEntries);

        TDynArray<FileSystem::Entry> lFiles;
        for (FileSystem::Entry& lEntry : lEntries)
        {
            if (!lEntry.bIsDirectory && lEntry.Name.EndsWith(REQUEST_SUFFIX))
            {
                lFiles.push_back(Move(lEntry));
            }
        }

        // Name order: an agent numbers its files to keep them in order.
        Algo::Sort(lFiles, [](const FileSystem::Entry& InA, const FileSystem::Entry& InB)
        {
            return InA.Name < InB.Name;
        });

        Uint32 lQueued = 0;
        for (const FileSystem::Entry& lFile : lFiles)
        {
            const Uint32      lIdLength = lFile.Name.GetLength() - OpaaxStringView(REQUEST_SUFFIX).GetLength();
            const OpaaxString lId       = lFile.Name.SubString(0, lIdLength);

            OpaaxString lText;
            if (!ReadText(lFile.AbsPath, lText))
            {
                continue;   // still being written: next frame
            }
            FileSystem::Remove(lFile.AbsPath);

            // The file's name is the request's id, so the answer's name is known in advance.
            AutomationRequest    lRequest;
            OpaaxString          lParseError;
            const nlohmann::json lJson = nlohmann::json::parse(lText.begin(), lText.end(), nullptr,
                                                               /*allow_exceptions*/ false);
            if (lJson.is_discarded() || !ParseAutomationRequest(lJson, lRequest, lParseError))
            {
                AutomationResponse lResponse;
                lResponse.Id    = lId;
                lResponse.Error = lJson.is_discarded() ? OpaaxString("not valid JSON") : lParseError;
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
        const OpaaxString lPath = PathString::Join(m_Folder, InResponse.Id + RESPONSE_SUFFIX);
        if (!WriteWhole(lPath, ToJsonText(ToJson(InResponse), 4)))
        {
            OPAAX_LOG(LogAutomation, Error, "Cannot write the answer '{}'", lPath);
        }
    }

    // =========================================================================
    // AutomationScript
    // =========================================================================
    bool AutomationScript::Load(const OpaaxString& InScript, const OpaaxString& InOutput, AutomationRunner& InRunner)
    {
        m_Script = InScript;
        m_Output = InOutput;

        OpaaxString lText;
        if (!ReadText(InScript, lText))
        {
            OPAAX_LOG(LogAutomation, Error, "Cannot read the automation script '{}'", InScript);
            return false;
        }

        TDynArray<AutomationRequest> lRequests;
        OpaaxString                  lError;
        if (!ParseAutomationScript(lText, lRequests, lError))
        {
            OPAAX_LOG(LogAutomation, Error, "Automation script '{}': {}", InScript, lError);
            return false;
        }

        for (AutomationRequest& lRequest : lRequests)
        {
            m_Pending.insert(lRequest.Id);
            InRunner.Enqueue(Move(lRequest));
        }

        m_bLoaded = true;
        OPAAX_LOG(LogAutomation, Info, "Automation script '{}': {} request(s), answers in '{}'",
                  InScript, lRequests.size(), m_Output);
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

    OpaaxString AutomationScript::DefaultOutput(const OpaaxString& InScript)
    {
        return PathString::ReplaceExtension(InScript, ".out.json");
    }

    void AutomationScript::WriteOutput() const
    {
        const nlohmann::json lJson{ { "script", PathString::Generic(m_Script) },
                                    { "done", IsDone() },
                                    { "failed", m_Failed },
                                    { "responses", m_Responses } };
        if (!WriteWhole(m_Output, ToJsonText(lJson, 4)))
        {
            OPAAX_LOG(LogAutomation, Error, "Cannot write the automation answers '{}'", m_Output);
        }
    }
}
