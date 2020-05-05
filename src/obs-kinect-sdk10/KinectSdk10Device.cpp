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

#include "KinectSdk10Device.hpp"
#include <util/threading.h>
#include <array>

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

	std::string ErrToString(HRESULT hr)
	{
		switch (hr)
		{
			case S_OK: return "No error";
			case E_FAIL: return "Unspecified failure";
			case E_NUI_ALREADY_INITIALIZED: return "Already initialized";
			case E_NUI_BADINDEX: return "Bad device index";
			case E_NUI_DATABASE_NOT_FOUND: return "Database not found";
			case E_NUI_DATABASE_VERSION_MISMATCH: return "Database version mismatch.";
			case E_NUI_DEVICE_NOT_CONNECTED: return "Device not connected";
			case E_NUI_DEVICE_NOT_READY: return "Device not ready";
			case E_NUI_NO_MORE_ITEMS: return "No more items";
			case E_NUI_FRAME_NO_DATA: return "No data in frame";
			case E_NUI_STREAM_NOT_ENABLED: return "Stream not enabled";
			case E_NUI_IMAGE_STREAM_IN_USE: return "Image stream is in use";
			case E_NUI_FRAME_LIMIT_EXCEEDED: return "Exceeded frame limit";
			case E_NUI_FEATURE_NOT_INITIALIZED: return "Feature is not initialized";
			case E_NUI_NOTGENUINE: return "Device is not genuine";
			case E_NUI_INSUFFICIENTBANDWIDTH: return "Insufficient USB bandwidth";
			case E_NUI_NOTSUPPORTED: return "Not supported";
			case E_NUI_DEVICE_IN_USE: return "Device is already in use";
			case E_NUI_HARDWARE_FEATURE_UNAVAILABLE: return "The requested feateure is not available on this version of the hardware";
			case E_NUI_NOTCONNECTED: return "The hub is no longer connected to the machine";
			case E_NUI_NOTREADY: return "Some part of the device is not connected";
			case E_NUI_SKELETAL_ENGINE_BUSY: return "Skeletal engine is already in use";
			case E_NUI_NOTPOWERED: return "The hub and motor are connected, but the camera is not";
			default: return "Unknown error";
		}
	}
}

KinectSdk10Device::KinectSdk10Device(int sensorId) :
m_hasRequestedPrivilege(false)
{
	HRESULT hr;

	INuiSensor* kinectSensor;
	
	hr = NuiCreateSensorByIndex(sensorId, &kinectSensor);
	if (FAILED(hr))
		throw std::runtime_error("failed to get Kinect sensor: " + ErrToString(hr));

	m_kinectSensor.reset(kinectSensor);

	INuiCoordinateMapper* coordinateMapper;

	hr = m_kinectSensor->NuiGetCoordinateMapper(&coordinateMapper);
	if (FAILED(hr))
		throw std::runtime_error("failed to get coordinate mapper: " + ErrToString(hr));

	m_coordinateMapper.reset(coordinateMapper);

	BSTR uniqueId = m_kinectSensor->NuiUniqueId();
	if (uniqueId) //< null unique id can happen with replaced usb drivers
	{
		std::size_t length = std::wcslen(uniqueId);

		std::array<char, 128> u8UniqueId;
		os_wcs_to_utf8(uniqueId, length, u8UniqueId.data(), u8UniqueId.size());

		SetUniqueName(u8UniqueId.data());
	}
	else
		SetUniqueName("Kinect #" + std::to_string(sensorId));
}

void KinectSdk10Device::SetServicePriority(ProcessPriority priority)
{
}

void KinectSdk10Device::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& exceptionPtr)
{
	os_set_thread_name("KinectDeviceSdk10");

	HandlePtr colorEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr));
	HandlePtr depthEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr));
	HandlePtr irEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr));

	HANDLE colorStream;
	HANDLE depthStream;
	HANDLE irStream;

	EnabledSourceFlags enabledSourceFlags = 0;
	DWORD enabledFrameSourceTypes = 0;

	InitializedNuiSensorPtr<INuiSensor> openedSensor;

	std::int64_t colorTimestamp = 0;
	std::int64_t depthTimestamp = 0;
	std::int64_t irTimestamp = 0;

	auto UpdateMultiSourceFrameReader = [&](EnabledSourceFlags enabledSources)
	{
		bool forceReset = (openedSensor == nullptr);
		DWORD newFrameSourcesTypes = 0;
		if (enabledSources & Source_Body)
			newFrameSourcesTypes |= NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX;
		else if (enabledSources & (Source_Depth | Source_ColorToDepthMapping))
			newFrameSourcesTypes |= NUI_INITIALIZE_FLAG_USES_DEPTH;

		if (enabledSources & (Source_Color | Source_ColorToDepthMapping | Source_Infrared)) //< Yup, IR requires color
		{
			newFrameSourcesTypes |= NUI_INITIALIZE_FLAG_USES_COLOR;
		
			/*
			Kinect v1 don't like to output both color and infrared at the same time, we have to force reset the device when switching
			from color to infrared or vice-versa to prevent frame corruption/frame without data
			*/
			if ((enabledSourceFlags & (Source_Color | Source_Infrared)) != (enabledSources & (Source_Color | Source_Infrared)))
				forceReset = true;
		}

		if (forceReset || newFrameSourcesTypes != enabledFrameSourceTypes)
		{
			openedSensor.reset(); //< Close sensor first (Kinectv1 doesn't support multiple NuiInitialize)

			HRESULT hr;

			hr = m_kinectSensor->NuiInitialize(newFrameSourcesTypes);
			if (FAILED(hr))
				throw std::runtime_error("failed to initialize Kinect: " + ErrToString(hr));

			ResetEvent(colorEvent.get());
			ResetEvent(depthEvent.get());

			if (newFrameSourcesTypes & NUI_INITIALIZE_FLAG_USES_COLOR)
			{
				hr = m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 2, colorEvent.get(), &colorStream);
				if (FAILED(hr))
					throw std::runtime_error("failed to open color stream: " + ErrToString(hr));

				colorTimestamp = 0;
			}

			if (newFrameSourcesTypes & NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX)
			{
				hr = m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_640x480, NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE, 2, depthEvent.get(), &depthStream);
				if (FAILED(hr))
					throw std::runtime_error("failed to open color stream: " + ErrToString(hr));

				depthTimestamp = 0;
			}
			else if (newFrameSourcesTypes & NUI_INITIALIZE_FLAG_USES_DEPTH)
			{
				hr = m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH, NUI_IMAGE_RESOLUTION_320x240, NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE, 2, depthEvent.get(), &depthStream);
				if (FAILED(hr))
					throw std::runtime_error("failed to open color stream: " + ErrToString(hr));

				depthTimestamp = 0;
			}

			if (enabledSources & Source_Infrared)
			{
				hr = m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR_INFRARED, NUI_IMAGE_RESOLUTION_640x480, 0, 2, irEvent.get(), &irStream);
				if (FAILED(hr))
					throw std::runtime_error("failed to open infrared stream: " + ErrToString(hr));

				irTimestamp = 0;
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

	constexpr std::uint64_t KinectMaxFramerate = 30;
	constexpr std::uint64_t PollingRate = KinectMaxFramerate * 2; //< Poll at twice the highest framerate of Kinect, to be sure (TODO: Use events)

	std::uint64_t now = os_gettime_ns();
	std::uint64_t delay = 1'000'000'000ULL / PollingRate;

	KinectFramePtr nextFramePtr = std::make_shared<KinectFrame>();

	std::vector<std::uint8_t> tempMemory;

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

			if ((enabledSourceFlags & Source_Infrared) && 
			    WaitForSingleObject(irEvent.get(), 0) == WAIT_OBJECT_0)
			{
				try
				{
					nextFramePtr->infraredFrame = RetrieveInfraredFrame(openedSensor.get(), irStream, &irTimestamp);
				}
				catch (const std::exception& e)
				{
					warn("failed to retrieve infrared frame: %s", e.what());
				}
			}

			bool canUpdateFrame = true;

			// Check all timestamp belongs to the same timeframe
			std::array<std::int64_t, 3> timestamps;
			std::size_t timestampCount = 0;

			if (enabledSourceFlags & Source_Color)
				timestamps[timestampCount++] = colorTimestamp;

			if (enabledSourceFlags & (Source_Body | Source_Depth | Source_ColorToDepthMapping))
				timestamps[timestampCount++] = depthTimestamp;

			if (enabledSourceFlags & Source_Infrared)
				timestamps[timestampCount++] = irTimestamp;

			std::int64_t refTimestamp = timestamps.front();
			for (std::size_t i = 0; i < timestampCount; ++i)
			{
				// 0 means we have no frame right now, skip it
				if (timestamps[i] == 0)
				{
					canUpdateFrame = false;
					break;
				}

				constexpr std::int64_t MaxAllowedElapsedTime = (1000 / KinectMaxFramerate) / 2;
				if (refTimestamp - timestamps[i] > MaxAllowedElapsedTime)
				{
					canUpdateFrame = false;
					break;
				}
			}

			if (canUpdateFrame)
			{
				// At this point, depth frame contains both index and depth informations
				if (nextFramePtr->depthFrame)
				{
					DepthFrameData& depthFrame = *nextFramePtr->depthFrame;

					if (enabledSourceFlags & Source_Body)
						nextFramePtr->bodyIndexFrame = BuildBodyFrame(depthFrame);

					if (enabledSourceFlags & Source_ColorToDepthMapping)
						nextFramePtr->depthMappingFrame = BuildDepthMappingFrame(openedSensor.get(), *nextFramePtr->colorFrame, depthFrame, tempMemory);
					
					// "Fix" depth frame
					ExtractDepth(depthFrame);
				}

				UpdateFrame(std::move(nextFramePtr));
				nextFramePtr = std::make_shared<KinectFrame>();
				colorTimestamp = 0;
				depthTimestamp = 0;
				irTimestamp = 0;
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

DepthMappingFrameData KinectSdk10Device::BuildDepthMappingFrame(INuiSensor* sensor, const ColorFrameData& colorFrame, const DepthFrameData& depthFrame, std::vector<std::uint8_t>& tempMemory)
{
	DepthMappingFrameData outputFrameData;
	outputFrameData.width = colorFrame.width;
	outputFrameData.height = colorFrame.height;
	outputFrameData.pitch = colorFrame.width * sizeof(DepthCoordinates);

	std::size_t colorPixelCount = outputFrameData.width * outputFrameData.height;

	outputFrameData.memory.resize(colorPixelCount * sizeof(DepthCoordinates));
	outputFrameData.ptr.reset(outputFrameData.memory.data());

	std::size_t depthPixelCount = depthFrame.width * depthFrame.height;
	const std::uint16_t* depthPixels = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());

	std::size_t depthImagePointSize = colorPixelCount * sizeof(NUI_DEPTH_IMAGE_POINT);
	tempMemory.resize(depthImagePointSize + depthPixelCount * sizeof(NUI_DEPTH_IMAGE_PIXEL));

	NUI_DEPTH_IMAGE_POINT* depthImagePoints = reinterpret_cast<NUI_DEPTH_IMAGE_POINT*>(&tempMemory[0]);
	NUI_DEPTH_IMAGE_PIXEL* depthImagePixels = reinterpret_cast<NUI_DEPTH_IMAGE_PIXEL*>(&tempMemory[depthImagePointSize]);

	for (std::size_t y = 0; y < depthFrame.height; ++y)
	{
		for (std::size_t x = 0; x < depthFrame.width; ++x)
		{
			std::size_t depthIndex = y * depthFrame.width + x;
			depthImagePixels[depthIndex].depth = NuiDepthPixelToDepth(depthPixels[depthIndex]);
			depthImagePixels[depthIndex].playerIndex = NuiDepthPixelToPlayerIndex(depthPixels[depthIndex]); //< Not a big deal if not available
		}
	}

	HRESULT hr = m_coordinateMapper->MapColorFrameToDepthFrame(
		NUI_IMAGE_TYPE_COLOR,
		ConvertResolutionToSize(colorFrame),
		ConvertResolutionToSize(depthFrame),
		DWORD(depthPixelCount),
		depthImagePixels,
		DWORD(colorPixelCount),
		depthImagePoints
	);

	if (FAILED(hr))
		throw std::runtime_error("failed to map from depth to color: " + ErrToString(hr));

	DepthCoordinates* outputPtr = reinterpret_cast<DepthCoordinates*>(outputFrameData.ptr.get());
	for (std::size_t y = 0; y < outputFrameData.height; ++y)
	{
		for (std::size_t x = 0; x < outputFrameData.width; ++x)
		{
			std::size_t colorIndex = y * outputFrameData.width + x;

			const NUI_DEPTH_IMAGE_POINT& depthImagePoint = depthImagePoints[colorIndex];

			outputPtr->x = float(depthImagePoint.x);
			outputPtr->y = float(depthImagePoint.y);
			outputPtr++;
		}
	}

	return outputFrameData;
}

BodyIndexFrameData KinectSdk10Device::BuildBodyFrame(const DepthFrameData& depthFrame)
{
	BodyIndexFrameData frameData;
	frameData.width = depthFrame.width;
	frameData.height = depthFrame.height;
	
	constexpr std::size_t bpp = 1; //< Body index is stored as R8

	frameData.pitch = frameData.width * bpp;
	frameData.memory.resize(frameData.width * frameData.height * bpp);

	std::uint8_t* memPtr = reinterpret_cast<std::uint8_t*>(frameData.memory.data());
	frameData.ptr.reset(memPtr);

	for (std::size_t y = 0; y < depthFrame.height; ++y)
	{
		for (std::size_t x = 0; x < depthFrame.width; ++x)
		{
			const std::uint16_t& ptr = *reinterpret_cast<const std::uint16_t*>(&depthFrame.ptr[y * depthFrame.pitch + x * 2]);

			// Extract body index from depth and body combination
			std::uint8_t bodyIndex = static_cast<std::uint8_t>(NuiDepthPixelToPlayerIndex(ptr));

			*memPtr++ = (bodyIndex > 0) ? bodyIndex - 1 : 0xFF; //< Convert to Kinect v2 player index logic
		}
	}

	return frameData;
}

ColorFrameData KinectSdk10Device::RetrieveColorFrame(INuiSensor* sensor, HANDLE colorStream, std::int64_t* timestamp)
{
	HRESULT hr;

	NUI_IMAGE_FRAME colorFrame;

	hr = sensor->NuiImageStreamGetNextFrame(colorStream, 1, &colorFrame);
	if (FAILED(hr))
		throw std::runtime_error("failed to access next frame: " + ErrToString(hr));

	auto ReleaseColorFrame = [&](NUI_IMAGE_FRAME* frame) { sensor->NuiImageStreamReleaseFrame(colorStream, frame); };
	std::unique_ptr<NUI_IMAGE_FRAME, decltype(ReleaseColorFrame)> releaseColor(&colorFrame, ReleaseColorFrame);

	INuiFrameTexture* texture = colorFrame.pFrameTexture;

	NUI_LOCKED_RECT lockedRect;
	
	hr = texture->LockRect(0, &lockedRect, nullptr, 0);
	if (FAILED(hr))
		throw std::runtime_error("failed to lock texture: " + ErrToString(hr));

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

DepthFrameData KinectSdk10Device::RetrieveDepthFrame(INuiSensor* sensor, HANDLE depthStream, std::int64_t* timestamp)
{
	HRESULT hr;

	NUI_IMAGE_FRAME depthFrame;

	hr = sensor->NuiImageStreamGetNextFrame(depthStream, 1, &depthFrame);
	if (FAILED(hr))
		throw std::runtime_error("failed to access next frame: " + ErrToString(hr));

	auto ReleaseColorFrame = [&](NUI_IMAGE_FRAME* frame) { sensor->NuiImageStreamReleaseFrame(depthStream, frame); };
	std::unique_ptr<NUI_IMAGE_FRAME, decltype(ReleaseColorFrame)> releaseColor(&depthFrame, ReleaseColorFrame);

	INuiFrameTexture* texture = depthFrame.pFrameTexture;

	NUI_LOCKED_RECT lockedRect;

	hr = texture->LockRect(0, &lockedRect, nullptr, 0);
	if (FAILED(hr))
		throw std::runtime_error("failed to lock texture: " + ErrToString(hr));

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

InfraredFrameData KinectSdk10Device::RetrieveInfraredFrame(INuiSensor* sensor, HANDLE irStream, std::int64_t* timestamp)
{
	HRESULT hr;

	NUI_IMAGE_FRAME irFrame;

	hr = sensor->NuiImageStreamGetNextFrame(irStream, 1, &irFrame);
	if (FAILED(hr))
		throw std::runtime_error("failed to access next frame: " + ErrToString(hr));

	auto ReleaseColorFrame = [&](NUI_IMAGE_FRAME* frame) { sensor->NuiImageStreamReleaseFrame(irStream, frame); };
	std::unique_ptr<NUI_IMAGE_FRAME, decltype(ReleaseColorFrame)> releaseColor(&irFrame, ReleaseColorFrame);

	INuiFrameTexture* texture = irFrame.pFrameTexture;

	NUI_LOCKED_RECT lockedRect;

	hr = texture->LockRect(0, &lockedRect, nullptr, 0);
	if (FAILED(hr))
		throw std::runtime_error("failed to lock texture: " + ErrToString(hr));

	auto UnlockRect = [](INuiFrameTexture* texture) { texture->UnlockRect(0); };
	std::unique_ptr<INuiFrameTexture, decltype(UnlockRect)> unlockRect(texture, UnlockRect);

	if (lockedRect.Pitch <= 0)
		throw std::runtime_error("texture pitch is zero");

	InfraredFrameData frameData;
	ConvertResolutionToSize(irFrame.eResolution, frameData);

	constexpr std::size_t bpp = 2; //< Infrared is stored as RG16

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
		*timestamp = irFrame.liTimeStamp.QuadPart;

	return frameData;
}

void KinectSdk10Device::ExtractDepth(DepthFrameData& depthFrame)
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

