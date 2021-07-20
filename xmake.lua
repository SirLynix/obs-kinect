includes(path.join(os.scriptdir(), "config.lua"))

local ObsFolder = ObsFolder or {}

rule("kinect_dynlib")
	after_load(function (target)
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
rule_end()

rule("copy_to_obs")
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
rule_end()

local function packageGen(outputFolder)
	return function (target)
		import("core.base.option")

		local archDir
		if target:is_arch("x86_64", "x64") then
			archDir = "64bit"
		else
			archDir = "32bit"
		end

   		local outputdir = path.join(option.get("outputdir") or config.buildir(), outputFolder, archDir)
		for _, filepath in ipairs({ target:targetfile(), target:symbolfile() }) do
			if os.isfile(filepath) then
				os.vcp(filepath, path.join(outputdir, path.filename(filepath)))
			end
		end
	end
end

rule("package_bin")
	after_package(packageGen("bin"))
rule_end()

rule("package_plugin")
	after_package(packageGen("obs-plugins"))
rule_end()

rule("package_deps")
	local installed_packages = {}

	after_package("windows", function (target)
		import("core.base.option")

		local archDir
		if target:is_arch("x86_64", "x64") then
			archDir = "64bit"
		else
			archDir = "32bit"
		end

		local outputdir = path.join(option.get("outputdir") or config.buildir(), "bin", archDir)

		for _, pkg in ipairs(target:orderpkgs()) do
			if not installed_packages[pkg:name()] then
				if pkg:enabled() and pkg:get("libfiles") then
					for _, dllpath in ipairs(table.wrap(pkg:get("libfiles"))) do
						if dllpath:endswith(".dll") then
							os.vcp(dllpath, outputdir)
						end
					end
				end
				installed_packages[pkg:name()] = true
			end
		end
	end)
rule_end()

add_repositories("local-repo xmake-repo")
add_requires("libfreenect", "libfreenect2", { configs = { debug = is_mode("debug") } })
add_requires("k4a")

add_requireconfs("*.libusb", { configs = { pic = true, shared = is_plat("windows") }})

if is_plat("windows") then
	add_requires("kinect-sdk1", "kinect-sdk2", { optional = true })
	add_requires("kinect-sdk1-toolkit", { optional = true, configs = { background_removal = true, facetrack = false, fusion = false, interaction = false, shared = true }})
end

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
	for _, suffix in pairs(is_mode("debug") and {"Debug"} or {"Release", "RelWithDebInfo"}) do
		local p = path.join(baseObsDir, suffix)
		if (os.isdir(p)) then
			add_linkdirs(p)
			break
		end
	end
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

target("obs-kinectcore")
	set_kind("shared")
	set_group("Core")

	add_defines("OBS_KINECT_CORE_EXPORT")

	add_headerfiles("include/obs-kinect-core/**.hpp", "include/obs-kinect-core/**.inl")
	add_headerfiles("src/obs-kinect-core/**.hpp", "src/obs-kinect-core/**.inl")
	add_files("src/obs-kinect-core/**.cpp")

	add_includedirs("src")

	add_rules("copy_to_obs", "package_plugin")

target("obs-kinect")
	set_kind("shared")
	set_group("Core")

	add_deps("obs-kinectcore")

	add_headerfiles("src/obs-kinect/**.hpp", "src/obs-kinect/**.inl")
	add_files("src/obs-kinect/**.cpp")

	add_includedirs("src")

	add_rules("kinect_dynlib", "copy_to_obs", "package_plugin")

	on_package(function (target)
		import("core.base.option")
   		local outputdir = option.get("outputdir") or config.buildir()

		os.vcp("data", path.join(outputdir, "data"))
	end)

target("obs-kinect-azuresdk")
	set_kind("shared")
	set_group("Azure")

	add_deps("obs-kinectcore")

	add_packages("k4a")

	add_headerfiles("src/obs-kinect-azuresdk/**.hpp", "src/obs-kinect-azuresdk/**.inl")
	add_files("src/obs-kinect-azuresdk/**.cpp")

	add_rules("kinect_dynlib", "copy_to_obs", "package_bin", "package_deps")

if is_plat("windows") then

	if has_package("kinect-sdk1") then
		target("obs-kinect-sdk10")
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

			add_rules("kinect_dynlib", "copy_to_obs", "package_bin", "package_deps")
	end

	if has_package("kinect-sdk2") then
		target("obs-kinect-sdk20")
			set_kind("shared")
			set_group("KinectV2")

			add_defines("UNICODE")
			add_deps("obs-kinectcore")
			add_packages("kinect-sdk2")
			add_syslinks("Advapi32")

			add_headerfiles("src/obs-kinect-sdk20/**.hpp", "src/obs-kinect-sdk20/**.inl")
			add_files("src/obs-kinect-sdk20/**.cpp")

			add_rules("kinect_dynlib", "copy_to_obs", "package_bin", "package_deps")

			if not is_arch("x86") and os.isdir("thirdparty/NuiSensorLib") then
				add_sysincludedirs("thirdparty/NuiSensorLib/include")
				add_linkdirs("thirdparty/NuiSensorLib/lib/x64")
				add_links("NuiSensorLib")
				add_syslinks("SetupAPI")
			end
	end

end

target("obs-kinect-freenect")
	set_kind("shared")
	set_group("KinectV1")

	add_deps("obs-kinectcore")
	add_packages("libfreenect")

	add_headerfiles("src/obs-kinect-freenect/**.hpp", "src/obs-kinect-freenect/**.inl")
	add_files("src/obs-kinect-freenect/**.cpp")

	add_rules("kinect_dynlib", "copy_to_obs", "package_bin", "package_deps")

target("obs-kinect-freenect2")
	set_kind("shared")
	set_group("KinectV2")

	add_deps("obs-kinectcore")
	add_packages("libfreenect2")

	add_headerfiles("src/obs-kinect-freenect2/**.hpp", "src/obs-kinect-freenect2/**.inl")
	add_files("src/obs-kinect-freenect2/**.cpp")

	add_rules("kinect_dynlib", "copy_to_obs", "package_bin", "package_deps")
