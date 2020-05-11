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
		switch (HRESULT_CODE(hr))
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
m_kinectElevation(0),
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

		SetUniqueName("Kinect #" + std::to_string(sensorId) + ": " + u8UniqueId.data());
	}
	else
		SetUniqueName("Kinect #" + std::to_string(sensorId));

	m_elevationUpdateEvent.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));
	m_exitElevationThreadEvent.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));

	m_elevationThread = std::thread(&KinectSdk10Device::ElevationThreadFunc, this);

	RegisterIntParameter("sdk10_camera_elevation", 0, [](long long a, long long b)
	{
		if (b == 0)
			return a;

		return b;
	});

	INuiColorCameraSettings* pNuiCameraSettings;
	if (SUCCEEDED(m_kinectSensor->NuiGetColorCameraSettings(&pNuiCameraSettings)))
	{
		m_cameraSettings.reset(pNuiCameraSettings);

		double brightness = 0.5;
		double exposureTime = 2000;
		double frameInterval = 2000;
		double gain = 8.0;

		m_cameraSettings->GetBrightness(&brightness);
		m_cameraSettings->GetExposureTime(&exposureTime);
		m_cameraSettings->GetFrameInterval(&frameInterval);
		m_cameraSettings->GetGain(&gain);

		RegisterBoolParameter("sdk10_auto_exposure", true, [](bool a, bool b) { return a && b; });
		RegisterDoubleParameter("sdk10_brightness", brightness, 0.001, [](double a, double b) { return std::max(a, b); });
		RegisterDoubleParameter("sdk10_exposure_time", exposureTime, 1, [](double a, double b) { return std::max(a, b); });
		RegisterDoubleParameter("sdk10_frame_interval", frameInterval, 1, [](double a, double b) { return std::max(a, b); });
		RegisterDoubleParameter("sdk10_gain", frameInterval, 1, [](double a, double b) { return std::max(a, b); });
	}
}

KinectSdk10Device::~KinectSdk10Device()
{
	SetEvent(m_exitElevationThreadEvent.get());
	m_elevationThread.join();
}

obs_properties_t* KinectSdk10Device::CreateProperties() const
{
	obs_property_t* p;

	constexpr double BrignessMax = 1.0;
	constexpr double BrignessMin = 0.0;
	constexpr double GainMax = 16.0;
	constexpr double GainMin = 0.0;
	constexpr double ExposureMax = 4000.0;
	constexpr double ExposureMin = 1.0;
	constexpr double FrameIntervalMax = 4000.0;
	constexpr double FrameIntervalMin = 0.0;

	obs_properties_t* props = obs_properties_create();
	p = obs_properties_add_bool(props, "sdk10_auto_exposure", Translate("ObsKinectV1.AutoExposure"));
	p = obs_properties_add_float_slider(props, "sdk10_brightness", Translate("ObsKinectV1.Brightness"), BrignessMin, BrignessMax, 0.05);
	p = obs_properties_add_float_slider(props, "sdk10_exposure_time", Translate("ObsKinectV1.Exposure"), ExposureMin, ExposureMax, 20.0);
	p = obs_properties_add_float_slider(props, "sdk10_frame_interval", Translate("ObsKinectV1.FrameInterval"), ExposureMin, ExposureMax, 20.0);
	p = obs_properties_add_float_slider(props, "sdk10_gain", Translate("ObsKinectV1.Gain"), GainMin, GainMax, 0.1);
	p = obs_properties_add_int_slider(props, "sdk10_camera_elevation", Translate("ObsKinectV1.CameraElevation"), NUI_CAMERA_ELEVATION_MINIMUM, NUI_CAMERA_ELEVATION_MAXIMUM, 1);
	obs_property_int_set_suffix(p, "°");

	return props;
}

INuiSensor* KinectSdk10Device::GetSensor() const
{
	return m_kinectSensor.get();
}

void KinectSdk10Device::ElevationThreadFunc()
{
	std::array<HANDLE, 2> events = { m_exitElevationThreadEvent.get(), m_elevationUpdateEvent.get() };

	for (;;)
	{
		DWORD eventIndex = WaitForMultipleObjects(DWORD(events.size()), events.data(), FALSE, INFINITE);
		switch (eventIndex)
		{
			case WAIT_OBJECT_0 + 1:
			{
				os_sleep_ms(250); //< sleep a bit to help reduce SetAngle commands and wear

				ResetEvent(m_elevationUpdateEvent.get());
				LONG newElevation = m_kinectElevation.load(std::memory_order_relaxed);

				info("setting elevation angle to %d", int(newElevation));
				HRESULT hr = m_kinectSensor->NuiCameraElevationSetAngle(newElevation);
				if (FAILED(hr))
				{
					switch (HRESULT_CODE(hr))
					{
						// Kinect doesn't like moving too quick, wait a bit and try again
						case ERROR_RETRY:
						case ERROR_TOO_MANY_CMDS:
							os_sleep_ms(100);
							SetEvent(m_elevationUpdateEvent.get());
							break;

						default:
							warn("failed to change Kinect elevation: %s", ErrToString(hr).c_str());
							break;
					}
				}
				break;
			}

			case WAIT_OBJECT_0: //< exit thread event
			default: //< shouldn't happen but still
				return;
		}
	}
}

void KinectSdk10Device::HandleBoolParameterUpdate(const std::string& parameterName, bool value)
{
	if (parameterName == "sdk10_auto_exposure")
		m_cameraSettings->SetAutoExposure(value);
}

void KinectSdk10Device::HandleDoubleParameterUpdate(const std::string& parameterName, double value)
{
	if (parameterName == "sdk10_brightness")
		m_cameraSettings->SetBrightness(value);
	else if (parameterName == "sdk10_exposure_time")
		m_cameraSettings->SetExposureTime(value);
	else if (parameterName == "sdk10_frame_interval")
		m_cameraSettings->SetFrameInterval(value);
	else if (parameterName == "sdk10_gain")
		m_cameraSettings->SetGain(value);
}

void KinectSdk10Device::HandleIntParameterUpdate(const std::string& parameterName, long long value)
{
	if (parameterName == "sdk10_camera_elevation")
	{
		m_kinectElevation.store(LONG(value), std::memory_order_relaxed);
		SetEvent(m_elevationUpdateEvent.get());
	}
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
			Kinect v1 doesn't like to output both color and infrared at the same time, we have to force reset the device when switching
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
				hr = m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH, NUI_IMAGE_RESOLUTION_640x480, NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE, 2, depthEvent.get(), &depthStream);
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
			std::array<HANDLE, 3> events;
			DWORD eventCount = 0;

			if (enabledSourceFlags & Source_Color)
				events[eventCount++] = colorEvent.get();

			if (enabledSourceFlags & (Source_Body | Source_Depth | Source_ColorToDepthMapping))
				events[eventCount++] = depthEvent.get();

			if (enabledSourceFlags & Source_Infrared)
				events[eventCount++] = irEvent.get();

			WaitForMultipleObjects(eventCount, events.data(), FALSE, 100);

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
	outputFrameData.pitch = colorFrame.width * sizeof(DepthMappingFrameData::DepthCoordinates);

	std::size_t colorPixelCount = outputFrameData.width * outputFrameData.height;

	outputFrameData.memory.resize(colorPixelCount * sizeof(DepthMappingFrameData::DepthCoordinates));
	outputFrameData.ptr.reset(reinterpret_cast<DepthMappingFrameData::DepthCoordinates*>(outputFrameData.memory.data()));

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

	DepthMappingFrameData::DepthCoordinates* outputPtr = outputFrameData.ptr.get();
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
			std::uint16_t ptr = depthFrame.ptr[y * depthFrame.width + x];

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

	frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(memPtr));
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
			std::uint16_t& ptr = depthFrame.ptr[y * depthFrame.width + x];
			ptr = NuiDepthPixelToDepth(ptr); //< Extract depth from depth and body combination
		}
	}
}
