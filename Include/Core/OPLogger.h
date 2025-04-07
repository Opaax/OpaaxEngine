#pragma once
#include <fstream>
#include <mutex>

#include "Boost/OPBoostTypes.h"
#include "Core/OPLogCategory.h"
#include "OpaaxTypes.h"
#include "OpaaxConst.h"

/**
 * @class OPLogger
 * 
 * The Logger class provides functionality to log messages to a file and console with thread safety.
 * TODO: Give the possibility to use c++ 20 format?
 */
class OPLogger final
{
    //-----------------------------------------------------------------
    // MEMBERS
    //-----------------------------------------------------------------
private:
    std::mutex      m_log_mutex;
    std::ofstream   m_logFile;
    OString         m_logPath;
    
    //-----------------------------------------------------------------
    // CTOR / DTOR
    //-----------------------------------------------------------------
public:
    OPLogger(const OPLogger& other) = default;
    OPLogger(OPLogger&& other) noexcept = default;
    
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
    OString LogFormat(ELogCategory LogCategory, const OString& Message);

    /**
     * @param LogCategory The log category for which the color is determined
     * @return The color code for logging based on the specified log category
     */
    OString LogColor(ELogCategory LogCategory);

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
    void Log(ELogCategory LogCategory, const OString& Message);

    //-----------------------------------------------------------------
    // Operators
    //-----------------------------------------------------------------
    OPLogger& operator=(const OPLogger& other)
    {
        if (this == &other)
            return *this;
        return *this;
    }

    OPLogger& operator=(OPLogger&& other) noexcept = default;
};
