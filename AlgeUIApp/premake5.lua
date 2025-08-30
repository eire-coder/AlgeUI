-- AlgeUIApp/premake5.lua

project "AlgeUIApp"
   kind "ConsoleApp"
   language "C++"
   cppdialect "C++20"
   staticruntime "off"

   targetdir ("../bin/" .. outputdir .. "/%{prj.name}")
   objdir ("../bin-int/" .. outputdir .. "/%{prj.name}")

   files { "src/**.h", "src/**.cpp" }

   includedirs
   {
      "../AlgeUI/include",
      "%{IncludeDir.VulkanSDK}",
      "%{IncludeDir.glm}",
      "../vendor/imgui"
   }

   libdirs
   {
      "../bin/" .. outputdir .. "/AlgeUI",
      "../vendor/glfw/bin/" .. outputdir .. "/GLFW"
   }

   links
   {
      "AlgeUI",
      "GLFW",
      "ImGui"
   }

   filter "system:windows"
      systemversion "latest"
      defines { "WL_PLATFORM_WINDOWS" }

      -- Add all required Windows system libraries for GLFW and DWM API
      links
      {
         "gdi32",
         "user32",
         "kernel32",
         "shell32",
         "dwmapi",
         "ole32" -- Add this line for completeness
      }

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
      kind "WindowedApp"
      defines { "WL_DIST" }
      runtime "Release"
      optimize "On"
      symbols "Off"