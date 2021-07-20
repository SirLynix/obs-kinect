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

#include "FreenectDevice.hpp"
#include <util/threading.h>
#include <mutex>
#include <sstream>

KinectFreenectDevice::KinectFreenectDevice(freenect_device* device, const char* serial) :
m_device(device)
{
	SetSupportedSources(Source_Color);
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

	try
	{
		freenect_frame_mode colorMode = freenect_find_video_mode(FREENECT_RESOLUTION_MEDIUM, FREENECT_VIDEO_RGB);
		if (!colorMode.is_valid)
			throw std::runtime_error("failed to find a valid color mode");

		if (freenect_set_video_mode(m_device, colorMode) < 0)
			throw std::runtime_error("failed to set video mode");

		currentColorMode = colorMode;
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

	struct FreenectUserdata
	{
		const std::uint8_t* rgbPtr = nullptr;
		std::mutex mutex;
		std::uint32_t rgbTimestamp = 0;
	};

	FreenectUserdata ud;

	freenect_set_user(m_device, &ud);
	freenect_set_video_callback(m_device, [](freenect_device* device, void* rgb, uint32_t timestamp)
	{
		FreenectUserdata* userdata = static_cast<FreenectUserdata*>(freenect_get_user(device));

		std::scoped_lock lock(userdata->mutex);
		userdata->rgbPtr = static_cast<const std::uint8_t*>(rgb);
		userdata->rgbTimestamp = timestamp;
	});

	while (IsRunning())
	{
		KinectFramePtr framePtr = std::make_shared<KinectFrame>();
		{
			std::scoped_lock lock(ud.mutex);

			const std::uint8_t* frameMem = ud.rgbPtr;
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

		UpdateFrame(std::move(framePtr));
		os_sleep_ms(1000 / 30);
	}

	if (freenect_stop_video(m_device) != 0)
		errorlog("failed to stop video");

	infolog("exiting thread");
}
