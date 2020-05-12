Config = {}
setmetatable(Config, { __index = _G })

local configLoader, err = load(io.readfile("config.lua"), "config.lua", "t", Config)
if (not configLoader) then
	error("config.lua failed to load: " .. err)
end

local configLoaded, err = pcall(configLoader)
if (not configLoaded) then
	error("config.lua failed to load: " .. err)
end

local obsinclude = assert(Config.LibObs.Include, "Missing obs include dir")
local obslib32 = assert(Config.LibObs.Lib32, "Missing obs lib dir (x86)")
local obslib64 = assert(Config.LibObs.Lib64, "Missing obs lib dir (x86_64)")

local projects = {}

table.insert(projects, {
	Name = "obs-kinect",
	Defines = "OBS_KINECT_CORE_EXPORT",
	Files = { 
		"include/obs-kinect/**.hpp", 
		"include/obs-kinect/**.inl", 
		"src/obs-kinect/**.hpp", 
		"src/obs-kinect/**.inl", 
		"src/obs-kinect/**.cpp"
	},
	Include = {
		"include/obs-kinect",
		"src/obs-kinect",
		obsinclude
	},
	LibDir32 = obslib32,
	LibDir64 = obslib64,
	Links = "obs"
})

if (Config.KinectSdk10) then
	table.insert(projects, {
		Name = "obs-kinect-sdk10",
		Defines = {"WIN32", "_WINDOWS"},
		Files = { 
			"src/obs-kinect-sdk10/**.hpp", 
			"src/obs-kinect-sdk10/**.inl", 
			"src/obs-kinect-sdk10/**.cpp"
		},
		Include = {
			obsinclude,
			"include/obs-kinect",
			assert(Config.KinectSdk10.Include, "Missing KinectSdk10 include dir"),
			Config.KinectSdk10Toolkit and Config.KinectSdk10Toolkit.Include or nil
		},
		LibDir32 = {
			obslib32,
			assert(Config.KinectSdk10.Lib32, "Missing KinectSdk10 lib dir (x86)")
		},
		LibDir64 = {
			obslib64,
			assert(Config.KinectSdk10.Lib64, "Missing KinectSdk10 lib dir (x86_64)")
		},
		Links = {
			"obs-kinect",
			"obs",
			"Kinect10"
		}
	})
else
	print("Skipping KinectSdk10 backend")
end

if (Config.KinectSdk20) then
	table.insert(projects, {
		Name = "obs-kinect-sdk20",
		Files = { 
			"src/obs-kinect-sdk20/**.hpp", 
			"src/obs-kinect-sdk20/**.inl", 
			"src/obs-kinect-sdk20/**.cpp"
		},
		Include = {
			obsinclude,
			"include/obs-kinect",
			assert(Config.KinectSdk20.Include, "Missing KinectSdk20 include dir")
		},
		LibDir32 = {
			obslib32,
			assert(Config.KinectSdk20.Lib32, "Missing KinectSdk20 lib dir (x86)")
		},
		LibDir64 = {
			obslib64,
			assert(Config.KinectSdk20.Lib64, "Missing KinectSdk20 lib dir (x86_64)")
		},
		Links = {
			"obs-kinect",
			"obs",
			"kinect20"
		}
	})
else
	print("Skipping KinectSdk20 backend")
end

workspace("obs-kinect")
	configurations({ "Debug", "Release" })
	platforms({ "x86", "x86_64" })

	if (_ACTION) then
		location("build/" .. _ACTION)
	end

	-- Trigger premake before building, to automatically include new files (and premake changes)
	if (os.ishost("windows")) then
		local commandLine = "premake5.exe " .. table.concat(_ARGV, ' ')

		prebuildcommands("cd ../.. && " .. commandLine)
	end

	for _, proj in pairs(projects) do
		project(proj.Name)
			kind("SharedLib")
			language("C++")
			cppdialect("C++17")
			targetdir("bin/%{cfg.buildcfg}/%{cfg.architecture}")

			defines(proj.Defines)
			files(proj.Files)

			includedirs(proj.Include)
			links(proj.Links)

			filter("action:vs*")
				defines("_ENABLE_ATOMIC_ALIGNMENT_FIX")
				disablewarnings("4251") -- class needs to have dll-interface to be used by clients of class blah blah blah

			filter("platforms:x86")
				architecture "x32"
				libdirs(proj.LibDir32)

			filter("platforms:x86_64")
				architecture "x64"
				libdirs(proj.LibDir64)

			filter("configurations:Debug")
				defines("DEBUG")
				symbols("On")

			filter("configurations:Release")
				defines("NDEBUG")
				omitframepointer("On")
				optimize("Full")
				symbols("On") -- Generate symbols in release too (helps in case of crash)

			if (Config.CopyToDebug32) then
				filter("configurations:Debug", "architecture:x32")
					postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToDebug32 })
			end

			if (Config.CopyToDebug64) then
				filter("configurations:Debug", "architecture:x64")
					postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToDebug64 })
			end

			if (Config.CopyToRelease32) then
				filter("configurations:Release", "architecture:x32")
					postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToRelease32 })
			end

			if (Config.CopyToRelease64) then
				filter("configurations:Release", "architecture:x64")
					postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToRelease64 })
			end

	end
