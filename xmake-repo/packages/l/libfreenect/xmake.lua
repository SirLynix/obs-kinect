package("libfreenect")
    set_homepage("https://github.com/OpenKinect/libfreenect")
    set_description("Drivers and libraries for the Xbox Kinect device on Windows, Linux, and OS X")
    set_license("GPL-2.0")

    set_urls("https://github.com/OpenKinect/libfreenect/archive/refs/tags/$(version).tar.gz",
             "https://github.com/OpenKinect/libfreenect.git")
    add_versions("v0.6.2", "e135f5e60ae290bf1aa403556211f0a62856a9e34f12f12400ec593620a36bfa")

    add_urls("https://github.com/OpenKinect/libfreenect.git")

    add_versions("v0.2.1", "fd64c5d9b214df6f6a55b4419357e51083f15d93")
    add_versions("v0.2.0", "v0.2.0")

    add_deps("cmake >=3.12.4")
    if is_plat("windows") then 
        add_deps("libusb >=1.0.22")
    else
        add_deps("libusb >=1.0.18")
    end

    on_install("windows", "linux", "macosx", function (package)
        io.replace("CMakeLists.txt", "find_package(libusb-1.0 REQUIRED)", "", {plain = true})

        local configs = {}
        table.insert(configs, "-DBUILD_C_SYNC=OFF")
        table.insert(configs, "-DBUILD_EXAMPLES=OFF")
        table.insert(configs, "-DBUILD_FAKENECT=OFF")
        table.insert(configs, "-DBUILD_PYTHON=OFF")
        table.insert(configs, "-DBUILD_REDIST_PACKAGE=ON") -- prevents executing fwfetcher.py
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))

        if package:config("pic") ~= false then
            table.insert(configs, "-DCMAKE_POSITION_INDEPENDENT_CODE=ON")
        end

        import("package.tools.cmake").install(package, configs, {packagedeps = "libusb"})
    end)

    on_test(function (package)
        assert(package:has_cfuncs("freenect_init", {includes = "libfreenect/libfreenect.h"}))
    end)
