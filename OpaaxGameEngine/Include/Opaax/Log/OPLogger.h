#pragma once

#include "Opaax/Boost/OPBoostTypes.h"
#include "OPLogCategory.h"
#include "Opaax/OpaaxTypes.h"
#include "Opaax/OpaaxConst.h"

/**
 * @class OPLogger
 * 
 * The Logger class provides functionality to log messages to a file and console with thread safety.
 * TODO: Give the possibility to use c++ 20 format?
 * TODO: Use spdlog?
 */
#pragma warning(disable: 4251)//Mutex not dll safe
class OPAAX_API OPLogger final
{
    //-----------------------------------------------------------------
    // MEMBERS
    //-----------------------------------------------------------------
private:
    std::mutex          m_log_mutex;
    std::ofstream       m_logFile;
    OSTDString          m_logPath;
    
    //-----------------------------------------------------------------
    // CTOR / DTOR
    //-----------------------------------------------------------------
public:
    OPLogger(const OPLogger& other) = delete;
    OPLogger(OPLogger&& other) noexcept = delete;
    
    OPLogger();
    ~OPLogger();

    //-----------------------------------------------------------------
    // Functions
    //-----------------------------------------------------------------
    //-------------------------- Private -----------------------------//
private:
    /**
     * Performs initialization for the OPLogger class, including setting up log file path, opening log file,
     * and writing the initial log message with timestamp.
     * This method is responsible for initializing the logger for logging purpose.
     */
    void Init();

    /**
     * @param LogCategory The category of the log message
     * @param Message The message to be logged
     * @return The formatted log message with the appropriate log level tag
     */
    OSTDString LogFormat(ELogCategory LogCategory, const OSTDString& Message);

    /**
     * @param LogCategory The log category for which the color is determined
     * @return The color code for logging based on the specified log category
     */
    OSTDString LogColor(ELogCategory LogCategory);

    //-------------------------- Static -----------------------------//
public:
    static OPLogger& Instance()
    {
        static OPLogger lInstance;
        return lInstance;
    }

    //-------------------------- Public -----------------------------//

    /**
     * @param LogCategory The category of the message
     * @param Message The message to be logged
     */
    void Log(ELogCategory LogCategory, const OSTDString& Message);

    //-----------------------------------------------------------------
    // Operators
    //-----------------------------------------------------------------
    OPLogger& operator=(const OPLogger& other)
    {
        if (this == &other)
            return *this;
        return *this;
    }

    OPLogger& operator=(OPLogger&& other) noexcept = delete;
};
#pragma warning(default: 4251)