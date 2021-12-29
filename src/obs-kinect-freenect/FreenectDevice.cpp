/******************************************************************************
	Copyright (C) 2021 by Jérôme Leclercq <lynix680@gmail.com>

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

#include "FreenectDevice.hpp"
#include <libfreenect/libfreenect_registration.h>
#include <util/threading.h>
#include <cstring>
#include <mutex>
#include <sstream>

KinectFreenectDevice::KinectFreenectDevice(freenect_device* device, const char* serial) :
m_device(device)
{
	SetSupportedSources(Source_Color | Source_Depth | Source_ColorMappedDepth);
	SetUniqueName("Kinect " + std::string(serial));
}

KinectFreenectDevice::~KinectFreenectDevice()
{
	StopCapture(); //< Ensure thread has joined before closing the device

	freenect_close_device(m_device);
}

void KinectFreenectDevice::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& error)
{
	os_set_thread_name("KinectDeviceFreenect");

	freenect_frame_mode currentColorMode;
	currentColorMode.is_valid = 0;

	freenect_frame_mode currentDepthMode;
	currentDepthMode.is_valid = 0;

	try
	{
		freenect_frame_mode colorMode = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
		if (!colorMode.is_valid)
			throw std::runtime_error("failed to find a valid color mode");

		if (freenect_set_video_mode(m_device, colorMode) < 0)
			throw std::runtime_error("failed to set video mode");

		currentColorMode = colorMode;

		freenect_frame_mode depthMode = freenect_find_depth_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_DEPTH_11BIT_PACKED);

		if (freenect_set_depth_mode(m_device, depthMode) < 0)
			throw std::runtime_error("failed to set video mode");

		currentDepthMode = depthMode;
	}
	catch (const std::exception&)
	{
		error = std::current_exception();
	}

	{
		std::unique_lock<std::mutex> lk(m);
		cv.notify_all();
	} // m & cv no longer exists from here

	if (error)
		return;

	if (freenect_start_video(m_device) != 0)
		errorlog("failed to start video");

	if (freenect_start_depth(m_device) != 0)
		errorlog("failed to start depth");

	struct FreenectUserdata
	{
		std::mutex depthMutex;
		std::mutex videoMutex;
		std::uint32_t depthTimestamp = 0;
		std::uint32_t videoTimestamp = 0;
		std::vector<std::uint16_t> depthBackBuffer;
		std::vector<std::uint16_t> depthFrontBuffer;
		std::vector<std::uint8_t> videoBackBuffer;
		std::vector<std::uint8_t> videoFrontBuffer;

	};

	FreenectUserdata ud;
	ud.depthBackBuffer.resize(currentDepthMode.bytes);
	ud.depthFrontBuffer.resize(currentDepthMode.bytes);
	ud.videoBackBuffer.resize(currentColorMode.bytes);
	ud.videoFrontBuffer.resize(currentColorMode.bytes);

	freenect_set_user(m_device, &ud);
	
	freenect_set_depth_buffer(m_device, ud.depthBackBuffer.data());
	freenect_set_depth_callback(m_device, [](freenect_device* device, void* /*depth*/, uint32_t timestamp)
	{
		FreenectUserdata* userdata = static_cast<FreenectUserdata*>(freenect_get_user(device));

		std::scoped_lock lock(userdata->depthMutex);
		userdata->depthTimestamp = timestamp;

		std::swap(userdata->depthBackBuffer, userdata->depthFrontBuffer);
		freenect_set_depth_buffer(device, userdata->depthBackBuffer.data());
	});

	freenect_set_video_buffer(m_device, ud.videoBackBuffer.data());
	freenect_set_video_callback(m_device, [](freenect_device* device, void* /*rgb*/, uint32_t timestamp)
	{
		FreenectUserdata* userdata = static_cast<FreenectUserdata*>(freenect_get_user(device));

		std::scoped_lock lock(userdata->videoMutex);
		userdata->videoTimestamp = timestamp;

		std::swap(userdata->videoBackBuffer, userdata->videoFrontBuffer);
		freenect_set_video_buffer(device, userdata->videoBackBuffer.data());
	});

	while (IsRunning())
	{
		KinectFramePtr framePtr = std::make_shared<KinectFrame>();

		// Video
		{
			std::scoped_lock lock(ud.videoMutex);

			const std::uint8_t* frameMem = ud.videoFrontBuffer.data();
			if (!frameMem)
				continue;

			ColorFrameData& frameData = framePtr->colorFrame.emplace();
			frameData.width = currentColorMode.width;
			frameData.height = currentColorMode.height;

			// Convert to RGBA
			std::size_t memSize = frameData.width * frameData.height * 4;
			frameData.memory.resize(memSize);
			std::uint8_t* memPtr = frameData.memory.data();

			for (std::size_t y = 0; y < frameData.height; ++y)
			{
				for (std::size_t x = 0; x < frameData.width; ++x)
				{
					*memPtr++ = frameMem[0];
					*memPtr++ = frameMem[1];
					*memPtr++ = frameMem[2];
					*memPtr++ = 0xFF;

					frameMem += 3;
				}
			}

			frameData.ptr.reset(frameData.memory.data());
			frameData.pitch = static_cast<std::uint32_t>(frameData.width * 4);
			frameData.format = GS_RGBA;
		}

		// Depth (and color mapped depth)
		{
			std::scoped_lock lock(ud.depthMutex);

			std::uint16_t* frameMem = ud.depthFrontBuffer.data();
			if (!frameMem)
				continue;

			// Depth
			{
				DepthFrameData& frameData = framePtr->depthFrame.emplace();
				frameData.width = currentDepthMode.width;
				frameData.height = currentDepthMode.height;

				// Copy it to buffer memory
				std::size_t memSize = frameData.width * frameData.height * 2;
				frameData.memory.resize(memSize);
				freenect_convert_packed_to_16bit(reinterpret_cast<std::uint8_t*>(frameMem), reinterpret_cast<std::uint16_t*>(frameData.memory.data()), 11, frameData.width*frameData.height);

				frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));
				frameData.pitch = static_cast<std::uint32_t>(frameData.width * 2);
			}

			// Color-mapped depth
			{
				DepthFrameData& frameData = framePtr->colorMappedDepthFrame.emplace();
				frameData.width = currentDepthMode.width;
				frameData.height = currentDepthMode.height;

				// Convert to R16
				std::size_t memSize = frameData.width * frameData.height * 2;
				frameData.memory.resize(memSize);
				freenect_map_depth_to_rgb(m_device, reinterpret_cast<std::uint8_t*>(frameMem), reinterpret_cast<std::uint16_t*>(frameData.memory.data()));

				frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));
				frameData.pitch = static_cast<std::uint32_t>(frameData.width * 2);
			}
		}

		UpdateFrame(std::move(framePtr));
		os_sleep_ms(1000 / 30);
	}

	if (freenect_stop_depth(m_device) != 0)
		errorlog("failed to stop depth");

	if (freenect_stop_video(m_device) != 0)
		errorlog("failed to stop video");

	infolog("exiting thread");
}
