/******************************************************************************
	Copyright (C) 2020 by Jérôme Leclercq <lynix680@gmail.com>

	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#ifndef OBS_KINECT_PLUGIN_KINECTFRAME
#define OBS_KINECT_PLUGIN_KINECTFRAME

#include "Helper.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

struct FrameData
{
	std::uint32_t width;
	std::uint32_t height;
	std::uint32_t pitch;
	std::vector<std::uint8_t> memory; //< TODO: Reuse memory
};

// A8 (alpha frame)
struct BackgroundRemovalFrameData : FrameData
{
	ObserverPtr<std::uint8_t[]> ptr;
};

// R8 (body index frame, 0 is player one, 1 is player two, etc. and 255 is no player)
struct BodyIndexFrameData : FrameData
{
	ObserverPtr<std::uint8_t[]> ptr;
};

// Color frame (custom format)
struct ColorFrameData : FrameData
{
	gs_color_format format;
	ObserverPtr<std::uint8_t[]> ptr;
};

// R16 depth frame (depth in millimeters)
struct DepthFrameData : FrameData
{
	ObserverPtr<std::uint16_t[]> ptr;
};

// R8 infrared frame
struct InfraredFrameData : FrameData
{
	ObserverPtr<std::uint8_t[]> ptr;
};

// RG16F depth mapping frame
struct DepthMappingFrameData : FrameData
{
	struct DepthCoordinates
	{
		float x;
		float y;
	};

	ObserverPtr<DepthCoordinates[]> ptr;
};

struct KinectFrame
{
	std::optional<BackgroundRemovalFrameData> backgroundRemovalFrame;
	std::optional<BodyIndexFrameData> bodyIndexFrame;
	std::optional<ColorFrameData> colorFrame;
	std::optional<DepthFrameData> depthFrame;
	std::optional<DepthMappingFrameData> depthMappingFrame;
	std::optional<InfraredFrameData> infraredFrame;
	std::uint64_t frameIndex;
};

using KinectFramePtr = std::shared_ptr<KinectFrame>;
using KinectFrameConstPtr = std::shared_ptr<const KinectFrame>;

#endif
