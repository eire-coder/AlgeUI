-- AlgeUI_Project/premake5.lua

workspace "AlgeUI"
   architecture "x64"
   startproject "AlgeUIApp"

   configurations { "Debug", "Release", "Dist" }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "AlgeUIExternal.lua"

include "AlgeUI"
include "AlgeUIApp"