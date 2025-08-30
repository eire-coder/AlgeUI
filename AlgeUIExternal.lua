-- AlgeUIExternal.lua

VULKAN_SDK = os.getenv("VULKAN_SDK")

IncludeDir = {}
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"
IncludeDir["glm"] = "../vendor/glm"

LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"

group "Dependencies"
   include "vendor/imgui"
   
   -- Define the GLFW project directly here
   project "GLFW"
      kind "StaticLib"
      language "C"
      staticruntime "off"

      targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
      objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

      -- Common, platform-independent files AND the Null backend fallback
      files
      {
         "vendor/glfw/include/GLFW/glfw3.h",
         "vendor/glfw/include/GLFW/glfw3native.h",
         "vendor/glfw/src/internal.h",
         "vendor/glfw/src/mappings.h",
         "vendor/glfw/src/context.c",
         "vendor/glfw/src/init.c",
         "vendor/glfw/src/input.c",
         "vendor/glfw/src/monitor.c",
         "vendor/glfw/src/platform.c",
         "vendor/glfw/src/vulkan.c",
         "vendor/glfw/src/window.c",
         
         -- Add the Null backend files
         "vendor/glfw/src/null_platform.h",
         "vendor/glfw/src/null_init.c",
         "vendor/glfw/src/null_joystick.c",
         "vendor/glfw/src/null_monitor.c",
         "vendor/glfw/src/null_window.c"
      }

      includedirs
      {
         "vendor/glfw/include",
         "vendor/glfw/src"
      }
      
      filter "system:windows"
         systemversion "latest"
         
         -- Windows-specific source files
         files
         {
            "vendor/glfw/src/win32_init.c",
            "vendor/glfw/src/win32_joystick.c",
            "vendor/glfw/src/win32_module.c",
            "vendor/glfw/src/win32_monitor.c",
            "vendor/glfw/src/win32_thread.c",
            "vendor/glfw/src/win32_time.c",
            "vendor/glfw/src/win32_window.c",
            "vendor/glfw/src/wgl_context.c",
            "vendor/glfw/src/egl_context.c",
            "vendor/glfw/src/osmesa_context.c",
            "vendor/glfw/src/win32_platform.h",
            "vendor/glfw/src/win32_joystick.h",
            "vendor/glfw/src/wgl_context.h",
            "vendor/glfw/src/egl_context.h",
            "vendor/glfw/src/osmesa_context.h"
         }

         defines 
         { 
            "_GLFW_WIN32",
            "_CRT_SECURE_NO_WARNINGS" 
         }

      filter "configurations:Debug"
         runtime "Debug"
         symbols "on"

      filter "configurations:Release"
         runtime "Release"
         optimize "on"
         symbols "on"

      filter "configurations:Dist"
         runtime "Release"
         optimize "on"
         symbols "off"
		 
group ""

group "Core"
   include "AlgeUI"
group ""