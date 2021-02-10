package("libfreenect2")
    set_homepage("https://github.com/OpenKinect/libfreenect2")
    set_description("Open source drivers for the Kinect for Windows v2 device")
    set_license("GPL-2.0")

    set_urls("https://github.com/OpenKinect/libfreenect2.git")

    add_deps("cmake", "libjpeg-turbo")

    if (is_plat("windows")) then
        -- base libusb doesn't work with libfreenect2, force it as a .dll to replace it with a custom version
        add_deps("libusb", { configs = {shared = true}})
    else
        add_deps("libusb")
    end

    on_install("windows", "linux", "macosx", function (package)
        local libjpegturbo = package:dep("libjpeg-turbo")
        local libusb = package:dep("libusb")

        if (libusb:fetch()) then
            io.replace("CMakeLists.txt", "FIND_PACKAGE(LibUSB REQUIRED)", "", {plain = true})
        end

        local configs = {}
        table.insert(configs, "-DBUILD_EXAMPLES=OFF")
        table.insert(configs, "-DENABLE_CXX11=ON")
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        table.insert(configs, "-DTurboJPEG_INCLUDE_DIRS=" .. libjpegturbo:installdir("include"))
        table.insert(configs, "-DTurboJPEG_LIBRARIES=" .. table.concat(libjpegturbo:fetch().libfiles, ";"))
        table.insert(configs, "-DLibUSB_INCLUDE_DIRS=" .. libusb:installdir("include"))
        table.insert(configs, "-DLibUSB_LIBRARIES=" .. table.concat(libusb:fetch().libfiles, ";"))
        import("package.tools.cmake").install(package, configs, {packagedeps="libusb"})
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            void test() {
                libfreenect2::Freenect2 fn2;
                fn2.enumerateDevices();
            }
        ]]}, {configs = {languages = "c++11"}, includes = "libfreenect2/libfreenect2.hpp"}))
    end)
