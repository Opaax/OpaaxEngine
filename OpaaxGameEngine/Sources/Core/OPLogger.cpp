#include <filesystem>
#include <iostream>

#include "Core/OPLogger.h"
#include "Core/OPLogColor.hpp"
#include "Core/OPFileSystem.h"

OPLogger::OPLogger()
{
    Init();
}

OPLogger::~OPLogger()
{
    if(m_logFile.is_open())
    {
        m_logFile.close();
    }
}

void OPLogger::Init()
{
    //Define log file path
    OString lNameSuffix = OPFileSystem::GetTimeStampDirectoryFriendly();
    m_logPath = OPAAX::CONST::LOG_DIRECTORY + OPAAX::CONST::LOG_PREFIX + lNameSuffix + OPAAX::CONST::LOG_FILE_TYPE;

    //make it relative to the project.
    STDFileSystem::path lDirPath = OPFileSystem::GetPathIfNCreate(m_logPath);

    //Open the file as writing mode.
    m_logFile.open(m_logPath, std::ios::out | std::ios::app);

    //Check is the file is correctly opened,
    //Otherwise write the first line.
    if (!m_logFile.is_open())
    {
        Log(ELogCategory::ELC_Error, (BSTFormat("Error opening file: %1%") % m_logPath).str());
        return;
    }

    m_logFile << "_______ Start at: " << OPFileSystem::GetTimeStamp() << std::endl;
};

OString OPLogger::LogFormat(ELogCategory LogCategory, const OString& Message)
{
    OString lLevel;
    switch (LogCategory) {
    case ELogCategory::ELC_Info:
        lLevel = "[INFOS]: ";
        break;
    case ELogCategory::ELC_Warning:
        lLevel = "[WARNING]: ";
        break;
    case ELogCategory::ELC_Error:
        lLevel = "[ERROR]: ";
        break;
    case ELogCategory::ELC_Debug:
        lLevel = "[DEBUG]: ";
        break;
    case ELogCategory::ELC_Verbose:
        lLevel = "[VERBOSE]: ";
        break;
    default: 
        lLevel = "[UNKNOWN]: ";
    }
    
    return lLevel + Message;
}

OString OPLogger::LogColor(ELogCategory LogCategory)
{
    OString lColor;
    switch (LogCategory) {
    case ELogCategory::ELC_Info:
        lColor = GAllLogColor.at(static_cast<Int8>(ELogColor::COLOR_LOG_RESETALL));
        break;
    case ELogCategory::ELC_Warning:
        lColor = GAllLogColor.at(static_cast<Int8>(ELogColor::COLOR_LOG_YELLOW));
        break;
    case ELogCategory::ELC_Error:
        lColor = GAllLogColor.at(static_cast<Int8>(ELogColor::COLOR_LOG_RED));
        break;
    case ELogCategory::ELC_Debug:
        lColor = GAllLogColor.at(static_cast<Int8>(ELogColor::COLOR_LOG_BLUE));
        break;
    case ELogCategory::ELC_Verbose:
        lColor = GAllLogColor.at(static_cast<Int8>(ELogColor::COLOR_LOG_GREEN));
        break;
    default: 
        lColor = GAllLogColor.at(static_cast<Int8>(ELogColor::COLOR_LOG_BLACK));
    }
    
    return lColor;
}

void OPLogger::Log(ELogCategory LogCategory, const OString& Message)
{
    std::lock_guard<std::mutex> lLockLog(m_log_mutex); // Thread safety

    OString lLogMessage = LogFormat(LogCategory, Message);
    OString lLogColor = LogColor(LogCategory);
    
    std::cout << lLogColor << lLogMessage << GAllLogColor.at(static_cast<Int8>(ELogColor::COLOR_LOG_RESETALL)) << std::endl;
    
    if (m_logFile.is_open())
    {
        m_logFile << "["<< OPFileSystem::GetTimeStamp() << "] =====> " << lLogMessage << std::endl;  // Save to file
    }
}
