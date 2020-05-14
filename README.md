# obs-kinect

OBS Plugin to access Kinect data (and setup a virtual green screen based on depth).

# Features

This plugins allows you to access a Kinect v1 (for Xbox 360) and Kinect v2 (for Xbox One) streams and setup a "virtual green screen effect" based on depth/body data.

![Example of a virtual Green screen made by obs-kinect](https://files.digitalpulsesoftware.net/obs-kinect.png)

[Example video](https://www.youtube.com/watch?v=4QWad1WISOY):  
[![Demo video](https://img.youtube.com/vi/4QWad1WISOY/0.jpg)](https://www.youtube.com/watch?v=4QWad1WISOY)

# Requirement

- Windows. For now this plugin is only compatible with Windows as it uses the official Kinect for Windows API (may change in the future).
- A Kinect (obviously), v1 (xbox 360) and v2 (xbox one) are supported (but v2 has better quality), they're *relatively* cheap on eBay (between 50€ and 100€).
- If your Kinect isn't pluggable to your computer: a Kinect to USB adapter (search for PeakLead Kinect to USB on Amazon).
- Not running on a potato computer, Kinect itself requires a little bit of CPU power, especially when using the faux green screen effect (I'm trying to improve that) because of the color-to-depth mapping (which is done on the CPU). The plugin itself runs on the GPU.
- ⚠️ OBS Studio >= 25.0 (since 0.3 this plugins no longer works on OBS Studio 24.0, if upgrading is a problem let me know).
- ⚠️ **Kinect for Windows runtime** (links in "How to use", note that Kinect for Windows SDK includes runtime)
- ⚠️ **Visual Studio 2019 redistribuables** ([32bits](https://aka.ms/vs/16/release/vc_redist.x86.exe), [64bits](https://aka.ms/vs/16/release/vc_redist.x64.exe))

# To do

- ~~Improve green-screen filtering using gaussian blur~~
- Optimize green-screen filtering effect (especially the color-to-depth mapping part, if possible)
- ~~Add support for Kinect v1~~
- Add support for Linux and macOS (using [libfreenect](https://github.com/OpenKinect/libfreenect) and [libfreenect2](https://github.com/OpenKinect/libfreenect2))
- ~~Use shaders to do faux green-screen processing. 1080p pixel processing is a really heavy CPU task and would benefit a lot to run on the GPU~~
- Add more fun effects (use issues to post your ideas)
- ~~Add possibility to use body index masking (pixels identified by Kinect SDK to be you)~~
- Support audio?

# How to use (for end-users, you probably want this)

**If you want to have support for the Kinect v1 (xbox 360 version)**
- Download and install [**Kinect for Windows runtime v1.8**](https://www.microsoft.com/en-us/download/details.aspx?id=40277)

**If you want to have support for the Kinect v2 (xbox one version)**
- Download and install [**Kinect for Windows runtime v2.2**](https://www.microsoft.com/en-us/download/details.aspx?id=100067)

You can/must install both if you want to support both Kinect versions.

Download the [latest release](https://github.com/SirLynix/obs-kinect/releases) and copy the files in your OBS folder, restart OBS and you should have a "Kinect source" available

⚠️ Don't forget to install the Visual Studio 2019 redistribuables ([32bits](https://aka.ms/vs/16/release/vc_redist.x86.exe), [64bits](https://aka.ms/vs/16/release/vc_redist.x64.exe)).

# How to build (for people wanting to contribute)

Clone and build OBS-studio first.

**If you want to have support for the Kinect v1 (xbox 360 version)**
- Download and install [**Kinect for Windows SDK 1.8**](https://www.microsoft.com/en-us/download/details.aspx?id=40278)
- (Optional) Download and install [**Kinect for Windows Developer Toolkit v1.8**](https://www.microsoft.com/en-us/download/details.aspx?id=40276), this is required for dedicated background support!

**If you want to have support for the Kinect v2 (xbox one version)**
- Download and install [**Kinect for Windows SDK v2.0**](https://www.microsoft.com/en-us/download/details.aspx?id=44561)

You can/must install both if you want to support both Kinect versions.

Copy the config.lua.default to config.lua and changes the values accordingly.

This project relies on [Premake](https://github.com/premake/premake-core) to generate its files, [download the last version](https://github.com/premake/premake-core/releases) and use it the main folder (for example, to generate a Visual Studio 2019 solution use the `premake5 vs2019` command).

Open the project workspace/solution (located in build/<actionfolder>) and build it.

# Commonly asked questions

## I copied the files and the source doesn't show up

Did you install every dependency, including Kinect Runtime (or SDK) and Visual Studio 2019 redistribuables?
Are you using OBS Studio 25.0 or newer? Since 0.3 this plugins no longer works with OBS Studio 24.0 (this is because of source icons OBS-Studio added in v25, I can build the plugin for OBS 24 if upgrading is an issue for you).

Links are in the "requirement" and "how to use" parts, right above.

## I have a Kinect source but there are no device in the list

Are you sure to have a Kinect v1/v2 plugged in?
Did you download Kinect for Windows runtime for your Kinect version (see "How to use")?

If yes, please download Kinect for Windows SDK (see "How to build") and try to run Kinect examples from it.
If Kinect examples are running but this plugins doesn't work, please [create an issue](https://github.com/SirLynix/obs-kinect/issues) and post your OBS Studio logs with it.

## The plugin works but I have "LoadLibrary failed for obs-kinect-XXX" in OBS logs

This happens because the plugins tries to load all known backend, which may fail if you don't have some of their dependencies (like the Kinect for Windows runtime associated with it). Don't care too much about it, it's a normal thing.

Developer note: a way to fix that warning would be to load kinect runtime dynamically in obs-kinect backends, instead of linking them (this is already done with KinectBackgroundRemoval dll, so heh, why not).

## Does it work on Linux/macOS?

Not yet, I still have to try to use libfreenect(2) for that.
Unfortunately since some of the features this plugin provides (like body indexing and dedicated background removal) are based on Windows Kinect SDK features, theses will probably never be available to other systems.

## Does it work with Streamlabs OBS?

Unfortunately as far as I know Streamlabs OBS doesn't support thirdparty plugins, so nope.

## Does this plugin works for the Kinect v1 (Xbox 360 version)?

Yes! I added support for the Kinect v1 in 0.3!

## Does this plugin supports Azure Kinect (v3)

Not yet, because I don't have one to test it. If you have an Azure Kinect and are willing to help, let me know!

## My Kinect cannot be plugged by USB, how can I use it on Windows?

Unfortunately, Microsoft used a proprietary port on the Xbox 360/One.
For the Kinect v1, you have to buy a Kinect to USB 2.0 and AC adapter to use it on your computer (search for Microsoft Xbox 360 Kinect Sensor Mains Power Supply Adapter on Amazon).
For the Kinect v2, you have to buy a Kinect to USB 3.0 and AC adapter to use it on your computer (search for PeakLead Kinect to USB on Amazon).

Don't forget to install the [Kinect for Windows runtime](https://www.microsoft.com/en-us/download/details.aspx?id=44559) before using this plugin.

## What is the use of having access to the depth and infrared images?

I don't know, it looked fun.

Maybe you could use the infrared image for an horror game stream (where you play in the dark).

Be sure to tune the depth/infrared to color values to what suits you the most.

## How does the green screen effect works?

Kinect gives us a color map (like any webcam does) and a depth map, allowing to get the distance between a pixel and the Kinect sensor.
The green screen effect discard pixels being too far (or too close) and voilà.

There's also a cool effect of transition that you can enable, just play around and have fun.

Since 0.2 there's also a "Body" filter which uses the body informations from Kinect (basically Kinect tries to identifies pixels associated with a human being), which can filter you out even if you move away from the sensor.

Since the depth and body maps provided by Kinect have lower resolution than the color map, the raw effect looks kinda harsh. Fortunately this plugin also provide a real-time blur improving the effect.

Since 0.3, Body and depth filter can be combined with "Body + depth" or "Body within depth" green screen types.
Kinect v1 dedicated background removal (Microsoft background removal) is also supported.

## Why isn't dedicated background removal available?

If you're using a KinectV2 this is normal, Microsoft didn't (afaik) release a background removal library for Kinect v2.
If you're using a KinectV1, you're maybe lacking the KinectBackgroundRemoval180_(32|64).dll in your plugin folder (next to obs-kinect.dll)?

Please [create an issue](https://github.com/SirLynix/obs-kinect/issues) otherwise!

## Why do nothing shows up when I enable body green-screen type?

Body filtering is based on body data from Kinect, which means Kinect has to recognize your body first.

Kinect v2 does that kinda well but v1 may struggle at the beginning, try to move away from your Kinect to let it see your whole body and then come back.

## I have to get really far away from the Kinect for the green screen to work!

Kinect v1 depth range is about 80-400cm in front of it, but if you have a Kinect for Windows you can enable near mode to bring it to 40-200cm.

Kinect v2 depth range is about 50-500cm in front of it.

This range is a physical limitation, there's not much I can do about it.

## (KinectV1) Near mode doesn't change anything

It seems Kinect for 360 doesn't support near mode, as it was introduced by Kinect for Windows. There's not much I can do about it.

## (KinectV1) Having both a color and a infrared source at the same time doesn't seem to work, my color source look weird

This is a limitation of the Kinect v1, infrared replaces the color stream which mean it cannot be displayed at the same time as color.

Kinect v2 doesn't have this limitation.

## (KinectV1) It takes a lot of time to show something

Kinect v1 takes indeed a lot of time to initialize itself and to change the running mode. As far as I know this is from the Kinect/Microsoft SDK and I can't do much against it.
As far as my tests went, it may take up to 10-15s before showing up something.

Note: if you enabled the green-screen effect with a body filter, try to move away from the Kinect to let it detect your body and then come back.

## Can I have multiple Kinect source at the same time?

Yes!

Since 0.3, Kinect devices are only polled once, no matter how many sources you have.
This means you can have as many Kinect sources for the price of one per device.

However, greenscreen processing is done on a per-source basis.

## Why do I have some "transparency shadow" around me/my hands?

That's because the color and the IR sensor of the kinect are one centimeter apart and don't see exactly the same thing. What you're seeing is really a "depth shadow".

![](https://images.ctfassets.net/rf6r9wh4bnrh/1iq47o4qO2cUioS6eksIYa/55d9375728c295b944bbeae2998f22c5/Single-OcclusionShadows-v2.png)

(image from https://daqri.com/blog/depth-cameras-for-mobile-ar/)

I hope to be able to improve this (using the body index masking).

## It flickers so much

That's how Kinect depth map is, but I'm doing my best to improve it.

Since version 0.2 you can use blur passes on the filter image to improve the result.
You can also try to use the new body filter.

Since version 0.3 you can allow some "depth-lag", which means the plugin is allowed to fetch a previous depth value (up to X frames in the past) if current depth isn't available.

## Why do closes object disappears before reaching the min distance?

Kinect cannot read depth below 40cm (v1) / 50cm (v2) in front of it and invalid depth are discarded.
Try moving your Kinect further away from the object/person you want to film.

## Can I use this with VR (since SteamVR lighthouses use infrared too)?

I tested it with the HTC Vive base stations (SteamVR 1.0) and didn't have any issues related to Kinect depth mapping. However the Kinect may cause tracking issues (or even prevent VR tracking).

[Here's a cool guide](https://skarredghost.com/2016/12/09/how-to-use-kinect-with-htc-vive/) about reducing those interferences.

## Why is it called a "green screen" even though there's no green involved

I'm lacking a better name, being a developper implies I suck at naming things.

## Why didn't you buy a real green screen instead of a Kinect?

I don't have the space to use a real green screen, and don't want to run into light setup.

Also I wanted a Kinect so badly since it got out.

## Hey I have an idea

Great, how about posting it in the [issues](https://github.com/SirLynix/obs-kinect/issues) so we can talk about it?

## How can I help?

Fork this project, improve it and make a [pull request](https://github.com/SirLynix/obs-kinect/pulls)

## Why isn't my language available?

That's probably because I don't speak it, I can only do the english and french translations, but you can translate this plugin [pull request](https://github.com/SirLynix/obs-kinect/pulls) with your language file!

Thanks to @pucgenie and @saphir1997 for the german translation.

## What's the "GPU depth mapping/Use GPU to fetch color-to-depth values" option?

Color to depth mapping is done using a depth mapping texture, which allows the shader to get a color pixel depth coordinates but involves an indirect/dependent texture lookup. Every GPU on the market should support this (even the integrated ones) but it can be rather expensive on some GPUs.

In case this is an issue for you, you can uncheck that box to generate color to depth values on the CPU.

I do not recommend unchecking this box if you're not experiencing any issue.

## What are "depth-lagging frames"?

With the 0.3 release I introduced "depth-lagging frames" to helps with flickering. As Kinect depth grid flickers (because it doesn't sample the same points every frame), this option allows the plugin to fetch the last known depth value for that pixel up to X previous depth frames if required. 

This helps with flickering but also introduces a "movement shadow", which may or may not be okay according to what you do.

## Does this plugin support other devices than Kinect?

Nope, and I doubt it will as theses are the only depth camera I have.

If you're looking for Intel Realsense support (which seems to be a way better device than KinectV2 today), [OBS has builtin support for this](https://youtu.be/wW4HiiksDcU)
