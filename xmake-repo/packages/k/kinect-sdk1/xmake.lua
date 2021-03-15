package("kinect-sdk1")
    set_homepage("https://www.microsoft.com/en-us/download/details.aspx?id=40278")
    set_description("The Kinect for Windows Software Development Kit (SDK) enables developers to create applications that support gesture and voice recognition, using Kinect sensor technology.")

    on_fetch("windows", function (package)
        if package:config("shared") then
            os.raise("Kinect for Windows SDK is only available in static mode")
        end

        local sdk10dir = os.getenv("KINECTSDK10_DIR")
        if sdk10dir and os.isdir(sdk10dir) then
            local libFolder = package:is_arch("x86") and "x86" or "amd64"

            local result = {}
            result.includedirs = { sdk10dir .. "/inc" }
            result.linkdirs = { sdk10dir .. "/lib/" .. libFolder }
            result.links  = { "Kinect10" }

            return result
        end
    end)

    on_install(function ()
        os.raise("Due to its license, the Kinect for Windows SDK cannot be automatically installed, please download and install it yourself from https://www.microsoft.com/en-us/download/details.aspx?id=40278")
    end)

    on_test(function (package)
        assert(package:has_cfuncs("NuiInitialize", {includes = "NuiApi.h"}))
    end)
