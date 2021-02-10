package("k4a")
    set_homepage("https://Azure.com/Kinect")
    set_description(" A cross platform (Linux and Windows) user mode SDK to read data from your Azure Kinect device.")
    set_license("MIT")

    set_urls("https://github.com/microsoft/Azure-Kinect-Sensor-SDK.git")
    add_versions("v1.4.1", "73106554449c64aff6b068078f0eada50c4474e99945b5ceb6ea4aab9a68457f")

    add_deps("cmake", "libusb")

    on_install("windows", "linux", "macosx", function (package)
        -- There's no option to disable examples, tests or tool building
        io.replace("CMakeLists.txt", "add_subdirectory(examples)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(tests)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(tools)", "", {plain = true})

        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        import("package.tools.cmake").install(package, configs, {packagedeps={"libsoundio", "libusb"}})
    end)

    on_test(function (package)
        assert(package:has_cfuncs("k4a_device_get_installed_count", {includes = "k4a/k4a.h"}))
    end)
