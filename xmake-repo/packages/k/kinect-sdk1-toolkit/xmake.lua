package("kinect-sdk1-toolkit")
    set_homepage("https://www.microsoft.com/en-us/download/details.aspx?id=40276")
    set_description("The Kinect for Windows Developer Toolkit contains updated and new source code samples, Kinect Fusion, Kinect Interactions, Kinect Studio, and other resources to simplify developing Kinect for Windows applications.")

    add_configs("background_removal", { description = "Enable background removal support.",  default = true, type = "boolean"})
    add_configs("facetrack",          { description = "Enables facetracking support.",  default = true, type = "boolean"})
    add_configs("fusion",             { description = "Enable fusion support.",  default = true, type = "boolean"})
    add_configs("interaction",        { description = "Enable interaction support.",  default = true, type = "boolean"})

    on_fetch("windows", function (package, opt)
        if not package:config("shared") then
            os.raise("Kinect for Windows Developer Toolkit is only available in shared mode")
        end

        local toolkitDir = os.getenv("KINECT_TOOLKIT_DIR")
        if toolkitDir and os.isdir(toolkitDir) then
            local libFolder = package:is_arch("x86") and "x86" or "amd64"
            local libSuffix = package:is_arch("x86") and "_32" or "_64"

            local result = {}
            result.includedirs = { toolkitDir .. "/inc" }
            result.linkdirs = { toolkitDir .. "/lib/" .. libFolder }
            result.links = {}
            result.libfiles = {}

            local function AddLib(name)
                table.insert(result.libfiles, toolkitDir .. "/bin/" .. name .. ".dll")
                table.insert(result.links, name)
            end

            if package:config("background_removal") then
                AddLib("KinectBackgroundRemoval180" .. libSuffix)
            end
            if package:config("facetrack") then
                AddLib("FaceTrackLib")
            end
            if package:config("fusion") then
                AddLib("KinectFusion180" .. libSuffix)
            end
            if package:config("interaction") then
                AddLib("KinectInteraction180" .. libSuffix)
            end

            return result
        end
    end)

    on_install(function ()
        os.raise("Due to its license, the Kinect for Windows Developer Toolkit cannot be automatically installed, please download and install it yourself from https://www.microsoft.com/en-us/download/details.aspx?id=40276")
    end)

    on_test(function (package)
        assert(package:has_cfuncs("NuiCreateBackgroundRemovedColorStream", {includes = "KinectBackgroundRemoval.h"}))
    end)
