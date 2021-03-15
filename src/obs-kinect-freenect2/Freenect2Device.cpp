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

#include "Freenect2Device.hpp"
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/registration.h>
#include <util/threading.h>
#include <array>
#include <sstream>

KinectFreenect2Device::KinectFreenect2Device(libfreenect2::Freenect2Device* device) :
m_device(device)
{
	SetSupportedSources(Source_Color | Source_ColorMappedBody | Source_ColorMappedDepth | Source_Depth | Source_Infrared);
	SetUniqueName("Kinect " + m_device->getSerialNumber());
}

KinectFreenect2Device::~KinectFreenect2Device()
{
	StopCapture(); //< Ensure thread has joined before closing the device

	m_device->close();
}

void KinectFreenect2Device::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& error)
{
	os_set_thread_name("KinectDeviceFreenect2");

	try
	{
		//TODO
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

	if (!m_device->startStreams(true, true))
	{
		errorlog("failed to start streams");
		return;
	}

	std::optional<libfreenect2::SyncMultiFrameListener> multiframeListener;
	libfreenect2::FrameMap frameMap;

	unsigned int enabledFrameTypes = 0;
	SourceFlags enabledSourceFlags = 0;

	// Registration variables (depth to color mapping)
	constexpr std::size_t colorMappedDepthWidth = 1920;
	constexpr std::size_t colorMappedDepthHeight = 1082;
	constexpr std::size_t colorMappedDepthBpp = 4;
	constexpr std::size_t depthWidth = 512;
	constexpr std::size_t depthHeight = 424;
	constexpr std::size_t depthBpp = 4;

	std::optional<libfreenect2::Frame> undistorted;
	std::optional<libfreenect2::Frame> registered;
	std::optional<libfreenect2::Frame> colorMappedDepth;
	std::optional<libfreenect2::Registration> registration;

	auto UpdateMultiFrameListener = [&](SourceFlags newEnabledSources)
	{
		unsigned int newFrameTypes = 0;
		if (newEnabledSources & (Source_Color | Source_ColorMappedDepth))
			newFrameTypes |= libfreenect2::Frame::Color;

		if (newEnabledSources & (Source_Depth | Source_ColorMappedDepth))
			newFrameTypes |= libfreenect2::Frame::Depth;

		if (newEnabledSources & Source_Infrared)
			newFrameTypes |= libfreenect2::Frame::Ir;

		if (!multiframeListener || enabledFrameTypes != newFrameTypes)
		{
			if (multiframeListener)
				multiframeListener->release(frameMap);

			multiframeListener.emplace(newFrameTypes);
			enabledFrameTypes = newFrameTypes;

			m_device->setColorFrameListener(&multiframeListener.value());
			m_device->setIrAndDepthFrameListener(&multiframeListener.value());
		}

		if ((newEnabledSources & Source_ColorMappedDepth) != (enabledSourceFlags & Source_ColorMappedDepth))
		{
			if (newEnabledSources & Source_ColorMappedDepth)
			{
				colorMappedDepth.emplace(colorMappedDepthWidth, colorMappedDepthHeight, depthBpp);
				colorMappedDepth->format = libfreenect2::Frame::Float;
				colorMappedDepth->status = 0;

				registration.emplace(m_device->getIrCameraParams(), m_device->getColorCameraParams());
				registered.emplace(depthWidth, depthHeight, depthBpp);
				undistorted.emplace(depthWidth, depthHeight, depthBpp);
			}
			else
			{
				// Free registration memory, if allocated
				colorMappedDepth.reset();
				registered.reset();
				registration.reset();
				undistorted.reset();
			}
		}

		enabledSourceFlags = newEnabledSources;

		infolog("Kinect active sources: %s", EnabledSourceToString(newEnabledSources).c_str());
	};

	while (IsRunning())
	{
		if (auto sourceFlagUpdate = GetSourceFlagsUpdate())
		{
			try
			{
				UpdateMultiFrameListener(sourceFlagUpdate.value());
			}
			catch (const std::exception& e)
			{
				errorlog("%s", e.what());

				os_sleep_ms(10);
				continue;
			}
		}

		if (!multiframeListener)
		{
			os_sleep_ms(100);
			continue;
		}

		// Don't use waitForNewFrame with a timeout since it can indefinitely block (with no C++11 support)
		if (!multiframeListener->hasNewFrame())
		{
			os_sleep_ms(1);
			continue;
		}

		multiframeListener->waitForNewFrame(frameMap);

		libfreenect2::Frame* colorFrame = frameMap[libfreenect2::Frame::Color];
		libfreenect2::Frame* depthFrame = frameMap[libfreenect2::Frame::Depth];
		libfreenect2::Frame* infraredFrame = frameMap[libfreenect2::Frame::Ir];

		try
		{
			KinectFramePtr framePtr = std::make_shared<KinectFrame>();
			if (enabledSourceFlags & Source_Color)
				framePtr->colorFrame = RetrieveColorFrame(colorFrame);

			if (enabledSourceFlags & Source_Depth)
				framePtr->depthFrame = RetrieveDepthFrame(depthFrame);

			if (enabledSourceFlags & Source_Infrared)
				framePtr->infraredFrame = RetrieveInfraredFrame(infraredFrame);

			if (enabledSourceFlags & Source_ColorMappedDepth)
			{
				libfreenect2::Frame* colorMappedDepthFrame = &colorMappedDepth.value();
				registration->apply(colorFrame, depthFrame, &undistorted.value(), &registered.value(), true, colorMappedDepthFrame);
				framePtr->colorMappedDepthFrame = RetrieveDepthFrame(colorMappedDepthFrame);
			}

			UpdateFrame(std::move(framePtr));
		}
		catch (const std::exception& e)
		{
			errorlog("%s", e.what());

			// Force sleep to prevent log spamming
			os_sleep_ms(100);
		}

		multiframeListener->release(frameMap);
	}

	if (multiframeListener)
		multiframeListener->release(frameMap);

	m_device->stop();

	infolog("exiting thread");
}

ColorFrameData KinectFreenect2Device::RetrieveColorFrame(const libfreenect2::Frame* frame)
{
	if (!frame || frame->status != 0)
		throw std::runtime_error("invalid color frame");

	ColorFrameData frameData;
	frameData.width = static_cast<std::uint32_t>(frame->width);
	frameData.height = static_cast<std::uint32_t>(frame->height);

	// Convert to RGBA
	std::size_t memSize = frame->width * frame->height * 4;
	frameData.memory.resize(memSize);
	std::uint8_t* memPtr = frameData.memory.data();

	const std::uint8_t* frameMem = frame->data;
	if (frame->format == libfreenect2::Frame::BGRX)
	{
		for (std::size_t y = 0; y < frameData.height; ++y)
		{
			for (std::size_t x = 0; x < frameData.width; ++x)
			{
				*memPtr++ = frameMem[2];
				*memPtr++ = frameMem[1];
				*memPtr++ = frameMem[0];
				*memPtr++ = 0xFF;

				frameMem += 4;
			}
		}
	}
	else if (frame->format == libfreenect2::Frame::RGBX)
	{
		for (std::size_t y = 0; y < frameData.height; ++y)
		{
			for (std::size_t x = 0; x < frameData.width; ++x)
			{
				*memPtr++ = frameMem[0];
				*memPtr++ = frameMem[1];
				*memPtr++ = frameMem[2];
				*memPtr++ = 0xFF;

				frameMem += 4;
			}
		}
	}
	else
		throw std::runtime_error("unhandled color frame format (" + std::to_string(frame->format) + ")");

	frameData.ptr.reset(frameData.memory.data());
	frameData.pitch = static_cast<std::uint32_t>(frame->width * 4);
	frameData.format = GS_RGBA;

	return frameData;
}

DepthFrameData KinectFreenect2Device::RetrieveDepthFrame(const libfreenect2::Frame* frame)
{
	if (!frame || frame->status != 0)
		throw std::runtime_error("invalid depth frame");

	if (frame->format != libfreenect2::Frame::Float)
		throw std::runtime_error("unexpected format " + std::to_string(frame->format));

	DepthFrameData frameData;
	frameData.width = static_cast<std::uint32_t>(frame->width);
	frameData.height = static_cast<std::uint32_t>(frame->height);

	// Convert from floating point meters to uint16 millimeters (TODO: Allow depth frame output to be float?)
	std::size_t memSize = frame->width * frame->height * 2;
	frameData.memory.resize(memSize);
	std::uint16_t* memPtr = reinterpret_cast<std::uint16_t*>(frameData.memory.data());

	const float* frameMem = reinterpret_cast<const float*>(frame->data);
	for (std::size_t y = 0; y < frameData.height; ++y)
	{
		for (std::size_t x = 0; x < frameData.width; ++x)
		{
			float d = *frameMem++;
			*memPtr++ = static_cast<std::uint16_t>(std::max(d, 0.f));
		}
	}

	frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));
	frameData.pitch = static_cast<std::uint32_t>(frame->width * 2);

	return frameData;
}

InfraredFrameData KinectFreenect2Device::RetrieveInfraredFrame(const libfreenect2::Frame* frame)
{
	if (!frame || frame->status != 0)
		throw std::runtime_error("invalid infrared frame");

	if (frame->format != libfreenect2::Frame::Float)
		throw std::runtime_error("unexpected format " + std::to_string(frame->format));

	InfraredFrameData frameData;
	frameData.width = static_cast<std::uint32_t>(frame->width);
	frameData.height = static_cast<std::uint32_t>(frame->height);

	// Convert from floating point meters to uint16 millimeters (TODO: Allow depth frame output to be float?)
	std::size_t memSize = frame->width * frame->height * 2;
	frameData.memory.resize(memSize);
	std::uint16_t* memPtr = reinterpret_cast<std::uint16_t*>(frameData.memory.data());

	const float* frameMem = reinterpret_cast<const float*>(frame->data);
	for (std::size_t y = 0; y < frameData.height; ++y)
	{
		for (std::size_t x = 0; x < frameData.width; ++x)
		{
			*memPtr++ = static_cast<std::uint16_t>(*frameMem++);
		}
	}

	frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));
	frameData.pitch = static_cast<std::uint32_t>(frame->width * 2);

	return frameData;
}
