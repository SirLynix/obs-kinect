package("k4a")
    set_homepage("https://Azure.com/Kinect")
    set_description("A cross platform (Linux and Windows) user mode SDK to read data from your Azure Kinect device.")
    set_license("MIT")

    set_policy("platform.longpaths", true)

    set_urls("https://github.com/microsoft/Azure-Kinect-Sensor-SDK.git")
    add_versions("v1.4.2", "cd2b3e9894b26fbeebfe34a5cab5b26efab0939a")
    add_versions("v1.4.1", "3cfe362dacc4ad9b2206af3f28d62e1eb7be99c2")

    add_deps("cmake", "libjpeg-turbo", "libusb")

    if is_plat("linux", "macosx") then
        add_deps("libx11", "libxrandr", "libxrender", "libxinerama", "libxfixes", "libxcursor", "libxi", "libxext")
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
        io.replace("CMakeLists.txt", "cmake_minimum_required(VERSION 3.9.0)", "cmake_minimum_required(VERSION 3.22)", {plain = true})
        -- There's no option to disable examples, tests or tool building
        io.replace("CMakeLists.txt", "add_subdirectory(examples)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(tests)", "", {plain = true})
        io.replace("CMakeLists.txt", "add_subdirectory(tools)", "", {plain = true})

        io.replace("extern/CMakeLists.txt", "add_subdirectory(glfw)", "", {plain = true})
        io.replace("extern/CMakeLists.txt", "add_subdirectory(googletest)", "", {plain = true})
        io.replace("extern/CMakeLists.txt", "add_subdirectory(imgui)", "", {plain = true})
        io.replace("extern/CMakeLists.txt", "add_subdirectory(libusb)", "", {plain = true})

        io.replace("src/usbcommand/CMakeLists.txt", "LibUSB::LibUSB", "", {plain = true})

        io.replace("src/color/mfcamerareader.h", "CMFCameraReader::", "", {plain = true})

        io.replace("extern/spdlog/src/include/spdlog/fmt/bundled/format.h", "#if defined _SECURE_SCL && _SECURE_SCL", "#if defined _SECURE_SCL && _SECURE_SCL && (!defined(_MSC_VER) || _MSC_VER < 1915)", {plain = true})
        io.replace("extern/azure_c_shared/src/src/hmac.c", "int hmacResult(HMACContext *ctx, uint8_t *digest)", "int hmacResult(HMACContext *ctx, uint8_t digest[USHAMaxHashSize])", {plain = true})

        io.replace("cmake/k4aCompilerFlags.cmake", "-Werror", "", {plain = true})
        io.replace("cmake/k4aCompilerFlags.cmake", "/WX", "", {plain = true})

        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        table.insert(configs, "-DK4A_VALIDATE_CLANG_FORMAT=OFF")

        local opt = {}
        opt.packagedeps = {"libusb"}
        import("package.tools.cmake").install(package, configs, opt)
    end)

    on_test(function (package)
        assert(package:has_cfuncs("k4a_device_get_installed_count", {includes = "k4a/k4a.h"}))
    end)
