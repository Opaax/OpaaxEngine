workspace "OpaaxGameEngine"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "Sandbox"
    
outputDir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"
boostincludedir = os.getenv("BOOST_INCLUDE_DIR")
boostlibsdir = os.getenv("BOOST_LIBRARY_DIR")

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
        "%{prj.name}/ThirdParty/glfw-3.4.bin.WIN64/include",
    }

    libdirs {
        boostlibsdir,
        "%{prj.name}/ThirdParty/glfw-3.4.bin.WIN64/lib-vc2022"
    }

    links {
        "glfw3.lib"
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
        "OpaaxGameEngine/ThirdParty/glfw-3.4.bin.WIN64/include",
    }

    libdirs {
        boostlibsdir,
        "OpaaxGameEngine/ThirdParty/glfw-3.4.bin.WIN64/lib-vc2022"
    }

    links {
        "OpaaxGameEngine",
        "glfw3.lib"
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