includes(path.join(os.scriptdir(), "config.lua"))

local AzureKinectBodyTrackingSdk = AzureKinectBodyTrackingSdk
local AzureKinectSdk = AzureKinectSdk
local KinectSdk10 = KinectSdk10
local KinectSdk10Toolkit = KinectSdk10Toolkit
local KinectSdk20 = KinectSdk20
local ObsPlugins = ObsPlugins

rule("override_filename")
	on_load("windows", function (target)
		target:set("filename", target:name() .. ".dll")
	end)

	on_load("linux", function (target)
		target:set("filename", target:name() .. ".so")
	end)

	on_load("macosx", function (target)
		target:set("filename", target:name() .. ".dynlib")
	end)
rule_end()

rule("copy_to_obs")
	after_build(function(target)
		local folderKey = (is_mode("debug") and "Debug" or "Release") .. (is_arch("x86") and "32" or "64")
		local dir = ObsPlugins[folderKey]
		if dir and os.isdir(dir) then
			for _, path in ipairs({ target:targetfile(), target:symbolfile() }) do
				if os.isfile(path) then
					os.vcp(path, dir)
				end
			end
		end
	end)
rule_end()

rule("package_plugin")
	after_package(function(target)
		import("core.base.option")
   		local outputdir = option.get("outputdir") or config.buildir()

		local archDir
		if target:is_arch("x86_64", "x64") then
			archDir = "64bit"
		else
			archDir = "32bit"
		end

		for _, filepath in ipairs({ target:targetfile(), target:symbolfile() }) do
			if os.isfile(filepath) then
				os.vcp(filepath, path.join(outputdir, "obs-plugins", archDir, path.filename(filepath)))
			end
		end
	end)
rule_end()

add_repositories("local-repo xmake-repo")
add_requires("libfreenect2", { configs = { debug = is_mode("debug") } })

if not AzureKinectSdk then
	add_requires("k4a")
end 

add_requireconfs("libfreenect2", "libfreenect2.libusb", { configs = { pic = true }})

set_project("obs-kinect")
set_version("1.0")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

add_includedirs("include")
set_languages("c89", "cxx17")
set_license("GPL-3.0")
set_runtimes(is_mode("releasedbg") and "MD" or "MDd")
set_symbols("debug", "hidden")
set_targetdir("./bin/$(os)_$(arch)_$(mode)")
set_warnings("allextra")

add_sysincludedirs(LibObs.Include)

local baseObsDir = path.translate(is_arch("x86") and LibObs.Lib32 or LibObs.Lib64)
if is_plat("windows") then
	local dirSuffix = is_mode("debug") and "Debug" or "Release"
	add_linkdirs(path.join(baseObsDir, dirSuffix))
else
	add_linkdirs(baseObsDir)
end

add_links("obs")

if is_plat("windows") then
	add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
	add_cxxflags("/Zc:__cplusplus", "/Zc:referenceBinding", "/Zc:throwingNew")
	add_cxflags("/w44062") -- Enable warning: Switch case not handled warning
	add_cxflags("/wd4251") -- Disable warning: class needs to have dll-interface to be used by clients of class blah blah blah
elseif is_plat("linux") then
	add_syslinks("pthread")
end

-- Override default package function
on_package(function() end)

target("obs-kinect")
	set_kind("shared")
	set_group("Core")

	add_defines("OBS_KINECT_CORE_EXPORT")
	add_headerfiles("include/obs-kinect/**.hpp", "include/obs-kinect/**.inl")
	add_headerfiles("src/obs-kinect/**.hpp", "src/obs-kinect/**.inl")
	add_files("src/obs-kinect/**.cpp")

	add_includedirs("src")

	add_rules("override_filename", "copy_to_obs", "package_plugin")

	on_package(function (target)
		import("core.base.option")
   		local outputdir = option.get("outputdir") or config.buildir()

		os.vcp("data", path.join(outputdir, "data"))
	end)

target("obs-kinect-azuresdk")
	set_kind("shared")
	set_group("Azure")

	add_deps("obs-kinect")

	if AzureKinectSdk then
		add_sysincludedirs(AzureKinectSdk.Include)
		add_linkdirs(path.translate(is_arch("x86") and AzureKinectSdk.Lib32 or AzureKinectSdk.Lib64))
		add_links("k4a")
	else
		add_packages("k4a")
	end

	add_headerfiles("src/obs-kinect-azuresdk/**.hpp", "src/obs-kinect-azuresdk/**.inl")
	add_files("src/obs-kinect-azuresdk/**.cpp")

	add_rules("override_filename", "copy_to_obs", "package_plugin")

if KinectSdk10 then
	target("obs-kinect-sdk10")
		set_kind("shared")
		set_group("KinectV1")

		add_defines("UNICODE")
		add_deps("obs-kinect")

		add_sysincludedirs(KinectSdk10.Include)
		add_linkdirs(path.translate(is_arch("x86") and KinectSdk10.Lib32 or KinectSdk10.Lib64))
		add_links("Kinect10")

		if KinectSdk10Toolkit then
			add_sysincludedirs(KinectSdk10Toolkit.Include)
		end

		add_headerfiles("src/obs-kinect-sdk10/**.hpp", "src/obs-kinect-sdk10/**.inl")
		add_files("src/obs-kinect-sdk10/**.cpp")

		add_rules("override_filename", "copy_to_obs", "package_plugin")

		if KinectSdk10Toolkit then
			on_package(function (target)
				import("core.base.option")
				local outputdir = option.get("outputdir") or config.buildir()

				local archDir
				local archSuffix
				if target:is_arch("x86_64", "x64") then
					archDir = "64bit"
					archSuffix = "64"
				else
					archDir = "32bit"
					archSuffix = "32"
				end

				local filepath = KinectSdk10Toolkit.Bin .. "/KinectBackgroundRemoval180_" .. archSuffix .. ".dll"
				os.vcp(filepath, path.join(outputdir, "bin", archDir, path.filename(filepath)))
			end)
		end
end

if KinectSdk20 then
	target("obs-kinect-sdk20")
		set_kind("shared")
		set_group("KinectV2")

		add_defines("UNICODE")
		add_deps("obs-kinect")

		add_sysincludedirs(KinectSdk10.Include)
		add_linkdirs(path.translate(is_arch("x86") and KinectSdk10.Lib32 or KinectSdk10.Lib64))
		add_links("Kinect20")
		add_syslinks("Advapi32")

		add_headerfiles("src/obs-kinect-sdk20/**.hpp", "src/obs-kinect-sdk20/**.inl")
		add_files("src/obs-kinect-sdk20/**.cpp")

		add_sysincludedirs(path.relative(KinectSdk20.Include, "."))
		add_linkdirs(path.translate(is_arch("x86") and KinectSdk20.Lib32 or KinectSdk20.Lib64))

		add_rules("override_filename", "copy_to_obs", "package_plugin")
end

target("obs-kinect-freenect2")
	set_kind("shared")
	set_group("KinectV2")

	add_deps("obs-kinect")
	add_packages("libfreenect2")

	add_headerfiles("src/obs-kinect-freenect2/**.hpp", "src/obs-kinect-freenect2/**.inl")
	add_files("src/obs-kinect-freenect2/**.cpp")

	add_rules("override_filename", "copy_to_obs", "package_plugin")

--[[

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
	local project = {
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
		Links = {
			"obs-kinect",
			"obs",
			"Kinect10"
		}
	}

	if (Config.KinectSdk10.Lib32) then
		project.LibDir32 = {
			obslib32,
			Config.KinectSdk10.Lib32
		}
	else
		print("Missing KinectSdk10 lib dir (x86)")
	end

	if (Config.KinectSdk10.Lib64) then
		project.LibDir64 = {
			obslib64,
			Config.KinectSdk10.Lib64
		}
	else
		print("Missing KinectSdk10 lib dir (x86_64)")
	end

	table.insert(projects, project)
else
	print("Skipping KinectSdk10 backend")
end

if (Config.KinectSdk20) then
	local project = {
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
		Include64 = {
			"thirdparty/NuiSensorLib/include"
		},
		LibDir32 = {},
		LibDir64 = {
			"thirdparty/NuiSensorLib/lib/x64"
		},
		Links = {
			"obs-kinect",
			"obs",
			"kinect20"
		},
		Links64 = {
			"NuiSensorLib",
			"SetupAPI"
		}
	}

	if (Config.KinectSdk20.Lib32) then
		table.insert(project.LibDir32, obslib32)
		table.insert(project.LibDir32, Config.KinectSdk20.Lib32)
	else
		print("Missing KinectSdk20 lib dir (x86)")
	end

	if (Config.KinectSdk20.Lib64) then
		table.insert(project.LibDir64, obslib64)
		table.insert(project.LibDir64, Config.KinectSdk20.Lib64)
	else
		print("Missing KinectSdk20 lib dir (x86_64)")
	end

	table.insert(projects, project)
else
	print("Skipping KinectSdk20 backend")
end

if (Config.AzureKinectSdk) then
	local project = {
		Name = "obs-kinect-azuresdk",
		Files = { 
			"src/obs-kinect-azuresdk/**.hpp", 
			"src/obs-kinect-azuresdk/**.inl", 
			"src/obs-kinect-azuresdk/**.cpp"
		},
		Include = {
			obsinclude,
			"include/obs-kinect",
			assert(Config.AzureKinectSdk.Include, "Missing AzureKinectSdk include dir")
		},
		Links = {
			"obs-kinect",
			"obs",
			"k4a"
		}
	}

	if (Config.AzureKinectSdk.Lib32) then
		project.LibDir32 = {
			obslib32,
			Config.AzureKinectSdk.Lib32
		}
	else
		print("Missing AzureKinectSdk lib dir (x86)")
	end

	if (Config.AzureKinectSdk.Lib64) then
		project.LibDir64 = {
			obslib64,
			Config.AzureKinectSdk.Lib64
		}
	else
		print("Missing AzureKinectSdk lib dir (x86_64)")
	end

	if (Config.AzureKinectBodyTrackingSdk) then
		table.insert(project.Include, Config.AzureKinectBodyTrackingSdk.Include)
	else
		print("Warning: AzureKinectSdk will not have body tracking support")
	end

	table.insert(projects, project)
else
	print("Skipping AzureKinectSdk backend")
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

			if (proj.LibDir32 and proj.LibDir64) then
				platforms { "x86", "x86_64" }
			elseif (proj.LibDir64) then
				platforms "x86_64"
			else
				platforms "x86"
			end

			filter("system:Windows")
				defines("NOMINMAX")
				defines("WIN32_LEAN_AND_MEAN")

			filter("action:vs*")
				defines("_ENABLE_ATOMIC_ALIGNMENT_FIX")
				disablewarnings("4251") -- class needs to have dll-interface to be used by clients of class blah blah blah

			filter("platforms:x86")
				architecture "x32"
				includedirs(proj.Include32)
				links(proj.Links32)
				libdirs(proj.LibDir32)

			filter("platforms:x86_64")
				architecture "x64"
				includedirs(proj.Include64)
				links(proj.Links64)
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
				filter("configurations:Debug", "platforms:x86")
					postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToDebug32 })
			end

			if (Config.CopyToDebug64) then
				filter("configurations:Debug", "platforms:x86_64")
					postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToDebug64 })
			end

			if (Config.CopyToRelease32) then
				filter("configurations:Release", "platforms:x86")
					postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToRelease32 })
			end

			if (Config.CopyToRelease64) then
				filter("configurations:Release", "platforms:x86_64")
					postbuildcommands({ "{COPY} %{cfg.buildtarget.abspath} " .. Config.CopyToRelease64 })
			end

	end
]]
