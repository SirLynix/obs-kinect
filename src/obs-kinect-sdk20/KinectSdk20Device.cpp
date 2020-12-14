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

#include "KinectSdk20Device.hpp"
#include <util/threading.h>
#include <array>
#include <tlhelp32.h>

KinectSdk20Device::KinectSdk20Device()
{
	IKinectSensor* pKinectSensor;
	if (FAILED(GetDefaultKinectSensor(&pKinectSensor)))
		throw std::runtime_error("failed to get Kinect sensor");

	m_kinectSensor.reset(pKinectSensor);

	if (FAILED(m_kinectSensor->Open()))
		throw std::runtime_error("failed to open Kinect sensor");

	m_openedKinectSensor.reset(pKinectSensor);

	ICoordinateMapper* pCoordinateMapper;
	if (FAILED(m_kinectSensor->get_CoordinateMapper(&pCoordinateMapper)))
		throw std::runtime_error("failed to retrieve coordinate mapper");

	m_coordinateMapper.reset(pCoordinateMapper);

	SetSupportedSources(Source_Body | Source_Color | Source_ColorToDepthMapping | Source_Depth | Source_Infrared);
	SetUniqueName("Default Kinect");

	RegisterIntParameter("sdk20_service_priority", static_cast<long long>(ProcessPriority::Normal), [](long long a, long long b)
	{
		return std::max(a, b);
	});

#if HAS_NUISENSOR_LIB
	std::array<NUISENSOR_DEVICE_INFO, 16> devices;
	ULONG deviceFound = NuiSensor_FindAllDevices(devices.data(), ULONG(devices.size()));
	if (deviceFound > 0)
	{
		auto DeviceToString = [](const NUISENSOR_DEVICE_INFO& deviceInfo) -> std::string
		{
			std::array<char, MAX_PATH * 4> devicePath;
			std::size_t length = os_wcs_to_utf8(deviceInfo.DevicePath, 0, devicePath.data(), devicePath.size());
			if (length == 0)
				return "<Error>";

			return std::string(devicePath.data(), length);
		};

		if (deviceFound > 1)
		{
			// Multiple Kinect v2 found, find the right one by using the serial number
			std::array<wchar_t, 256> wideId = { L"<failed to get id>" };
			HRESULT serialResult = m_openedKinectSensor->get_UniqueKinectId(UINT(wideId.size()), wideId.data());
			if (SUCCEEDED(serialResult))
			{
				std::size_t serialLength = std::wcslen(wideId.data());
				for (ULONG i = 0; i < deviceFound; ++i)
				{
					NUISENSOR_HANDLE deviceHandle;
					if (!NuiSensor_InitializeEx(&deviceHandle, devices[i].DevicePath))
					{
						errorlog("failed to initialize device #%u %s", i, DeviceToString(devices[i]).c_str());
						continue;
					}

					m_nuiHandle.reset(deviceHandle);

					NUISENSOR_SERIAL_NUMBER serial;
					if (!NuiSensor_GetSerialNumber(deviceHandle, &serial))
					{
						errorlog("failed to retrieve serial number of device #%u (%s)", i, DeviceToString(devices[i]).c_str());
						continue;
					}

					// Even though NUISENSOR_SERIAL_NUMBER returns an array of byte, it seems to be an array of wchar_t that can be compared using memcmp
					if (std::memcmp(serial.Data, wideId.data(), std::min(sizeof(serial.Data) / sizeof(BYTE), serialLength * sizeof(wchar_t))) == 0)
					{
						// Found it!
						break;
					}

					m_nuiHandle.reset();
				}
			}
			else
				errorlog("failed to retrieve Kinect serial");
		}
		else
		{
			NUISENSOR_HANDLE deviceHandle;
			if (NuiSensor_InitializeEx(&deviceHandle, devices[0].DevicePath))
				m_nuiHandle.reset(deviceHandle);
			else
				errorlog("failed to initialize device #0 %s", DeviceToString(devices[0]).c_str());
		}
	}

	if (m_nuiHandle)
	{
		auto MaxDouble = [](double a, double b)
		{
			return std::max(a, b);
		};

		auto MaxInt = [](long long a, long long b)
		{
			return std::max(a, b);
		};

		// Default values read from my KinectV2
		RegisterIntParameter("sdk20_exposure_mode", static_cast<int>(ExposureControl::FullyAuto), MaxInt);
		RegisterDoubleParameter("sdk20_analog_gain", 5.333333, 0.01, MaxDouble);
		RegisterDoubleParameter("sdk20_digital_gain", 1.000286, 0.01, MaxDouble);
		RegisterDoubleParameter("sdk20_exposure_compensation", 0.0, 0.01, MaxDouble);
		RegisterDoubleParameter("sdk20_exposure", 10.0, 0.1, MaxDouble);
		RegisterIntParameter("sdk20_white_balance_mode", static_cast<int>(WhiteBalanceMode::Auto), MaxInt);
		RegisterDoubleParameter("sdk20_red_gain", 1.0, 0.01, MaxDouble);
		RegisterDoubleParameter("sdk20_green_gain", 1.0, 0.01, MaxDouble);
		RegisterDoubleParameter("sdk20_blue_gain", 1.0, 0.01, MaxDouble);
		RegisterIntParameter("sdk20_powerline_frequency", static_cast<int>(PowerlineFrequency::Freq50), MaxInt);
		RegisterIntParameter("sdk20_led_nexus_intensity", 100, MaxInt);
		RegisterIntParameter("sdk20_led_privacy_intensity", 100, MaxInt);
	}
	else
		warnlog("failed to open a NuiSensor handle to the Kinect, some functionnality (such as exposure mode control) will be disabled");
#else
	warnlog("obs-kinect-sdk20 backend has been built without NuiSensorLib support, some functionnality (such as exposure mode control) will be disabled");
#endif
}

KinectSdk20Device::~KinectSdk20Device()
{
	// Reset service priority on exit
	SetServicePriority(ProcessPriority::Normal);

#if HAS_NUISENSOR_LIB
	// Reset exposure and white mode to automatic
	if (m_nuiHandle)
	{
		NuiSensorColorCameraSettings cameraSettings;
		cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_EXPOSURE_MODE, 0); //< 0 = fully auto
		cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_WHITE_BALANCE_MODE, 1); //< 1 = auto

		if (!cameraSettings.Execute(m_nuiHandle.get()))
			warnlog("failed to reset camera color settings");
	}
#endif
}

obs_properties_t* KinectSdk20Device::CreateProperties() const
{
	obs_property_t* p;

	obs_properties_t* props = obs_properties_create();
	p = obs_properties_add_list(props, "sdk20_service_priority", Translate("ObsKinectV2.ServicePriority"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, Translate("ObsKinectV2.ServicePriority_High"), static_cast<int>(ProcessPriority::High));
	obs_property_list_add_int(p, Translate("ObsKinectV2.ServicePriority_AboveNormal"), static_cast<int>(ProcessPriority::AboveNormal));
	obs_property_list_add_int(p, Translate("ObsKinectV2.ServicePriority_Normal"), static_cast<int>(ProcessPriority::Normal));

#if HAS_NUISENSOR_LIB
	p = obs_properties_add_list(props, "sdk20_exposure_mode", Translate("ObsKinectV2.ExposureMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, Translate("ObsKinectV2.ExposureControl_FullyAuto"), static_cast<int>(ExposureControl::FullyAuto));
	obs_property_list_add_int(p, Translate("ObsKinectV2.ExposureControl_SemiAuto"), static_cast<int>(ExposureControl::SemiAuto));
	obs_property_list_add_int(p, Translate("ObsKinectV2.ExposureControl_Manual"), static_cast<int>(ExposureControl::Manual));
	
	obs_properties_add_float_slider(props, "sdk20_analog_gain", Translate("ObsKinectV2.AnalogGain"), 1.0, 8.0, 0.1);
	obs_properties_add_float_slider(props, "sdk20_digital_gain", Translate("ObsKinectV2.DigitalGain"), 1.0, 4.0, 0.1);
	obs_properties_add_float_slider(props, "sdk20_exposure_compensation", Translate("ObsKinectV2.ExposureCompensation"), -2.0, 2.0, 0.1);
	obs_properties_add_float_slider(props, "sdk20_exposure", Translate("ObsKinectV2.ExposureTime"), 0.0, 100.0, 1.0);

	p = obs_properties_add_list(props, "sdk20_white_balance_mode", Translate("ObsKinectV2.WhiteBalanceMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, Translate("ObsKinectV2.WhiteBalanceMode_Auto"), static_cast<int>(WhiteBalanceMode::Auto));
	obs_property_list_add_int(p, Translate("ObsKinectV2.WhiteBalanceMode_Manual"), static_cast<int>(WhiteBalanceMode::Manual));
	obs_property_list_add_int(p, Translate("ObsKinectV2.WhiteBalanceMode_Unknown"), static_cast<int>(WhiteBalanceMode::Unknown));

	obs_properties_add_float_slider(props, "sdk20_red_gain", Translate("ObsKinectV2.RedGain"), 1.0, 4.0, 0.1);
	obs_properties_add_float_slider(props, "sdk20_green_gain", Translate("ObsKinectV2.GreenGain"), 1.0, 4.0, 0.1);
	obs_properties_add_float_slider(props, "sdk20_blue_gain", Translate("ObsKinectV2.BlueGain"), 1.0, 4.0, 0.1);

	p = obs_properties_add_list(props, "sdk20_powerline_frequency", Translate("ObsKinectV2.PowerlineFrequency"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, Translate("ObsKinectV2.PowerlineFrequency_50Hz"), static_cast<int>(PowerlineFrequency::Freq50));
	obs_property_list_add_int(p, Translate("ObsKinectV2.PowerlineFrequency_60Hz"), static_cast<int>(PowerlineFrequency::Freq60));
#endif

	return props;
}

bool KinectSdk20Device::MapColorToDepth(const std::uint16_t* depthValues, std::size_t valueCount, std::size_t colorPixelCount, DepthMappingFrameData::DepthCoordinates* depthCoordinatesOut) const
{
	static_assert(sizeof(UINT16) == sizeof(std::uint16_t));
	static_assert(sizeof(DepthMappingFrameData::DepthCoordinates) == sizeof(DepthSpacePoint));

	const UINT16* depthPtr = reinterpret_cast<const UINT16*>(depthValues);
	DepthSpacePoint* coordinatePtr = reinterpret_cast<DepthSpacePoint*>(depthCoordinatesOut);

	HRESULT r = m_coordinateMapper->MapColorFrameToDepthSpace(UINT(valueCount), depthPtr, UINT(colorPixelCount), coordinatePtr);
	if (FAILED(r))
		return false;

	return true;
}

void KinectSdk20Device::SetServicePriority(ProcessPriority priority)
{
	if (s_servicePriority == priority)
		return;

	DWORD priorityClass;
	switch (priority)
	{
		case ProcessPriority::High:        priorityClass = HIGH_PRIORITY_CLASS; break;
		case ProcessPriority::AboveNormal: priorityClass = ABOVE_NORMAL_PRIORITY_CLASS; break;
		case ProcessPriority::Normal:      priorityClass = NORMAL_PRIORITY_CLASS; break;

		default:
			warnlog("unknown process priority %d", int(priority));
			return;
	}

	static bool hasRequestedPrivileges = false;
	if (!hasRequestedPrivileges)
	{
		LUID luid;
		if (!LookupPrivilegeValue(nullptr, SE_INC_BASE_PRIORITY_NAME, &luid))
		{
			warnlog("failed to get privilege SE_INC_BASE_PRIORITY_NAME");
			return;
		}

		TOKEN_PRIVILEGES tkp;
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Luid = luid;
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		HANDLE token;
		if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
		{
			warnlog("failed to open processor token");
			return;
		}
		HandlePtr tokenOwner(token);

		if (!AdjustTokenPrivileges(token, FALSE, &tkp, sizeof(tkp), nullptr, nullptr))
		{
			warnlog("failed to adjust token privileges");
			return;
		}

		infolog("adjusted token privileges successfully");
		hasRequestedPrivileges = true;
	}

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
	{
		warnlog("failed to retrieve processes snapshot");
		return;
	}
	HandlePtr snapshotOwner(snapshot);

	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	if (Process32First(snapshot, &entry))
	{
		do
		{
#ifdef UNICODE
			if (wcscmp(entry.szExeFile, L"KinectService.exe") == 0)
#else
			if (strcmp(entry.szExeFile, "KinectService.exe") == 0)
#endif
			{
				infolog("found KinectService.exe, trying to update its priority...");

				HANDLE process = OpenProcess(PROCESS_SET_INFORMATION, FALSE, entry.th32ProcessID);
				if (!process)
				{
					warnlog("failed to open process");
					return;
				}
				HandlePtr processOwner(process);

				if (!SetPriorityClass(process, priorityClass))
				{
					warnlog("failed to update process priority");
					return;
				}

				s_servicePriority = priority;

				infolog("KinectService.exe priority updated successfully to %s", ProcessPriorityToString(priority));
				return;
			}
		}
		while (Process32Next(snapshot, &entry));
	}

	warnlog("KinectService.exe not found");
}

auto KinectSdk20Device::RetrieveBodyIndexFrame(IMultiSourceFrame* multiSourceFrame) -> BodyIndexFrameData
{
	IBodyIndexFrameReference* pBodyIndexFrameReference;
	if (FAILED(multiSourceFrame->get_BodyIndexFrameReference(&pBodyIndexFrameReference)))
		throw std::runtime_error("Failed to get body index frame reference");

	ReleasePtr<IBodyIndexFrameReference> bodyIndexFrameReference(pBodyIndexFrameReference);

	IBodyIndexFrame* pBodyIndexFrame;
	if (FAILED(pBodyIndexFrameReference->AcquireFrame(&pBodyIndexFrame)))
		throw std::runtime_error("Failed to acquire body index frame");

	ReleasePtr<IBodyIndexFrame> bodyIndexFrame(pBodyIndexFrame);

	IFrameDescription* pBodyIndexFrameDescription;
	if (FAILED(bodyIndexFrame->get_FrameDescription(&pBodyIndexFrameDescription)))
		throw std::runtime_error("Failed to get body index frame description");

	ReleasePtr<IFrameDescription> bodyIndexFrameDescription(pBodyIndexFrameDescription);

	int width;
	int height;
	unsigned int bytePerPixel;

	// srsly microsoft
	if (FAILED(bodyIndexFrameDescription->get_Width(&width)) ||
	    FAILED(bodyIndexFrameDescription->get_Height(&height)) ||
	    FAILED(bodyIndexFrameDescription->get_BytesPerPixel(&bytePerPixel)))
	{
		throw std::runtime_error("Failed to retrieve bodyIndex frame description values");
	}

	if (bytePerPixel != sizeof(BYTE))
		throw std::runtime_error("Unexpected BPP");

	BodyIndexFrameData frameData;
	frameData.memory.resize(width * height * sizeof(BYTE));
	BYTE* memPtr = reinterpret_cast<BYTE*>(frameData.memory.data());

	if (FAILED(bodyIndexFrame->CopyFrameDataToArray(UINT(width * height), memPtr)))
		throw std::runtime_error("Failed to access body index frame buffer");

	frameData.width = width;
	frameData.height = height;
	frameData.pitch = width * bytePerPixel;
	frameData.ptr.reset(frameData.memory.data());

	return frameData;
}

auto KinectSdk20Device::RetrieveColorFrame(IMultiSourceFrame* multiSourceFrame) -> ColorFrameData
{
	ColorFrameData frameData;

	IColorFrameReference* pColorFrameReference;
	if (FAILED(multiSourceFrame->get_ColorFrameReference(&pColorFrameReference)))
		throw std::runtime_error("Failed to get color frame reference");

	ReleasePtr<IColorFrameReference> colorFrameReference(pColorFrameReference);

	IColorFrame* pColorFrame;
	if (FAILED(pColorFrameReference->AcquireFrame(&pColorFrame)))
		throw std::runtime_error("Failed to acquire color frame");

	ReleasePtr<IColorFrame> colorFrame(pColorFrame);

	IFrameDescription* pColorFrameDescription;
	if (FAILED(pColorFrame->get_FrameDescription(&pColorFrameDescription)))
		throw std::runtime_error("Failed to get color frame description");

	ReleasePtr<IFrameDescription> colorFrameDescription(pColorFrameDescription);

	int width;
	int height;
	unsigned int bytePerPixel;
	ColorImageFormat imageFormat;
		
	// srsly microsoft
	if (FAILED(colorFrameDescription->get_Width(&width)) ||
	    FAILED(colorFrameDescription->get_Height(&height)) ||
	    FAILED(colorFrameDescription->get_BytesPerPixel(&bytePerPixel)) ||
	    FAILED(colorFrame->get_RawColorImageFormat(&imageFormat)))
	{
		throw std::runtime_error("Failed to retrieve color frame description values");
	}

	frameData.width = width;
	frameData.height = height;

	// Convert to RGBA
	std::size_t memSize = width * height * 4;
	frameData.memory.resize(memSize);
	std::uint8_t* memPtr = frameData.memory.data();

	if (FAILED(colorFrame->CopyConvertedFrameDataToArray(UINT(memSize), reinterpret_cast<BYTE*>(memPtr), ColorImageFormat_Rgba)))
		throw std::runtime_error("Failed to copy color buffer");

	frameData.ptr.reset(memPtr);
	frameData.pitch = width * 4;
	frameData.format = GS_RGBA;

	return frameData;
}

auto KinectSdk20Device::RetrieveDepthFrame(IMultiSourceFrame* multiSourceFrame) -> DepthFrameData
{
	IDepthFrameReference* pDepthFrameReference;
	if (FAILED(multiSourceFrame->get_DepthFrameReference(&pDepthFrameReference)))
		throw std::runtime_error("Failed to get depth frame reference");

	ReleasePtr<IDepthFrameReference> depthFrameReference(pDepthFrameReference);

	IDepthFrame* pDepthFrame;
	if (FAILED(pDepthFrameReference->AcquireFrame(&pDepthFrame)))
		throw std::runtime_error("Failed to acquire depth frame");

	ReleasePtr<IDepthFrame> depthFrame(pDepthFrame);

	IFrameDescription* pDepthFrameDescription;
	if (FAILED(depthFrame->get_FrameDescription(&pDepthFrameDescription)))
		throw std::runtime_error("Failed to get depth frame description");

	ReleasePtr<IFrameDescription> depthFrameDescription(pDepthFrameDescription);

	int width;
	int height;
	unsigned int bytePerPixel;

	// srsly microsoft
	if (FAILED(depthFrameDescription->get_Width(&width)) ||
	    FAILED(depthFrameDescription->get_Height(&height)) ||
	    FAILED(depthFrameDescription->get_BytesPerPixel(&bytePerPixel)))
	{
		throw std::runtime_error("Failed to retrieve depth frame description values");
	}

	if (bytePerPixel != sizeof(UINT16))
		throw std::runtime_error("Unexpected BPP");

	DepthFrameData frameData;
	frameData.memory.resize(width * height * sizeof(UINT16));
	UINT16* memPtr = reinterpret_cast<UINT16*>(frameData.memory.data());

	if (FAILED(depthFrame->CopyFrameDataToArray(UINT(width * height), memPtr)))
		throw std::runtime_error("Failed to access depth frame buffer");

	frameData.width = width;
	frameData.height = height;
	frameData.pitch = width * bytePerPixel;
	frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));

	return frameData;
}

DepthMappingFrameData KinectSdk20Device::RetrieveDepthMappingFrame(const KinectSdk20Device& device, const ColorFrameData& colorFrame, const DepthFrameData& depthFrame)
{
	DepthMappingFrameData outputFrameData;
	outputFrameData.width = colorFrame.width;
	outputFrameData.height = colorFrame.height;

	std::size_t colorPixelCount = outputFrameData.width * outputFrameData.height;

	const std::uint16_t* depthPtr = reinterpret_cast<const std::uint16_t*>(depthFrame.ptr.get());
	std::size_t depthPixelCount = depthFrame.width * depthFrame.height;

	outputFrameData.memory.resize(colorPixelCount * sizeof(DepthMappingFrameData::DepthCoordinates));

	DepthMappingFrameData::DepthCoordinates* coordinatePtr = reinterpret_cast<DepthMappingFrameData::DepthCoordinates*>(outputFrameData.memory.data());

	if (!device.MapColorToDepth(depthPtr, depthPixelCount, colorPixelCount, coordinatePtr))
		throw std::runtime_error("failed to map color to depth");

	outputFrameData.ptr.reset(coordinatePtr);
	outputFrameData.pitch = colorFrame.width * sizeof(DepthMappingFrameData::DepthCoordinates);

	return outputFrameData;
}

auto KinectSdk20Device::RetrieveInfraredFrame(IMultiSourceFrame* multiSourceFrame) -> InfraredFrameData
{
	IInfraredFrameReference* pInfraredFrameReference;
	if (FAILED(multiSourceFrame->get_InfraredFrameReference(&pInfraredFrameReference)))
		throw std::runtime_error("Failed to get infrared frame reference");

	ReleasePtr<IInfraredFrameReference> infraredFrameReference(pInfraredFrameReference);

	IInfraredFrame* pInfraredFrame;
	if (FAILED(pInfraredFrameReference->AcquireFrame(&pInfraredFrame)))
		throw std::runtime_error("Failed to acquire infrared frame");

	ReleasePtr<IInfraredFrame> infraredFrame(pInfraredFrame);

	IFrameDescription* pInfraredFrameDescription;
	if (FAILED(infraredFrame->get_FrameDescription(&pInfraredFrameDescription)))
		throw std::runtime_error("Failed to get infrared frame description");

	ReleasePtr<IFrameDescription> infraredFrameDescription(pInfraredFrameDescription);

	int width;
	int height;
	unsigned int bytePerPixel;

	// srsly microsoft
	if (FAILED(infraredFrameDescription->get_Width(&width)) ||
	    FAILED(infraredFrameDescription->get_Height(&height)) ||
	    FAILED(infraredFrameDescription->get_BytesPerPixel(&bytePerPixel)))
	{
		throw std::runtime_error("Failed to retrieve infrared frame description values");
	}

	if (bytePerPixel != sizeof(UINT16))
		throw std::runtime_error("Unexpected BPP");

	InfraredFrameData frameData;
	frameData.memory.resize(width * height * sizeof(UINT16));
	UINT16* memPtr = reinterpret_cast<UINT16*>(frameData.memory.data());

	if (FAILED(infraredFrame->CopyFrameDataToArray(UINT(width * height), memPtr)))
		throw std::runtime_error("Failed to access depth frame buffer");

	frameData.width = width;
	frameData.height = height;
	frameData.pitch = width * bytePerPixel;
	frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));

	return frameData;
}

void KinectSdk20Device::HandleDoubleParameterUpdate(const std::string& parameterName, double value)
{
#if HAS_NUISENSOR_LIB
	NuiSensorColorCameraSettings cameraSettings;

	float fValue = float(value);

	if (parameterName == "sdk20_analog_gain")
		cameraSettings.AddCommandFloat(NUISENSOR_RGB_COMMAND_SET_ANALOG_GAIN, fValue);
	else if (parameterName == "sdk20_digital_gain")
		cameraSettings.AddCommandFloat(NUISENSOR_RGB_COMMAND_SET_DIGITAL_GAIN, fValue);
	else if (parameterName == "sdk20_exposure_compensation")
		cameraSettings.AddCommandFloat(NUISENSOR_RGB_COMMAND_SET_EXPOSURE_COMPENSATION, fValue);
	else if (parameterName == "sdk20_exposure")
		// from https://github.com/microsoft/MixedRealityCompanionKit/blob/e01d8e1bf60cd20a62e182610e8a9bfb757a7654/KinectIPD/KinectIPD/KinectExposure.cs#L27
		cameraSettings.AddCommandFloat(NUISENSOR_RGB_COMMAND_SET_EXPOSURE_TIME_MS, 640.f * fValue / 100.f);
	else if (parameterName == "sdk20_red_gain")
		cameraSettings.AddCommandFloat(NUISENSOR_RGB_COMMAND_SET_RED_CHANNEL_GAIN, fValue);
	else if (parameterName == "sdk20_green_gain")
		cameraSettings.AddCommandFloat(NUISENSOR_RGB_COMMAND_SET_GREEN_CHANNEL_GAIN, fValue);
	else if (parameterName == "sdk20_blue_gain")
		cameraSettings.AddCommandFloat(NUISENSOR_RGB_COMMAND_SET_BLUE_CHANNEL_GAIN, fValue);
	else
		errorlog("unhandled parameter %s", parameterName.c_str());

	if (cameraSettings.GetCommandCount() > 0)
	{
		if (cameraSettings.Execute(m_nuiHandle.get()))
		{
			if (!cameraSettings.GetReplyStatus(0))
				errorlog("Kinect refused color camera setting (%s) with value %f");
		}
		else
			errorlog("failed to send color settings to the Kinect");
	}
#endif
}

void KinectSdk20Device::HandleIntParameterUpdate(const std::string& parameterName, long long value)
{
	if (parameterName == "sdk20_service_priority")
		SetServicePriority(static_cast<ProcessPriority>(value));
#if HAS_NUISENSOR_LIB
	else if (parameterName == "sdk20_exposure_mode")
	{
		ExposureControl exposureMode = static_cast<ExposureControl>(value);

		NuiSensorColorCameraSettings cameraSettings;
		cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_ACS, 0); //< I have no idea what this is

		switch (exposureMode)
		{
			case ExposureControl::FullyAuto:
				cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_EXPOSURE_MODE, 0); //< 0 = fully auto
				break;

			case ExposureControl::SemiAuto:
				cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_EXPOSURE_MODE, 3); //< 3 = semi auto
				break;

			case ExposureControl::Manual:
				cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_EXPOSURE_MODE, 4); //< 4 = manual
				break;
		}

		if (cameraSettings.Execute(m_nuiHandle.get()))
		{
			if (!cameraSettings.GetReplyStatus(0))
				errorlog("SET_ACS command failed");

			if (!cameraSettings.GetReplyStatus(1))
				errorlog("SET_EXPOSURE_MODE command failed");
		}
		else
			errorlog("failed to send color settings to the Kinect");
	}
	else if (parameterName == "sdk20_white_balance_mode")
	{
		WhiteBalanceMode whiteBalanceMode = static_cast<WhiteBalanceMode>(value);
		
		NuiSensorColorCameraSettings cameraSettings;

		switch (whiteBalanceMode)
		{
			case WhiteBalanceMode::Auto:
				cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_WHITE_BALANCE_MODE, 1); //< 1 = auto
				break;

			case WhiteBalanceMode::Manual:
				cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_WHITE_BALANCE_MODE, 3); //< 3 = manual
				break;

			case WhiteBalanceMode::Unknown:
				cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_WHITE_BALANCE_MODE, 0); //< 0 = ? (similar to manual but ignores red/green/blue gains)
				break;
		}

		if (cameraSettings.Execute(m_nuiHandle.get()))
		{
			if (!cameraSettings.GetReplyStatus(0))
				errorlog("SET_WHITE_BALANCE_MODE command failed");
		}
		else
			errorlog("failed to send color settings to the Kinect");
	}
	else if (parameterName == "sdk20_powerline_frequency")
	{
		PowerlineFrequency powerlineFrequency = static_cast<PowerlineFrequency>(value);

		NuiSensorColorCameraSettings cameraSettings;

		switch (powerlineFrequency)
		{
			case PowerlineFrequency::Freq50:
				cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_FLICKER_FREE_FREQUENCY, 50);
				break;

			case PowerlineFrequency::Freq60:
				cameraSettings.AddCommand(NUISENSOR_RGB_COMMAND_SET_FLICKER_FREE_FREQUENCY, 60);
				break;
		}

		if (cameraSettings.Execute(m_nuiHandle.get()))
		{
			if (!cameraSettings.GetReplyStatus(0))
				errorlog("SET_FLICKER_FREE_FREQUENCY command failed");
		}
		else
			errorlog("failed to send color settings to the Kinect");
	}
#endif
	else
		errorlog("unhandled parameter %s", parameterName.c_str());
}

void KinectSdk20Device::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& error)
{
	os_set_thread_name("KinectDeviceSdk20");

	ReleasePtr<IMultiSourceFrameReader> multiSourceFrameReader;

	SourceFlags enabledSourceFlags = 0;
	DWORD enabledFrameSourceTypes = 0;

	auto UpdateMultiSourceFrameReader = [&](SourceFlags enabledSources)
	{
		DWORD newFrameSourcesTypes = 0;
		if (enabledSources & Source_Body)
			newFrameSourcesTypes |= FrameSourceTypes_BodyIndex;

		if (enabledSources & (Source_Color | Source_ColorToDepthMapping))
			newFrameSourcesTypes |= FrameSourceTypes_Color;

		if (enabledSources & (Source_Depth | Source_ColorToDepthMapping))
			newFrameSourcesTypes |= FrameSourceTypes_Depth;

		if (enabledSources & Source_Infrared)
			newFrameSourcesTypes |= FrameSourceTypes_Infrared;

		if (!multiSourceFrameReader || newFrameSourcesTypes != enabledFrameSourceTypes)
		{
			IMultiSourceFrameReader* pMultiSourceFrameReader;
			if (FAILED(m_openedKinectSensor->OpenMultiSourceFrameReader(newFrameSourcesTypes, &pMultiSourceFrameReader)))
				throw std::runtime_error("failed to acquire source frame reader");

			multiSourceFrameReader.reset(pMultiSourceFrameReader);
		}

		enabledFrameSourceTypes = newFrameSourcesTypes;
		enabledSourceFlags = enabledSources;

		infolog("Kinect active sources: %s", EnabledSourceToString(enabledSourceFlags).c_str());
	};

	try
	{
		std::array<wchar_t, 256> wideId = { L"<failed to get id>" };
		m_openedKinectSensor->get_UniqueKinectId(UINT(wideId.size()), wideId.data());

		std::array<char, wideId.size()> id = { "<failed to get id>" };
		WideCharToMultiByte(CP_UTF8, 0, wideId.data(), int(wideId.size()), id.data(), int(id.size()), nullptr, nullptr);

		infolog("found kinect sensor (%s)", id.data());
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

	constexpr std::uint64_t MaxKinectFPS = 30;

	uint64_t now = os_gettime_ns();
	uint64_t delay = 1'000'000'000ULL / MaxKinectFPS;

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
					errorlog("%s", e.what());

					os_sleep_ms(10);
					continue;
				}
			}
		}

		if (!multiSourceFrameReader)
		{
			os_sleep_ms(100);
			continue;
		}

		IMultiSourceFrame* pMultiSourceFrame;
		HRESULT acquireResult = multiSourceFrameReader->AcquireLatestFrame(&pMultiSourceFrame);
			
		if (FAILED(acquireResult))
		{
			if (acquireResult == E_PENDING)
			{
				os_sleep_ms(10);
				continue;
			}

			warnlog("failed to acquire latest frame: %d", HRESULT_CODE(acquireResult));
			continue;
		}

		ReleasePtr<IMultiSourceFrame> multiSourceFrame(pMultiSourceFrame);

		try
		{
			KinectFramePtr framePtr = std::make_shared<KinectFrame>();
			if (enabledSourceFlags & Source_Body)
				framePtr->bodyIndexFrame = RetrieveBodyIndexFrame(multiSourceFrame.get());

			if (enabledSourceFlags & (Source_Color | Source_ColorToDepthMapping))
				framePtr->colorFrame = RetrieveColorFrame(multiSourceFrame.get());

			if (enabledSourceFlags & (Source_Depth | Source_ColorToDepthMapping))
				framePtr->depthFrame = RetrieveDepthFrame(multiSourceFrame.get());

			if (enabledSourceFlags & Source_Infrared)
				framePtr->infraredFrame = RetrieveInfraredFrame(multiSourceFrame.get());

			if (enabledSourceFlags & Source_ColorToDepthMapping)
				framePtr->depthMappingFrame = RetrieveDepthMappingFrame(*this, *framePtr->colorFrame, *framePtr->depthFrame);

			UpdateFrame(std::move(framePtr));
			os_sleepto_ns(now += delay);
		}
		catch (const std::exception& e)
		{
			errorlog("%s", e.what());

			// Force sleep to prevent log spamming
			os_sleep_ms(100);
		}
	}

	infolog("exiting thread");
}

ProcessPriority KinectSdk20Device::s_servicePriority = ProcessPriority::Normal;
