Config = {}

local configLoader, err = load(io.readfile("config.lua"), "config.lua", "t", Config)
if (not configLoader) then
	error("config.lua failed to load: " .. err)
end

local configLoaded, err = pcall(configLoader)
if (not configLoaded) then
	error("config.lua failed to load: " .. err)
end

workspace("obs-kinect")
	configurations({ "Debug", "Release" })
	platforms({ "x86", "x86_64" })

	if (_ACTION) then
		location("build/" .. _ACTION)
	end

	project("obs-kinect")
		kind("SharedLib")
		language("C++")
		cppdialect("C++17")
		targetdir("bin/%{cfg.buildcfg}")

		files({ "src/**.hpp", "src/**.c", "src/**.cpp" })

		includedirs(assert(Config.libkinect.Include, "Missing kinect include dir"))
		includedirs(assert(Config.libobs.Include, "Missing obs include dir"))
		links("kinect20")
		links("obs")

		filter({ "action:vs*" })
			defines("_ENABLE_ATOMIC_ALIGNMENT_FIX")

		filter({ "platforms:x86" })
			architecture "x32"
			libdirs(assert(Config.libkinect.Lib32, "Missing kinect lib dir (x86)"))
			libdirs(assert(Config.libobs.Lib32, "Missing obs lib dir (x86)"))

		filter({ "platforms:x86_64" })
			architecture "x64"
			libdirs(assert(Config.libkinect.Lib64, "Missing kinect lib dir (x86_64)"))
			libdirs(assert(Config.libobs.Lib64, "Missing obs lib dir (x86_64)"))


		filter("configurations:Debug")
			defines({ "DEBUG" })
			symbols("On")
			postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToDebug })

		filter("configurations:Release")
			defines({ "NDEBUG" })
			optimize("On")
			postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToRelease })
