package("k4a")
    set_homepage("https://Azure.com/Kinect")
    set_description("A cross platform (Linux and Windows) user mode SDK to read data from your Azure Kinect device.")
    set_license("MIT")

    set_urls("https://github.com/microsoft/Azure-Kinect-Sensor-SDK.git")
    add_versions("v1.4.1", "73106554449c64aff6b068078f0eada50c4474e99945b5ceb6ea4aab9a68457f")

    add_deps("cmake")

    if is_plat("linux", "macosx") then
        add_deps("libx11")
    end

    on_fetch("windows", function (package)
        -- KINECTSDKAZURE_DIR is not an official Microsoft env
        local defaultInstallPath = os.getenv("KINECTSDKAZURE_DIR") or path.join(os.getenv("ProgramFiles"), "Azure Kinect SDK v1.4.1", "sdk")
        if os.isdir(defaultInstallPath) then
            local archFolder = package:is_arch("x86") and "x86" or "amd64"

            local platformFolder = path.join(defaultInstallPath, "windows-desktop", archFolder, "release")

            local result = {}
            result.includedirs = { defaultInstallPath .. "/include" }
            result.linkdirs = { platformFolder .. "/lib/" }

            result.links = { "k4a" }
            result.libfiles = {
                platformFolder .. "/bin/depthengine_2_0.dll",
                platformFolder .. "/bin/k4a.dll"
            }

            return result
        end
    end)

    on_install("windows", "linux", "macosx", function (package)
        -- There's no option to disable examples, tests or tool building
        io.replace("CMakeLists.txt", "add_subdirectory(examples)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(tests)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(tools)", "", {plain = true})

        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        import("package.tools.cmake").install(package, configs)
    end)

    on_test(function (package)
        assert(package:has_cfuncs("k4a_device_get_installed_count", {includes = "k4a/k4a.h"}))
    end)
