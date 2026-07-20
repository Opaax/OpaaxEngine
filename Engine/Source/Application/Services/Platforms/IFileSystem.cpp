#include "IFileSystem.h"

#include <iostream>

#include "Application/OpaaxApplication.h"
#include "Application/Services/ILogger.h"
#include "Core/OpaaxForward.h"

namespace Opaax
{
    namespace  STDFileSyt = std::filesystem;

    bool IFileSystem::CreateDirectories(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty())
        {
            return false;
        }
        
        STDFileSyt::path lPath(InPath.CStr());
        return STDFileSyt::create_directories(lPath);
    }

    bool IFileSystem::IsPathExist(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty())
        {
            return false;
        }
        
        STDFileSyt::path lPath(InPath.CStr());
        return STDFileSyt::exists(lPath);
    }

    OpaaxString IFileSystem::GetPathIfNCreate(const OpaaxString& InPath)
    {
        if (InPath.IsEmpty())
        {
            return {};
        }

        if(!IsPathExist(InPath))
        {
            try
            {
                CreateDirectories(InPath);
                return InPath;
            }
            catch ([[maybe_unused]] const STDFileSyt::filesystem_error& lError)
            {
                ILogger& Logger = OpaaxApplication::GetAppService<ILogger>();
                
                if (!Logger.IsNull())
                {
                    OPAAX_APP_LOG(Error,"IFileSystem Connot create: {}", lError.what());
                }else
                {
                    std::cout << "IFileSystem Connot create: "<< lError.what() << std::endl;
                }
                
                return {};
            }
        }

        return InPath;
    }
}

