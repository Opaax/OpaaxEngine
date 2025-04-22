workspace "OpaaxGameEngine"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "Sandbox"
    
outputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
boostincludedir = os.getenv("BOOST_INCLUDE_DIR")
boostlibsdir = os.getenv("BOOST_LIBRARY_DIR")
vulkansdk = os.getenv("VULKAN_SDK")
vulkaninclude = (vulkansdk.."/Include")
vulkanlib = (vulkansdk.."/Lib")

-- === Projet OpaaxGameEngine ===
project "OpaaxGameEngine"
    location "OpaaxGameEngine"
    kind "SharedLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "On"

    targetdir ("Binaries/" .. outputDir .. "/%{prj.name}")
    objdir ("Intermediate/" .. outputDir .. "/%{prj.name}")
    
    pchheader "OPpch.h"
    pchsource "OpaaxGameEngine/Source/OPpch.cpp"

    files {
        "%{prj.name}/Include/**.h",
        "%{prj.name}/Include/**.hpp",
        "%{prj.name}/Source/**.cpp"
    }

    includedirs {
        "%{prj.name}/Include",
        boostincludedir,
        "%{prj.name}/ThirdParty/SDL3-3.2.10/include",
        vulkaninclude,
        "%{prj.name}/ThirdParty/glm",
        "%{prj.name}/ThirdParty/VulkanMemoryAllocator/include",
    }

    libdirs {
        boostlibsdir,
        "%{prj.name}/ThirdParty/SDL3-3.2.10/lib/x64",
        vulkanlib
    }

    links {
        "SDL3",
        "vulkan-1"
    }

    filter "system:windows"
        systemversion "latest"
        defines {
            "OPAAX_PLATFORM_WINDOWS",
            "OPAAX_BUILD_DLL",
            "_WINDLL",
            "_UNICODE",
            "UNICODE"
        }
        postbuildcommands {
            "{COPY} %{cfg.buildtarget.relpath} ../Binaries/" .. outputDir .. "/Sandbox"
        }

    filter "configurations:Debug"
        defines { "OPAAX_DEBUG_MODE" }
        symbols "On"

    filter "configurations:Release"
        defines { "OPAAX_RELEASE_MODE" }
        optimize "On"
        
-- === Projet Sandbox ===
project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "On"

    targetdir ("Binaries/" .. outputDir .. "/%{prj.name}")
    objdir ("Intermediate/" .. outputDir .. "/%{prj.name}")

    files {
        "%{prj.name}/Include/**.h",
        "%{prj.name}/Include/**.hpp",
        "%{prj.name}/Source/**.cpp"
    }

    includedirs {
        "%{prj.name}/Include",
        "OpaaxGameEngine/Include",
        boostincludedir,
        "OpaaxGameEngine/ThirdParty/SDL3-3.2.10/include",
        "OpaaxGameEngine/ThirdParty/glm",
        "OpaaxGameEngine/ThirdParty/VulkanMemoryAllocator/include",
        vulkaninclude
    }

    libdirs {
        boostlibsdir,
        "OpaaxGameEngine/ThirdParty/SDL3-3.2.10/lib/x64",
        vulkanlib
    }

    links {
        "OpaaxGameEngine",
        "SDL3",
        "vulkan-1"
    }

    filter "system:windows"
        systemversion "latest"
        defines {
            "OPAAX_PLATFORM_WINDOWS",
            "_WINDLL",
            "_UNICODE",
            "UNICODE"
        }

    filter "configurations:Debug"
        defines { "OPAAX_DEBUG_MODE" }
        symbols "On"

    filter "configurations:Release"
        defines { "OPAAX_RELEASE_MODE" }
        optimize "On"