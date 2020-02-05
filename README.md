# obs-kinect

OBS Plugin to access Kinect data (and setup a virtual green screen based on depth).

# Requirement

- Windows. For now this plugin is only compatible with Windows as it uses the official Kinect for Windows API (may change in the future).
- A Kinect (obviously), v2 (xbox one) preferred (this plugin should work with v1 but this hasn't been tested), they're *relatively* cheap on eBay (between 50€ and 100€)
- If your Kinect isn't pluggable to your computer: a Kinect to USB adapter (search for PeakLead Kinect to USB on Amazon)
- Not running on a potato computer, especially for the faux green screen effect since this plugin processes pixels **on the CPU** which requires a lot of power (especially with the Kinect v2, as it has a 1080p color camera).
- [Kinect for Windows runtime](https://www.microsoft.com/en-us/download/details.aspx?id=44559) or [Kinect for Windows SDK](https://www.microsoft.com/en-us/download/details.aspx?id=44561) (if you plan to build this yourself or play with Kinect examples)
- OBS Studio >= 24.0.3 (hasn't been tested on older versions)

# To do

- Improve green-screen filtering
- Test & debug with Kinect v1
- Support Linux and macOS (using [libfreenect](https://github.com/OpenKinect/libfreenect) and [libfreenect2](https://github.com/OpenKinect/libfreenect2))
- Use shaders to do faux green-screen processing. 1080p pixel processing is a really heavy CPU task and would benefit a lot to run on the GPU.
- Add more fun effects (use issues to post your ideas)
- Add possibility to use body index masking (pixels identified by Kinect SDK to be you)
- Support audio?

# How to use

Download the latest releases and copy the files in your OBS folder, restart OBS and you should have a "Kinect source" available

# How to build

Clone and build OBS-studio first.
Be sure to have the [Kinect for Windows SDK](https://www.microsoft.com/en-us/download/details.aspx?id=44561)

Copy the config.lua.default to config.lua and changes the values accordingly.

This project relies on [Premake](https://github.com/premake/premake-core) to generate its files, [download the last version](https://github.com/premake/premake-core/releases) and use it the main folder (for example, to generate a Visual Studio 2019 solution use the `premake5 vs2019` command).

Open the project workspace/solution (located in build/<actionfolder>) and build it.

# Commonly asked questions

## Does it works on Linux/macOS?

Not yet, I still have to try to use libfreenect(2) for that.
Unfortunately since some of the features I'm planning are based on Windows Kinect SDK features (like body indexing), theses will probably never be available to other systems.

## Does this plugin works for the Kinect v1?

It should, since this plugin relies on the official Kinect for Windows SDK which works with both Kinect devices.

## I have a Kinect for Xbox One, how can I use it on my computer?

Unfortunately, Microsoft used a proprietary port on the Xbox One. You have to buy a USB 3.0 and AC adapter to use it on your computer (search for PeakLead Kinect to USB on Amazon).

Don't forget to install the [Kinect for Windows runtime](https://www.microsoft.com/en-us/download/details.aspx?id=44559) before using this plugin

## What is the use of having access to the depth and infrared images?

I don't know, it looked fun.

Maybe you could use the infrared image for an horror game stream (where you play in the dark).

Be sure to tune the depth/infrared to color values to what suits you the most.

## How does the green screen effect works?

Kinect gives us a color map (like any webcam does) and a depth map, allowing to get the distance between a pixel and the Kinect sensor.
The green screen effect discard pixels being too far (or too close) and voilà.
It also allows you to crop the source image, in case you have some depth artifacts. It also helps to decrease CPU power required for this effect (as cropped pixels are not processed).

There's also a cool effect of transition that you can enable, just play around and have fun.

## Why do I have some "transparency shadow" around me/my hands?

That's because the color and the IR sensor of the kinect are one centimeter apart and don't see exactly the same thing. What you're seeing is really a "depth shadow".

![](https://images.ctfassets.net/rf6r9wh4bnrh/1iq47o4qO2cUioS6eksIYa/55d9375728c295b944bbeae2998f22c5/Single-OcclusionShadows-v2.png)
(image from https://daqri.com/blog/depth-cameras-for-mobile-ar/)

I hope to be able to improve this (using the body index masking).

## It flickers so much

I know right? That's how the raw depth map is. I hope to be able to improve this.

You can try to use the bilinear filtering, it reduces this a little bit.

## Ew, that effect is ugly, it's so pixelated

That's because the depth map is one fourth of the color map (on the Kinect v2)

That's the very first version of this plugin, I will work on that in the future. But I doubt it will ever look perfect on fullscreen (this effect is intended for streaming where you typically don't occupy all the screen).

## Can I use this with VR (since SteamVR lighthouses use infrared too)?

I tested it with the HTC Vive base stations (SteamVR 1.0) and didn't have any issues, should be fine.

## Why is it called a "green screen" even though there's no green involved

I'm lacking a better name, being a developper implies I sucks at naming things.

## Why didn't you buy a real green screen instead of a Kinect?

I don't have the space to use a real green screen, and don't want to run into lights issues.

Also I wanted a Kinect so badly.

## It uses so much CPU

Yup, that's because all of the pixel processing is done on the CPU for now, I'm thinking of using the GPU to apply the green screen effect.

## Hey I have an idea

Great, how about posting it in the [issues](https://github.com/SirLynix/obs-kinect/issues) so we can talk about it?

## How can I help?

Fork this project, improve it and make a [pull request](https://github.com/SirLynix/obs-kinect/pulls)

## Why isn't my language available?

Because I don't speak it, I could only do the english and french translations, but you can make it available by making a [pull request](https://github.com/SirLynix/obs-kinect/pulls) with your language file!

