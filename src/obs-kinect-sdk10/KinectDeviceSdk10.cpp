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

#include "KinectDeviceSdk10.hpp"
#include <array>
#include <tlhelp32.h>

namespace
{
	template<typename FrameData>
	void ConvertResolutionToSize(NUI_IMAGE_RESOLUTION resolution, FrameData& frameData)
	{
		switch (resolution)
		{
			case NUI_IMAGE_RESOLUTION_80x60:
			{
				frameData.width = 80;
				frameData.height = 60;
				return;
			}

			case NUI_IMAGE_RESOLUTION_320x240:
			{
				frameData.width = 320;
				frameData.height = 240;
				return;
			}

			case NUI_IMAGE_RESOLUTION_640x480:
			{
				frameData.width = 640;
				frameData.height = 480;
				return;
			}

			case NUI_IMAGE_RESOLUTION_1280x960:
			{
				frameData.width = 1280;
				frameData.height = 960;
				return;
			}

			case NUI_IMAGE_RESOLUTION_INVALID:
				break;
		}

		throw std::runtime_error("invalid image resolution");
	}

	template<typename FrameData>
	NUI_IMAGE_RESOLUTION ConvertResolutionToSize(const FrameData& frameData)
	{
		if (frameData.width == 80 && frameData.height == 60)
			return NUI_IMAGE_RESOLUTION_80x60;
		else if (frameData.width == 320 && frameData.height == 240)
			return NUI_IMAGE_RESOLUTION_320x240;
		else if (frameData.width == 640 && frameData.height == 480)
			return NUI_IMAGE_RESOLUTION_640x480;
		else if (frameData.width == 1280 && frameData.height == 960)
			return NUI_IMAGE_RESOLUTION_1280x960;

		throw std::runtime_error("invalid image resolution");
	}

}

KinectDeviceSdk10::KinectDeviceSdk10(int sensorId) :
m_hasRequestedPrivilege(false)
{
	INuiSensor* kinectSensor;
	if (FAILED(NuiCreateSensorByIndex(sensorId, &kinectSensor)))
		throw std::runtime_error("failed to get Kinect sensor");

	m_kinectSensor.reset(kinectSensor);

	BSTR uniqueId = m_kinectSensor->NuiUniqueId();
	std::size_t length = std::wcslen(uniqueId);

	std::array<char, 128> u8UniqueId;
	os_wcs_to_utf8(uniqueId, length, u8UniqueId.data(), u8UniqueId.size());

	SetUniqueName(u8UniqueId.data());
}

void KinectDeviceSdk10::SetServicePriority(ProcessPriority priority)
{
}

void KinectDeviceSdk10::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr)
{
	HandlePtr colorEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr));
	HandlePtr depthEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr));

	HANDLE colorStream;
	HANDLE depthStream;

	EnabledSourceFlags enabledSourceFlags = 0;
	DWORD enabledFrameSourceTypes = 0;

	InitializedNuiSensorPtr<INuiSensor> openedSensor;

	std::int64_t colorTimestamp = 0;
	std::int64_t depthTimestamp = 0;

	auto UpdateMultiSourceFrameReader = [&](EnabledSourceFlags enabledSources)
	{
		DWORD newFrameSourcesTypes = 0;
		if (enabledSources & Source_Body)
			newFrameSourcesTypes |= NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX;
		else if (enabledSources & (Source_Depth | Source_ColorToDepthMapping))
			newFrameSourcesTypes |= NUI_INITIALIZE_FLAG_USES_DEPTH;

		if (enabledSources & (Source_Color | Source_ColorToDepthMapping))
			newFrameSourcesTypes |= NUI_INITIALIZE_FLAG_USES_COLOR;

		if (!openedSensor || newFrameSourcesTypes != enabledFrameSourceTypes)
		{
			openedSensor.reset(); //< Close sensor first

			if (FAILED(m_kinectSensor->NuiInitialize(newFrameSourcesTypes)))
				throw std::runtime_error("failed to initialize Kinect");

			ResetEvent(colorEvent.get());
			ResetEvent(depthEvent.get());

			if (newFrameSourcesTypes & NUI_INITIALIZE_FLAG_USES_COLOR)
			{
				if (FAILED(m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 2, colorEvent.get(), &colorStream)))
					throw std::runtime_error("failed to open color stream");

				colorTimestamp = 0;
			}

			if (newFrameSourcesTypes & NUI_INITIALIZE_FLAG_USES_DEPTH)
			{
				if (FAILED(m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH, NUI_IMAGE_RESOLUTION_640x480, NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE, 2, depthEvent.get(), &depthStream)))
					throw std::runtime_error("failed to open color stream");

				depthTimestamp = 0;
			}

			openedSensor.reset(m_kinectSensor.get());
		}

		enabledFrameSourceTypes = newFrameSourcesTypes;
		enabledSourceFlags = enabledSources;

		info("Kinect active sources: %s", EnabledSourceToString(enabledSourceFlags).c_str());
	};

	{
		std::unique_lock<std::mutex> lk(m);
		cv.notify_all();
	} // m & cv no longer exists from here

	constexpr std::uint64_t MaxKinectFPS = 30;

	std::uint64_t now = os_gettime_ns();
	std::uint64_t delay = 1'000'000'000ULL / MaxKinectFPS;

	KinectFramePtr nextFramePtr = std::make_shared<KinectFrame>();

	std::vector<LONG> tempMemory;

	while (IsRunning())
	{
		{
			if (auto sourceFlagUpdate = GetSourceFlagsUpdate())
			{
				try
				{
					UpdateMultiSourceFrameReader(sourceFlagUpdate.value());
				}
				catch (const std::exception& e)
				{
					blog(LOG_ERROR, "%s", e.what());

					os_sleep_ms(10);
					continue;
				}
			}
		}

		if (!openedSensor)
		{
			os_sleep_ms(100);
			continue;
		}

		try
		{
			// Check if color frame is available
			if ((enabledSourceFlags & Source_Color) && 
			    WaitForSingleObject(colorEvent.get(), 0) == WAIT_OBJECT_0)
			{
				try
				{
					nextFramePtr->colorFrame = RetrieveColorFrame(openedSensor.get(), colorStream, &colorTimestamp);
				}
				catch (const std::exception& e)
				{
					warn("failed to retrieve color frame: %s", e.what());
				}
			}

			if ((enabledSourceFlags & (Source_Depth | Source_ColorToDepthMapping)) && 
			    WaitForSingleObject(depthEvent.get(), 0) == WAIT_OBJECT_0)
			{
				try
				{
					nextFramePtr->depthFrame = RetrieveDepthFrame(openedSensor.get(), depthStream, &depthTimestamp);
				}
				catch (const std::exception& e)
				{
					warn("failed to retrieve depth frame: %s", e.what());
				}
			}

			bool canUpdateFrame = true;
			if ((enabledSourceFlags & Source_Color) &&
			    (enabledSourceFlags & (Source_Depth | Source_ColorToDepthMapping)))
			{
				// Ensure color and depth frames belongs to the same time frame
				if (colorTimestamp != 0 && depthTimestamp != 0)
				{
					constexpr std::int64_t MaxAllowedElapsedTime = (1000 / MaxKinectFPS) / 2;
					if ((colorTimestamp - depthTimestamp) > MaxAllowedElapsedTime)
						canUpdateFrame = false;
				}
				else
					canUpdateFrame = false;
			}

			if (canUpdateFrame)
			{
				// At this point, depth frame contains both index and depth informations
				if (nextFramePtr->depthFrame)
				{
					DepthFrameData& depthFrame = *nextFramePtr->depthFrame;

					if (enabledSourceFlags & Source_ColorToDepthMapping && nextFramePtr->colorFrame)
						nextFramePtr->depthMappingFrame = RetrieveDepthMappingFrame(openedSensor.get(), *nextFramePtr->colorFrame, depthFrame, tempMemory);
					
					// "Fix" depth frame
					ExtractDepth(depthFrame);
				}

				UpdateFrame(std::move(nextFramePtr));
				nextFramePtr = std::make_shared<KinectFrame>();
			}

			os_sleepto_ns(now += delay);
		}
		catch (const std::exception& e)
		{
			blog(LOG_ERROR, "%s", e.what());

			// Force sleep to prevent log spamming
			os_sleep_ms(100);
		}
	}

	blog(LOG_INFO, "exiting thread");
}

ColorFrameData KinectDeviceSdk10::RetrieveColorFrame(INuiSensor* sensor, HANDLE colorStream, std::int64_t* timestamp)
{
	NUI_IMAGE_FRAME colorFrame;
	if (FAILED(sensor->NuiImageStreamGetNextFrame(colorStream, 1, &colorFrame)))
		throw std::runtime_error("failed to access next frame");

	auto ReleaseColorFrame = [&](NUI_IMAGE_FRAME* frame) { sensor->NuiImageStreamReleaseFrame(colorStream, frame); };
	std::unique_ptr<NUI_IMAGE_FRAME, decltype(ReleaseColorFrame)> releaseColor(&colorFrame, ReleaseColorFrame);

	INuiFrameTexture* texture = colorFrame.pFrameTexture;

	NUI_LOCKED_RECT lockedRect;
	if (FAILED(texture->LockRect(0, &lockedRect, nullptr, 0)))
		throw std::runtime_error("failed to lock texture");

	auto UnlockRect = [](INuiFrameTexture* texture) { texture->UnlockRect(0); };
	std::unique_ptr<INuiFrameTexture, decltype(UnlockRect)> unlockRect(texture, UnlockRect);

	if (lockedRect.Pitch <= 0)
		throw std::runtime_error("texture pitch is zero");

	ColorFrameData frameData;
	ConvertResolutionToSize(colorFrame.eResolution, frameData);

	constexpr std::size_t bpp = 4; //< Color is stored as BGRA8

	std::size_t memSize = frameData.width * frameData.height * 4;
	frameData.memory.resize(memSize);
	std::uint8_t* memPtr = frameData.memory.data();

	frameData.ptr.reset(memPtr);
	frameData.pitch = frameData.width * 4;
	frameData.format = GS_BGRA;

	if (frameData.pitch == lockedRect.Pitch)
		std::memcpy(memPtr, lockedRect.pBits, frameData.pitch * frameData.height);
	else
	{
		std::uint32_t bestPitch = std::min<std::uint32_t>(frameData.pitch, lockedRect.Pitch);
		for (std::size_t y = 0; y < frameData.height; ++y)
		{
			const std::uint8_t* input = &lockedRect.pBits[y * lockedRect.Pitch];
			std::uint8_t* output = memPtr + y * frameData.pitch;

			std::memcpy(output, input, bestPitch);
		}
	}

	// Fix alpha (color frame alpha is at zero, because reasons)
	for (std::size_t y = 0; y < frameData.height; ++y)
	{
		for (std::size_t x = 0; x < frameData.width; ++x)
		{
			std::uint8_t* ptr = &memPtr[y * frameData.pitch + x * 4];
			ptr[3] = 255;
		}
	}

	if (timestamp)
		*timestamp = colorFrame.liTimeStamp.QuadPart;

	return frameData;
}

DepthFrameData KinectDeviceSdk10::RetrieveDepthFrame(INuiSensor* sensor, HANDLE depthStream, std::int64_t* timestamp)
{
	NUI_IMAGE_FRAME depthFrame;
	if (FAILED(sensor->NuiImageStreamGetNextFrame(depthStream, 1, &depthFrame)))
		throw std::runtime_error("failed to access next frame");

	auto ReleaseColorFrame = [&](NUI_IMAGE_FRAME* frame) { sensor->NuiImageStreamReleaseFrame(depthStream, frame); };
	std::unique_ptr<NUI_IMAGE_FRAME, decltype(ReleaseColorFrame)> releaseColor(&depthFrame, ReleaseColorFrame);

	INuiFrameTexture* texture = depthFrame.pFrameTexture;

	NUI_LOCKED_RECT lockedRect;
	if (FAILED(texture->LockRect(0, &lockedRect, nullptr, 0)))
		throw std::runtime_error("failed to lock texture");

	auto UnlockRect = [](INuiFrameTexture* texture) { texture->UnlockRect(0); };
	std::unique_ptr<INuiFrameTexture, decltype(UnlockRect)> unlockRect(texture, UnlockRect);

	if (lockedRect.Pitch <= 0)
		throw std::runtime_error("texture pitch is zero");

	DepthFrameData frameData;
	ConvertResolutionToSize(depthFrame.eResolution, frameData);

	constexpr std::size_t bpp = 2; //< Depth is stored as RG16 (depth and player index combined)

	std::size_t memSize = frameData.width * frameData.height * bpp;
	frameData.memory.resize(memSize);
	std::uint8_t* memPtr = frameData.memory.data();

	frameData.ptr.reset(memPtr);
	frameData.pitch = frameData.width * bpp;

	if (frameData.pitch == lockedRect.Pitch)
		std::memcpy(memPtr, lockedRect.pBits, frameData.pitch * frameData.height);
	else
	{
		std::uint32_t bestPitch = std::min<std::uint32_t>(frameData.pitch, lockedRect.Pitch);
		for (std::size_t y = 0; y < frameData.height; ++y)
		{
			const std::uint8_t* input = &lockedRect.pBits[y * lockedRect.Pitch];
			std::uint8_t* output = memPtr + y * frameData.pitch;

			std::memcpy(output, input, bestPitch);
		}
	}

	if (timestamp)
		*timestamp = depthFrame.liTimeStamp.QuadPart;

	return frameData;
}

DepthMappingFrameData KinectDeviceSdk10::RetrieveDepthMappingFrame(INuiSensor* sensor, const ColorFrameData& colorFrame, const DepthFrameData& depthFrame, std::vector<LONG>& tempMemory)
{
	DepthMappingFrameData outputFrameData;
	outputFrameData.width = colorFrame.width;
	outputFrameData.height = colorFrame.height;
	outputFrameData.pitch = colorFrame.width * sizeof(DepthCoordinates);

	std::size_t colorPixelCount = outputFrameData.width * outputFrameData.height;

	const std::uint16_t* depthPtr = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());
	std::size_t depthPixelCount = depthFrame.width * depthFrame.height;

	outputFrameData.memory.resize(colorPixelCount * sizeof(DepthCoordinates));
	outputFrameData.ptr.reset(outputFrameData.memory.data());

	tempMemory.resize(depthFrame.width * depthFrame.height * 2);
	if (FAILED(sensor->NuiImageGetColorPixelCoordinateFrameFromDepthPixelFrameAtResolution(
		ConvertResolutionToSize(colorFrame),
		ConvertResolutionToSize(depthFrame),
		depthFrame.width * depthFrame.height,
		reinterpret_cast<USHORT*>(depthFrame.ptr.get()),
		DWORD(tempMemory.size()),
		tempMemory.data()
	)))
		throw std::runtime_error("failed to map from depth to color");

	DepthCoordinates* outputPtr = reinterpret_cast<DepthCoordinates*>(outputFrameData.ptr.get());
	for (std::size_t i = 0; i < tempMemory.size(); i += 2)
	{
		outputPtr->x = static_cast<float>(tempMemory[i]);
		outputPtr->y = static_cast<float>(tempMemory[i + 1]);
		outputPtr++;
	}

	return outputFrameData;
}

void KinectDeviceSdk10::ExtractDepth(DepthFrameData& depthFrame)
{
	for (std::size_t y = 0; y < depthFrame.height; ++y)
	{
		for (std::size_t x = 0; x < depthFrame.width; ++x)
		{
			std::uint16_t& ptr = *reinterpret_cast<std::uint16_t*>(&depthFrame.ptr[y * depthFrame.pitch + x * 2]);
			ptr = NuiDepthPixelToDepth(ptr); //< Extract depth from depth and body combination
		}
	}
}

