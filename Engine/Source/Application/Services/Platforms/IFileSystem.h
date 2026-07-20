#pragma once

#include "Core/EngineAPI.h"
#include "Core/OpaaxString.hpp"

namespace Opaax
{
    class OPAAX_API IFileSystem
    {
    public:
        virtual ~IFileSystem() {}

        // =============================================================================
        // Functions
        // =============================================================================
    private:
        /**
         * Create directories for the specified path.
         *
         * @param InPath The path for which directories need to be created.
         * @return true if the directories are successfully created, false otherwise.
         */
        bool CreateDirectories(const OpaaxString& InPath);

        /**
         * Check if the specified path exists.
         *
         * @param InPath The path to check for existence.
         * @return true if the path exists, false otherwise.
         */
        bool IsPathExist(const OpaaxString& InPath);
        
        /**
         * Get the path after creating all necessary directories along the way.
         *
         * @param InPath The input path for which directories need to be created.
         * @return The final path after creating all necessary directories. Empty path if directories cannot be created.
         */
        OpaaxString GetPathIfNCreate(const OpaaxString& InPath);
    };
}
