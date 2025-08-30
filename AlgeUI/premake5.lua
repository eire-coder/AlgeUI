-- AlgeUI/premake5.lua

project "AlgeUI"
   kind "StaticLib"
   language "C++"
   cppdialect "C++20"
   staticruntime "off"

   targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

   files 
   { 
      "src/**.h", 
      "src/**.cpp",
      "include/**.h"
   }

   includedirs
   {
      "src",
      "include",
      "../vendor/imgui",
      "../vendor/glfw/include", -- For headers
      "../vendor/stb_image",
      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.glm}",
   }
   
   -- CRITICAL FIX: Tells the linker where to find the compiled GLFW.lib file.
   libdirs
   {
      "../vendor/glfw/bin/" .. outputdir .. "/GLFW"
   }

   links
   {
      "ImGui",
      "GLFW",
      "%{Library.Vulkan}",
   }

   filter "system:windows"
      systemversion "latest"
      defines { "WL_PLATFORM_WINDOWS" }

   filter "configurations:Debug"
      defines { "WL_DEBUG" }
      runtime "Debug"
      symbols "On"

   filter "configurations:Release"
      defines { "WL_RELEASE" }
      runtime "Release"
      optimize "On"
      symbols "On"

   filter "configurations:Dist"
      defines { "WL_DIST" }
      runtime "Release"
      optimize "On"
      symbols "Off"