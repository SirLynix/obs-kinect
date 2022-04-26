package("k4abt-headers")
    set_kind("library", {headeronly = true})
    set_homepage("https://Azure.com/Kinect")
    set_description("Headers of the Azure Kinect Body Tracking SDK")
    set_license("MIT")

    add_versions("v1.4.1", "")

    on_fetch("windows", function (package)
        -- KINECTSDKAZUREBT_DIR is not an official Microsoft env
        local defaultInstallPath = os.getenv("KINECTSDKAZUREBT_DIR") or path.join(os.getenv("ProgramFiles"), "Azure Kinect Body Tracking SDK", "sdk")
        if os.isdir(defaultInstallPath) then
            local result = {}
            result.includedirs = { defaultInstallPath .. "/include" }

            return result
        end
    end)
