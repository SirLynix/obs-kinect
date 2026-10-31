includes("@builtin/xpack")

includes(path.join(os.scriptdir(), "config.lua"))

local ObsFolder = ObsFolder or {}

rule("kinect_dynlib", function ()
	after_load(function (target)
		target:add("rpathdirs", ".")
		target:add("rpathdirs", "@executable_path")
	end)

	on_load("windows", function (target)
		target:set("filename", target:name() .. ".dll")
	end)

	on_load("linux", function (target)
		target:set("filename", target:name() .. ".so")
	end)

	on_load("macosx", function (target)
		target:set("filename", target:name() .. ".dylib")
	end)
end)

rule("copy_to_obs", function ()
	after_build(function(target)
		local folderKey = (is_mode("debug") and "Debug" or "Release") .. (is_arch("x86") and "32" or "64")
		local obsDir = ObsFolder[folderKey]
		if not obsDir then
			return
		end

		local outputFolder
		if target:name() == "obs-kinectcore" then 
			outputFolder = "bin"
		else
			outputFolder = "obs-plugins"
		end

		local archDir
		if target:is_arch("x86_64", "x64") then
			archDir = "64bit"
		else
			archDir = "32bit"
		end

   		local outputdir = path.join(obsDir, outputFolder, archDir)
		if outputdir and os.isdir(outputdir) then
			for _, path in ipairs({ target:targetfile(), target:symbolfile() }) do
				if os.isfile(path) then
					os.vcp(path, outputdir)
				end
			end
		end
	end)
end)

rule("package_binaries", function ()
	after_package(function (target)
		import("core.base.option")

		local outputdir = path.join(option.get("outputdir") or config.buildir(), "obs-studio", "plugins", "obs-kinect")
		for _, filepath in ipairs({ target:targetfile(), target:symbolfile() }) do
			if os.isfile(filepath) then
				os.vcp(filepath, path.join(outputdir, path.filename(filepath)))
			end
		end
	end)
end)

add_repositories("local-repo xmake-repo")
add_requires("libfreenect", "libfreenect2", { configs = { debug = is_mode("debug") } })
add_requires("k4a", "libusb")
add_requires("k4abt-headers", { optional = true })

add_requireconfs("libusb", "*.libusb", { configs = { pic = true, shared = is_plat("windows") }})
add_requireconfs("libfreenect2", "libfreenect2.libjpeg-turbo", { configs = { shared = not is_plat("windows") }})

if is_plat("windows") then
	add_requires("kinect-sdk1", "kinect-sdk2", { optional = true })
	add_requires("kinect-sdk1-toolkit", { optional = true, configs = { background_removal = true, facetrack = false, fusion = false, interaction = false, shared = true }})
end

set_project("obs-kinect")
set_version("1.0.0")

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

set_policy("install.strip_packagelibs", false)

local baseObsDir = path.translate(is_arch("x86") and LibObs.Build32 or LibObs.Build64)
if is_plat("windows") then
	for _, suffix in pairs(is_mode("debug") and {"Debug"} or {"Release", "RelWithDebInfo"}) do
		local p = path.join(baseObsDir, "libobs", suffix)
		if os.isdir(p) then
			add_sysincludedirs(path.join(baseObsDir, "config"))
			add_linkdirs(p)
			break
		end
	end
else
	add_sysincludedirs(path.join(baseObsDir, "config"))
	add_linkdirs(path.join(baseObsDir, "libobs"))
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

target("obs-kinectcore", function ()
	set_kind("shared")
	set_group("Core")

	add_defines("OBS_KINECT_CORE_EXPORT")

	add_headerfiles("include/obs-kinect-core/**.hpp", "include/obs-kinect-core/**.inl")
	add_headerfiles("src/obs-kinect-core/**.hpp", "src/obs-kinect-core/**.inl")
	add_files("src/obs-kinect-core/**.cpp")

	add_includedirs("src")

	add_rules("copy_to_obs", "package_binaries")
end)

target("obs-kinect", function ()
	set_kind("shared")
	set_group("Core")

	add_deps("obs-kinectcore")

	add_headerfiles("src/obs-kinect/**.hpp", "src/obs-kinect/**.inl")
	add_files("src/obs-kinect/**.cpp")

	add_includedirs("src")

	add_rules("kinect_dynlib", "copy_to_obs", "package_binaries")

	on_package(function (target)
		import("core.base.option")
   		local outputdir = option.get("outputdir") or config.buildir()

		os.vcp("data", path.join(outputdir, "obs-studio", "plugins", "obs-kinect", "data"))
	end)
end)

target("obs-kinect-azuresdk", function()
	set_kind("shared")
	set_group("Azure")

	add_deps("obs-kinectcore")

	add_packages("k4a")

	add_headerfiles("src/obs-kinect-azuresdk/**.hpp", "src/obs-kinect-azuresdk/**.inl")
	add_files("src/obs-kinect-azuresdk/**.cpp")

	add_rules("kinect_dynlib", "copy_to_obs", "package_binaries")

	if has_package("k4abt-headers") then
		add_packages("k4abt-headers")
	end
end)

if is_plat("windows") then

	if has_package("kinect-sdk1") then
		target("obs-kinect-sdk10", function ()
			set_kind("shared")
			set_group("KinectV1")

			add_defines("UNICODE")
			add_deps("obs-kinectcore")
			add_packages("kinect-sdk1")

			if has_package("kinect-sdk1-toolkit") then
				add_packages("kinect-sdk1-toolkit", { links = {} })
			end

			add_headerfiles("src/obs-kinect-sdk10/**.hpp", "src/obs-kinect-sdk10/**.inl")
			add_files("src/obs-kinect-sdk10/**.cpp")

			add_rules("kinect_dynlib", "copy_to_obs", "package_binaries")
		end)
	end

	if has_package("kinect-sdk2") then
		target("obs-kinect-sdk20", function ()
			set_kind("shared")
			set_group("KinectV2")

			add_defines("UNICODE")
			add_deps("obs-kinectcore")
			add_packages("kinect-sdk2")
			add_syslinks("Advapi32")

			add_headerfiles("src/obs-kinect-sdk20/**.hpp", "src/obs-kinect-sdk20/**.inl")
			add_files("src/obs-kinect-sdk20/**.cpp")

			add_rules("kinect_dynlib", "copy_to_obs", "package_binaries")

			if not is_arch("x86") and os.isdir("thirdparty/NuiSensorLib") then
				add_sysincludedirs("thirdparty/NuiSensorLib/include")
				add_linkdirs("thirdparty/NuiSensorLib/lib/x64")
				add_links("NuiSensorLib")
				add_syslinks("SetupAPI")
			end
		end)
	end

end

target("obs-kinect-freenect", function ()
	set_kind("shared")
	set_group("KinectV1")

	add_deps("obs-kinectcore")
	add_packages("libfreenect", "libusb")

	add_headerfiles("src/obs-kinect-freenect/**.hpp", "src/obs-kinect-freenect/**.inl")
	add_files("src/obs-kinect-freenect/**.cpp")

	add_rules("kinect_dynlib", "copy_to_obs", "package_binaries")
end)

target("obs-kinect-freenect2", function ()
	set_kind("shared")
	set_group("KinectV2")

	add_deps("obs-kinectcore")
	add_packages("libfreenect2")

	add_headerfiles("src/obs-kinect-freenect2/**.hpp", "src/obs-kinect-freenect2/**.inl")
	add_files("src/obs-kinect-freenect2/**.cpp")

	add_rules("kinect_dynlib", "copy_to_obs", "package_binaries")
end)

local function installfiles(package, target_utils)
	local binfiles = {}
	for _, target in ipairs(package:targets()) do
		table.insert(binfiles, target:targetfile())
		if os.isfile(target:symbolfile()) then
			table.insert(binfiles, target:symbolfile())
		end

		target_utils.get_target_libfiles(target, binfiles, target:targetfile(), {})
	end
	binfiles = table.unique(binfiles)

	local installfiles = {}
	for _, file in ipairs(binfiles) do
		table.insert(installfiles, {source = file, dest = path.filename(file)})
	end

	for _,filepath in pairs((package:sourcefiles())) do
		table.insert(installfiles, {source = filepath, dest = filepath})
	end

	return installfiles
end

xpack("obs-kinect", function ()
	set_formats("nsis", "zip", "targz")
	set_homepage("https://github.com/SirLynix/obs-kinect")
	set_title("obs-kinect plugin")
	set_description("A plugin for OBS-Studio allowing you to use your Kinect as a camera source.")
	set_author("SirLynix")
	set_maintainer("SirLynix")
	set_iconfile("obs-kinect.ico")
	if set_displayicon then
		set_displayicon("obs-kinect.ico")
	else
		set_nsis_displayicon("obs-kinect.ico")
	end

	set_license("GPL")
	set_licensefile("LICENSE")

	add_targets("obs-kinect", "obs-kinect-azuresdk", "obs-kinect-freenect", "obs-kinect-freenect2")

	if is_plat("windows") then
		if has_package("kinect-sdk1") then
			add_targets("obs-kinect-sdk10")
		end
		
		if has_package("kinect-sdk2") then
			add_targets("obs-kinect-sdk20")
		end
	end

	add_sourcefiles("(data/**)")

	on_load(function (package)
		if package:format() == "nsis" then
			package:set("specfile", "installer.nsi")
		end
	end)

	on_installcmd(function (package, batchcmds)
		import("private.utils.target", {alias = "target_utils"})
		local installfiles = installfiles(package, target_utils)

		local installdir = package:installdir()

		batchcmds:mkdir(installdir)
		batchcmds:mkdir(path.join(installdir, "data"))
		for _, file in ipairs(installfiles) do
			batchcmds:cp(file.source, path.join(installdir, file.dest))
		end
	end)

    on_uninstallcmd(function (package, batchcmds)
		import("private.utils.target", {alias = "target_utils"})
		local installfiles = installfiles(package, target_utils)

		local installdir = package:installdir()

		for _, file in ipairs(installfiles) do
			batchcmds:rm(path.join(installdir, file.dest), {emptydirs = true})
		end
    end)
end)
