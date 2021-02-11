package("libjpeg-turbo")
    set_homepage("https://libjpeg-turbo.org/")
    set_description("libjpeg-turbo is a JPEG image codec that uses SIMD instructions to accelerate baseline JPEG compression and decompression on x86, x86-64, Arm, PowerPC, and MIPS systems, as well as progressive JPEG compression on x86, x86-64, and Arm systems.")
    set_license("zlib")

    set_urls("https://github.com/libjpeg-turbo/libjpeg-turbo/archive/$(version).tar.gz",
             "https://github.com/libjpeg-turbo/libjpeg-turbo.git")
    add_versions("2.0.90", "6a965adb02ad898b2ae48214244618fe342baea79db97157fdc70d8844ac6f09")

    add_deps("cmake")

    on_install("windows", "linux", "macosx", function (package)
        local cxflags
        if package:config("pic") then
            cxflags = {"-fPIC"}
        end

        local configs = {}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        if package:config("shared") then
            table.insert(configs, "-DENABLE_SHARED=ON")
            table.insert(configs, "-DENABLE_STATIC=OFF")
        else
            table.insert(configs, "-DENABLE_SHARED=OFF")
            table.insert(configs, "-DENABLE_STATIC=ON")
        end
        if package:config("vs_runtime"):startswith("MD") then
            table.insert(configs, "-DWITH_CRT_DLL=ON")
        end
        import("package.tools.cmake").install(package, configs, {cxflags=cxflags})
    end)

    on_test(function (package)
        assert(package:has_cfuncs("tjInitCompress", {includes = "turbojpeg.h"}))
    end)
