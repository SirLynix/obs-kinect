package("kinect-sdk1")
    set_homepage("https://www.microsoft.com/en-us/download/details.aspx?id=44561")
    set_description("The Kinect for Windows Software Development Kit (SDK) 2.0 enables developers to create applications that support gesture and voice recognition, using Kinect sensor technology.")

    on_fetch("windows", function (package)
        if package:config("shared") then
            os.raise("Kinect for Windows SDK is only available in static mode")
        end

        local sdk20dir = os.getenv("KINECTSDK20_DIR")
        if sdk20dir and os.isdir(sdk20dir) then
            local libFolder = package:is_arch("x86") and "x86" or "x64"

            local result = {}
            result.includedirs = { sdk20dir .. "/inc" }
            result.linkdirs = { sdk20dir .. "/lib/" .. libFolder }
            result.links  = { "Kinect20" }

            return result
        end
    end)

    on_install(function ()
        os.raise("Due to its license, the Kinect for Windows SDK 2 cannot be automatically installed, please download and install it yourself from https://www.microsoft.com/en-us/download/details.aspx?id=44561")
    end)

    on_test(function (package)
        assert(package:has_cfuncs("NuiInitialize", {includes = "NuiApi.h"}))
    end)
