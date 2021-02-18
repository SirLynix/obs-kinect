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
#include <comdef.h>
#include <util/threading.h>
#include <array>
#include <sstream>

namespace
{
	template<typename T>
	struct AlwaysFalse : std::false_type {};

	void set_property_visibility(obs_properties_t* props, const char* propertyName, bool visible)
	{
		obs_property_t* property = obs_properties_get(props, propertyName);
		if (property)
			obs_property_set_visible(property, visible);
	}

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
			case E_NUI_HARDWARE_FEATURE_UNAVAILABLE: return "The requested feature is not available on this version of the hardware";
			case E_NUI_NOTCONNECTED: return "The hub is no longer connected to the machine";
			case E_NUI_NOTREADY: return "Some part of the device is not connected";
			case E_NUI_SKELETAL_ENGINE_BUSY: return "Skeletal engine is already in use";
			case E_NUI_NOTPOWERED: return "The hub and motor are connected, but the camera is not";
			default: 
			{
				_com_error err(hr);
				LPCTSTR errMsg = err.ErrorMessage();

				std::string errMessage;
				errMessage.resize(512, ' ');
				errMessage.resize(os_wcs_to_utf8(errMsg, 0, errMessage.data(), errMessage.size()));

				return "Unhandled error (" + errMessage + ")";
			}
		}
	}

	template<typename T>
	struct FirstParamExtractor;

	template<typename O, typename R, typename FirstArg, typename... Args>
	struct FirstParamExtractor<R(O::*)(FirstArg, Args...)>
	{
		using ArgType = FirstArg;
	};

	template<typename O, typename R, typename FirstArg, typename... Args>
	struct FirstParamExtractor<R(O::*)(FirstArg, Args...) const>
	{
		using ArgType = FirstArg;
	};
}

KinectSdk10Device::KinectSdk10Device(int sensorId) :
m_kinectHighRes(false),
m_kinectNearMode(false),
m_kinectElevation(0),
#if HAS_BACKGROUND_REMOVAL
m_trackedSkeleton(NUI_SKELETON_INVALID_TRACKING_ID),
#endif
m_hasColorSettings(false)
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

	SourceFlags supportedSources = Source_Body | Source_Color | Source_ColorToDepthMapping | Source_Depth | Source_Infrared;

#if HAS_BACKGROUND_REMOVAL
	if (Dyn::NuiCreateBackgroundRemovedColorStream)
		supportedSources |= Source_BackgroundRemoval;
#endif

	SetSupportedSources(supportedSources);

	StartElevationThread();
	RegisterParameters();
}

KinectSdk10Device::~KinectSdk10Device()
{
	StopCapture(); //< Ensure capture thread finishes before unloading background removal lib

	m_kinectSensor->NuiSkeletonTrackingDisable();

	SetEvent(m_exitElevationThreadEvent.get());
	m_elevationThread.join();
}

obs_properties_t* KinectSdk10Device::CreateProperties() const
{
	obs_property_t* p;

	// Values obtained by calling GetMin[...] and GetMax[...]
	constexpr double BrignessMax = 1.0;
	constexpr double BrignessMin = 0.0;
	constexpr double ContrastMax = 2.0;
	constexpr double ContrastMin = 0.5;
	constexpr double ExposureMax = 4000.0;
	constexpr double ExposureMin = 1.0;
	constexpr double FrameIntervalMax = 4000.0;
	constexpr double FrameIntervalMin = 0.0;
	constexpr double GainMax = 16.0;
	constexpr double GainMin = 0.0;
	constexpr double GammaMax = 2.7999999999999998;
	constexpr double GammaMin = 1.0;
	constexpr double HueMax = 22.0;
	constexpr double HueMin = -22.0;
	constexpr double SaturationMax = 2.0;
	constexpr double SaturationMin = 0.0;
	constexpr double SharpnessMax = 1.0;
	constexpr double SharpnessMin = 0.0;
	constexpr long long WhiteBalanceMax = 6500;
	constexpr long long WhiteBalanceMin = 2700;

	obs_properties_t* props = obs_properties_create();
	p = obs_properties_add_bool(props, "sdk10_near_mode", Translate("ObsKinectV1.NearMode"));
	obs_property_set_long_description(p, Translate("ObsKinectV1.NearModeDesc"));
	p = obs_properties_add_bool(props, "sdk10_high_res", Translate("ObsKinectV1.HighRes"));
	obs_property_set_long_description(p, Translate("ObsKinectV1.HighResDesc"));

	p = obs_properties_add_int_slider(props, "sdk10_camera_elevation", Translate("ObsKinectV1.CameraElevation"), NUI_CAMERA_ELEVATION_MINIMUM, NUI_CAMERA_ELEVATION_MAXIMUM, 1);
	obs_property_int_set_suffix(p, "°");

	if (m_hasColorSettings)
	{
		p = obs_properties_add_list(props, "sdk10_backlight_compensation", Translate("ObsKinect.BacklightCompensation"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
		obs_property_list_add_int(p, Translate("ObsKinect.BacklightCompensation_AverageBrightness"), static_cast<int>(BacklightCompensation::AverageBrightness));
		obs_property_list_add_int(p, Translate("ObsKinect.BacklightCompensation_CenterOnly"), static_cast<int>(BacklightCompensation::CenterOnly));
		obs_property_list_add_int(p, Translate("ObsKinect.BacklightCompensation_CenterPriority"), static_cast<int>(BacklightCompensation::CenterPriority));
		obs_property_list_add_int(p, Translate("ObsKinect.BacklightCompensation_LowLightsPriority"), static_cast<int>(BacklightCompensation::LowLightsPriority));

		p = obs_properties_add_bool(props, "sdk10_exposure_auto", Translate("ObsKinect.AutoExposure"));
		
		obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
		{
			bool autoExposure = obs_data_get_bool(s, "sdk10_exposure_auto");

			set_property_visibility(props, "sdk10_exposure_time", !autoExposure);
			set_property_visibility(props, "sdk10_frame_interval", !autoExposure);
			set_property_visibility(props, "sdk10_gain", !autoExposure);

			return true;
		});

		obs_properties_add_float_slider(props, "sdk10_exposure_time", Translate("ObsKinect.ExposureTime"), ExposureMin, ExposureMax, 20.0);
		obs_properties_add_float_slider(props, "sdk10_frame_interval", Translate("ObsKinect.FrameInterval"), FrameIntervalMin, FrameIntervalMax, 10.0);
		obs_properties_add_float_slider(props, "sdk10_gain", Translate("ObsKinect.Gain"), GainMin, GainMax, 0.1);

		obs_properties_add_float_slider(props, "sdk10_brightness", Translate("ObsKinect.Brightness"), BrignessMin, BrignessMax, 0.05);
		obs_properties_add_float_slider(props, "sdk10_contrast", Translate("ObsKinect.Contrast"), ContrastMin, ContrastMax, 0.01);
		obs_properties_add_float_slider(props, "sdk10_gamma", Translate("ObsKinect.Gamma"), GammaMin, GammaMax, 0.01);
		obs_properties_add_float_slider(props, "sdk10_hue", Translate("ObsKinect.Hue"), HueMin, HueMax, 0.1);
		obs_properties_add_float_slider(props, "sdk10_saturation", Translate("ObsKinect.Saturation"), SaturationMin, SaturationMax, 0.01);
		obs_properties_add_float_slider(props, "sdk10_sharpness", Translate("ObsKinect.Sharpness"), SharpnessMin, SharpnessMax, 0.01);

		p = obs_properties_add_bool(props, "sdk10_whitebalance_auto", Translate("ObsKinect.AutoWhiteBalance"));
		obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
		{
			bool autoWhiteBalance = obs_data_get_bool(s, "sdk10_whitebalance_auto");

			set_property_visibility(props, "sdk10_whitebalance", !autoWhiteBalance);

			return true;
		});

		obs_properties_add_int_slider(props, "sdk10_whitebalance", Translate("ObsKinect.WhiteBalance"), WhiteBalanceMin, WhiteBalanceMax, 1);

		p = obs_properties_add_list(props, "sdk10_powerline_frequency", Translate("ObsKinect.PowerlineFrequency"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
		obs_property_list_add_int(p, Translate("ObsKinect.PowerlineFrequency_Disabled"), static_cast<int>(PowerlineFrequency::Disabled));
		obs_property_list_add_int(p, Translate("ObsKinect.PowerlineFrequency_50Hz"), static_cast<int>(PowerlineFrequency::Freq50));
		obs_property_list_add_int(p, Translate("ObsKinect.PowerlineFrequency_60Hz"), static_cast<int>(PowerlineFrequency::Freq60));

		obs_properties_add_button2(props, "sdk10_dump", Translate("ObsKinect.DumpCameraSettings"), [](obs_properties_t* /*props*/, obs_property_t* /*property*/, void* data)
		{
			INuiColorCameraSettings* cameraSettings = static_cast<INuiColorCameraSettings*>(data);

			struct CameraSetting
			{
				const char* str;
				std::variant<
					HRESULT(STDMETHODCALLTYPE INuiColorCameraSettings::*)(BOOL*),
					HRESULT(STDMETHODCALLTYPE INuiColorCameraSettings::*)(NUI_BACKLIGHT_COMPENSATION_MODE*),
					HRESULT(STDMETHODCALLTYPE INuiColorCameraSettings::*)(NUI_POWER_LINE_FREQUENCY*),
					HRESULT(STDMETHODCALLTYPE INuiColorCameraSettings::*)(LONG*),
					HRESULT(STDMETHODCALLTYPE INuiColorCameraSettings::*)(double*)
				> method;
			};

			std::array<CameraSetting, 14> settings = {
				{
					{ "automatic exposure", &INuiColorCameraSettings::GetAutoExposure },
					{ "automatic white balance", &INuiColorCameraSettings::GetAutoWhiteBalance },
					{ "backlight compensation", &INuiColorCameraSettings::GetBacklightCompensationMode },
					{ "brightness", &INuiColorCameraSettings::GetBrightness },
					{ "contrast", &INuiColorCameraSettings::GetContrast },
					{ "exposure time", &INuiColorCameraSettings::GetExposureTime },
					{ "frame interval", &INuiColorCameraSettings::GetFrameInterval },
					{ "gain", &INuiColorCameraSettings::GetGain },
					{ "gamma", &INuiColorCameraSettings::GetGamma },
					{ "hue", &INuiColorCameraSettings::GetHue },
					{ "powerline frequency", &INuiColorCameraSettings::GetPowerLineFrequency },
					{ "saturation", &INuiColorCameraSettings::GetSaturation },
					{ "sharpness", &INuiColorCameraSettings::GetSharpness },
					{ "white balance", &INuiColorCameraSettings::GetWhiteBalance }
				}
			};

			std::ostringstream ss;
			ss << "Color settings dump:\n";

			std::size_t commandIndex = 0;
			for (const CameraSetting& setting : settings)
			{
				ss << setting.str << ": ";
				std::visit([&](auto&& arg)
				{
					using T = std::decay_t<decltype(arg)>;
					using ParamType = std::remove_pointer_t<typename FirstParamExtractor<T>::ArgType>;

					ParamType value;
					HRESULT result = (cameraSettings ->* arg)(&value);

					if (SUCCEEDED(result))
					{
						if constexpr (std::is_same_v<ParamType, LONG> || std::is_same_v<ParamType, double>)
							ss << value;
						else if constexpr (std::is_same_v<ParamType, BOOL>)
							ss << ((value) ? "enabled" : "disabled");
						else if constexpr (std::is_same_v<ParamType, NUI_BACKLIGHT_COMPENSATION_MODE>)
						{
							switch (value)
							{
								case NUI_BACKLIGHT_COMPENSATION_MODE_AVERAGE_BRIGHTNESS:
									ss << "average brightness";
									break;

								case NUI_BACKLIGHT_COMPENSATION_MODE_CENTER_PRIORITY:
									ss << "center priority";
									break;

								case NUI_BACKLIGHT_COMPENSATION_MODE_LOWLIGHTS_PRIORITY:
									ss << "lowlights priority";
									break;

								case NUI_BACKLIGHT_COMPENSATION_MODE_CENTER_ONLY:
									ss << "center only";
									break;

								default:
									ss << "unknown backlight compensation mode (" << static_cast<int>(value) << ")";
									break;
							}
						}
						else if constexpr (std::is_same_v<ParamType, NUI_POWER_LINE_FREQUENCY>)
						{
							switch (value)
							{
								case NUI_POWER_LINE_FREQUENCY_DISABLED:
									ss << "disabled";
									break;

								case NUI_POWER_LINE_FREQUENCY_50HZ:
									ss << "50Hz";
									break;

								case NUI_POWER_LINE_FREQUENCY_60HZ:
									ss << "60Hz";
									break;

								default:
									ss << "unknown powerline frequency mode (" << static_cast<int>(value) << ")";
									break;
							}
						}
						else
							static_assert(AlwaysFalse<ParamType>(), "unhandled type");
					}
					else
						ss << "failed to retrieve data (" << ErrToString(result) << ")";

				}, setting.method);

				if (++commandIndex < settings.size())
					ss << '\n';
			}

			infolog("%s", ss.str().c_str());

			return true;
		}, m_cameraSettings.get());
	}

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

				infolog("setting elevation angle to %d", int(newElevation));
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
							warnlog("failed to change Kinect elevation: %s", ErrToString(hr).c_str());
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
	auto LogOnFailure = [&](HRESULT result)
	{
		if (FAILED(result))
			errorlog("failed to update %s to %s: %s", parameterName.c_str(), (value) ? "enabled" : "disabled", ErrToString(result).c_str());
	};

	if (parameterName == "sdk10_exposure_auto")
	{
		assert(m_cameraSettings);
		LogOnFailure(m_cameraSettings->SetAutoExposure(value));
	}
	else if (parameterName == "sdk10_whitebalance_auto")
	{
		assert(m_cameraSettings);
		LogOnFailure(m_cameraSettings->SetAutoWhiteBalance(value));
	}
	else if (parameterName == "sdk10_near_mode")
		m_kinectNearMode.store(value, std::memory_order_relaxed);
	else if (parameterName == "sdk10_high_res")
	{
		m_kinectHighRes.store(value);
		TriggerSourceFlagsUpdate();
	}
	else
		errorlog("unhandled parameter %s", parameterName.c_str());
}

void KinectSdk10Device::HandleDoubleParameterUpdate(const std::string& parameterName, double value)
{
	assert(m_cameraSettings);

	auto LogOnFailure = [&](HRESULT result)
	{
		if (FAILED(result))
			errorlog("failed to update %s to %lf: %s", parameterName.c_str(), value, ErrToString(result).c_str());
	};

	if (parameterName == "sdk10_brightness")
		LogOnFailure(m_cameraSettings->SetBrightness(value));
	else if (parameterName == "sdk10_contrast")
		LogOnFailure(m_cameraSettings->SetContrast(value));
	else if (parameterName == "sdk10_exposure_time")
		LogOnFailure(m_cameraSettings->SetExposureTime(value));
	else if (parameterName == "sdk10_frame_interval")
		LogOnFailure(m_cameraSettings->SetFrameInterval(value));
	else if (parameterName == "sdk10_gain")
		LogOnFailure(m_cameraSettings->SetGain(value));
	else if (parameterName == "sdk10_gamma")
		LogOnFailure(m_cameraSettings->SetGamma(value));
	else if (parameterName == "sdk10_hue")
		LogOnFailure(m_cameraSettings->SetHue(value));
	else if (parameterName == "sdk10_saturation")
		LogOnFailure(m_cameraSettings->SetSaturation(value));
	else if (parameterName == "sdk10_sharpness")
		LogOnFailure(m_cameraSettings->SetSharpness(value));
	else
		errorlog("unhandled parameter %s", parameterName.c_str());
}

void KinectSdk10Device::HandleIntParameterUpdate(const std::string& parameterName, long long value)
{
	auto LogOnFailure = [&](HRESULT result)
	{
		if (FAILED(result))
			errorlog("failed to update %s to %lld: %s", parameterName.c_str(), value, ErrToString(result).c_str());
	};

	if (parameterName == "sdk10_backlight_compensation")
	{
		BacklightCompensation backlightCompensationMode = static_cast<BacklightCompensation>(value);

		assert(m_cameraSettings);
		switch (backlightCompensationMode)
		{
			case BacklightCompensation::AverageBrightness:
				LogOnFailure(m_cameraSettings->SetBacklightCompensationMode(NUI_BACKLIGHT_COMPENSATION_MODE_AVERAGE_BRIGHTNESS));
				break;

			case BacklightCompensation::CenterPriority:
				LogOnFailure(m_cameraSettings->SetBacklightCompensationMode(NUI_BACKLIGHT_COMPENSATION_MODE_CENTER_PRIORITY));
				break;

			case BacklightCompensation::LowLightsPriority:
				LogOnFailure(m_cameraSettings->SetBacklightCompensationMode(NUI_BACKLIGHT_COMPENSATION_MODE_LOWLIGHTS_PRIORITY));
				break;

			case BacklightCompensation::CenterOnly:
				LogOnFailure(m_cameraSettings->SetBacklightCompensationMode(NUI_BACKLIGHT_COMPENSATION_MODE_CENTER_ONLY));
				break;

			default:
				break;
		}
	}
	else if (parameterName == "sdk10_powerline_frequency")
	{
		PowerlineFrequency backlightCompensationMode = static_cast<PowerlineFrequency>(value);

		assert(m_cameraSettings);
		switch (backlightCompensationMode)
		{
			case PowerlineFrequency::Disabled:
				LogOnFailure(m_cameraSettings->SetPowerLineFrequency(NUI_POWER_LINE_FREQUENCY_DISABLED));
				break;

			case PowerlineFrequency::Freq50:
				LogOnFailure(m_cameraSettings->SetPowerLineFrequency(NUI_POWER_LINE_FREQUENCY_50HZ));
				break;

			case PowerlineFrequency::Freq60:
				LogOnFailure(m_cameraSettings->SetPowerLineFrequency(NUI_POWER_LINE_FREQUENCY_60HZ));
				break;

			default:
				break;
		}
	}
	else if (parameterName == "sdk10_whitebalance")
	{
		assert(m_cameraSettings);
		LogOnFailure(m_cameraSettings->SetWhiteBalance(LONG(value)));
	}
	else if (parameterName == "sdk10_camera_elevation")
	{
		m_kinectElevation.store(LONG(value), std::memory_order_relaxed);
		SetEvent(m_elevationUpdateEvent.get());
	}
	else
		errorlog("unhandled parameter %s", parameterName.c_str());
}

void KinectSdk10Device::RegisterParameters()
{
	RegisterIntParameter("sdk10_camera_elevation", 0, [](long long a, long long b)
	{
		if (b == 0)
			return a;

		return b;
	});

	RegisterBoolParameter("sdk10_near_mode", false, [](bool a, bool b) { return a || b; });
	RegisterBoolParameter("sdk10_high_res", false, [](bool a, bool b) { return a || b; });

	INuiColorCameraSettings* pNuiCameraSettings;
	if (SUCCEEDED(m_kinectSensor->NuiGetColorCameraSettings(&pNuiCameraSettings)))
	{
		m_hasColorSettings = true;

		m_cameraSettings.reset(pNuiCameraSettings);

		auto OrBool = [](bool a, bool b)
		{
			return a || b;
		};

		auto MaxDouble = [](double a, double b)
		{
			return std::max(a, b);
		};

		auto MaxInt = [](long long a, long long b)
		{
			return std::max(a, b);
		};

		// Default values taken by reading values after a call to ResetCameraSettingsToDefault
		RegisterIntParameter("sdk10_backlight_compensation", static_cast<int>(BacklightCompensation::AverageBrightness), MaxInt);
		RegisterDoubleParameter("sdk10_brightness", 0.2156, 0.001, MaxDouble);
		RegisterDoubleParameter("sdk10_contrast", 1.0, 0.01, MaxDouble);
		RegisterBoolParameter("sdk10_exposure_auto", true, OrBool);
		RegisterDoubleParameter("sdk10_exposure_time", 4000.0, 1, MaxDouble);
		RegisterDoubleParameter("sdk10_frame_interval", 0.0, 1, MaxDouble);
		RegisterDoubleParameter("sdk10_gain", 1.0, 0.1, MaxDouble);
		RegisterDoubleParameter("sdk10_gamma", 2.2, 0.01, MaxDouble);
		RegisterDoubleParameter("sdk10_hue", 0.0, 0.1, MaxDouble);
		RegisterIntParameter("sdk10_powerline_frequency", static_cast<int>(PowerlineFrequency::Disabled), MaxInt);
		RegisterDoubleParameter("sdk10_saturation", 1.0, 0.01, MaxDouble);
		RegisterDoubleParameter("sdk10_sharpness", 0.5, 0.01, MaxDouble);
		RegisterBoolParameter("sdk10_whitebalance_auto", true, OrBool);
		RegisterIntParameter("sdk10_whitebalance", 2700, MaxInt);
	}
}

void KinectSdk10Device::StartElevationThread()
{
	m_elevationUpdateEvent.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));
	m_exitElevationThreadEvent.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));

	m_elevationThread = std::thread(&KinectSdk10Device::ElevationThreadFunc, this);
}

void KinectSdk10Device::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& /*exceptionPtr*/)
{
	os_set_thread_name("KinectDeviceSdk10");

	HandlePtr colorEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr));
	HandlePtr depthEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr));
	HandlePtr irEvent(CreateEvent(nullptr, TRUE, FALSE, nullptr));

	HANDLE colorStream = INVALID_HANDLE_VALUE;
	HANDLE depthStream = INVALID_HANDLE_VALUE;
	HANDLE irStream = INVALID_HANDLE_VALUE;

	std::int64_t colorTimestamp = 0;
	std::int64_t depthTimestamp = 0;
	std::int64_t irTimestamp = 0;

#if HAS_BACKGROUND_REMOVAL
	HandlePtr backgroundRemovalEvent;
	HandlePtr skeletonEvent;
	if (Dyn::NuiCreateBackgroundRemovedColorStream)
	{
		backgroundRemovalEvent.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));
		skeletonEvent.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));
	}

	ReleasePtr<INuiBackgroundRemovedColorStream> backgroundRemovalStream;
	std::int64_t backgroundRemovalTimestamp = 0;
#endif

	SourceFlags enabledSourceFlags = 0;
	DWORD enabledFrameSourceTypes = 0;

	InitializedNuiSensorPtr<INuiSensor> openedSensor;
	bool colorHighRes = m_kinectHighRes.load();
	bool depthNearMode = false; //< near mode is always enabled after stream retrieval

	auto UpdateKinectStreams = [&](SourceFlags enabledSources)
	{
		bool forceReset = (openedSensor == nullptr);
		DWORD newFrameSourcesTypes = 0;
		if (enabledSources & (Source_Body | Source_BackgroundRemoval))
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

		if (enabledSources & Source_Color)
		{
			bool highRes = m_kinectHighRes.load();
			if (colorHighRes != highRes)
			{
				colorHighRes = highRes;
				forceReset = true;
			}
		}

		if (forceReset || newFrameSourcesTypes != enabledFrameSourceTypes)
		{
			openedSensor.reset(); //< Close sensor first (Kinectv1 doesn't support multiple NuiInitialize)

			HRESULT hr;

			hr = m_kinectSensor->NuiInitialize(newFrameSourcesTypes);
			if (FAILED(hr))
				throw std::runtime_error("failed to initialize Kinect: " + ErrToString(hr));

			InitializedNuiSensorPtr<INuiSensor> tempOpenedSensor(m_kinectSensor.get());

			ResetEvent(colorEvent.get());
			ResetEvent(depthEvent.get());
			ResetEvent(irEvent.get());

			colorStream = INVALID_HANDLE_VALUE;
			depthStream = INVALID_HANDLE_VALUE;
			irStream = INVALID_HANDLE_VALUE;

			if (newFrameSourcesTypes & NUI_INITIALIZE_FLAG_USES_COLOR)
			{
				NUI_IMAGE_RESOLUTION colorRes = (colorHighRes) ? NUI_IMAGE_RESOLUTION_1280x960 : NUI_IMAGE_RESOLUTION_640x480;
				hr = m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR, colorRes, 0, 2, colorEvent.get(), &colorStream);
				if (FAILED(hr))
					throw std::runtime_error("failed to open color stream: " + ErrToString(hr));

				colorTimestamp = 0;
			}

			if (newFrameSourcesTypes & NUI_INITIALIZE_FLAG_USES_DEPTH_AND_PLAYER_INDEX)
			{
				hr = m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH_AND_PLAYER_INDEX, NUI_IMAGE_RESOLUTION_640x480, 0, 2, depthEvent.get(), &depthStream);
				if (FAILED(hr))
					throw std::runtime_error("failed to open body and depth stream: " + ErrToString(hr));

				depthNearMode = false;
				depthTimestamp = 0;
			}
			else if (newFrameSourcesTypes & NUI_INITIALIZE_FLAG_USES_DEPTH)
			{
				hr = m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH, NUI_IMAGE_RESOLUTION_640x480, 0, 2, depthEvent.get(), &depthStream);
				if (FAILED(hr))
					throw std::runtime_error("failed to open depth stream: " + ErrToString(hr));

				depthNearMode = false;
				depthTimestamp = 0;
			}

			if (enabledSources & Source_Infrared)
			{
				hr = m_kinectSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR_INFRARED, NUI_IMAGE_RESOLUTION_640x480, 0, 2, irEvent.get(), &irStream);
				if (FAILED(hr))
					throw std::runtime_error("failed to open infrared stream: " + ErrToString(hr));

				irTimestamp = 0;
			}

#if HAS_BACKGROUND_REMOVAL
			ResetEvent(skeletonEvent.get());
			ResetEvent(backgroundRemovalEvent.get());

			if (enabledSources & Source_BackgroundRemoval && Dyn::NuiCreateBackgroundRemovedColorStream)
			{
				hr = m_kinectSensor->NuiSkeletonTrackingEnable(skeletonEvent.get(), NUI_SKELETON_TRACKING_FLAG_ENABLE_IN_NEAR_RANGE);
				if (FAILED(hr))
					throw std::runtime_error("failed to enable skeleton tracking: " + ErrToString(hr));

				INuiBackgroundRemovedColorStream* backgroundRemovedColorStream;
				hr = Dyn::NuiCreateBackgroundRemovedColorStream(m_kinectSensor.get(), &backgroundRemovedColorStream);
				if (FAILED(hr))
					throw std::runtime_error("failed to create background removing stream: " + ErrToString(hr));

				backgroundRemovalStream.reset(backgroundRemovedColorStream);
				hr = backgroundRemovalStream->Enable(NUI_IMAGE_RESOLUTION_640x480, NUI_IMAGE_RESOLUTION_640x480, backgroundRemovalEvent.get());
				if (FAILED(hr))
					throw std::runtime_error("failed to enable background removing stream: " + ErrToString(hr));

				backgroundRemovalTimestamp = 0;
			}
			else
				m_kinectSensor->NuiSkeletonTrackingDisable();
#endif

			openedSensor = std::move(tempOpenedSensor);
		}

		enabledFrameSourceTypes = newFrameSourcesTypes;
		enabledSourceFlags = enabledSources;

		infolog("Kinect active sources: %s", EnabledSourceToString(enabledSourceFlags).c_str());
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
		if (auto sourceFlagUpdate = GetSourceFlagsUpdate())
		{
			try
			{
				UpdateKinectStreams(sourceFlagUpdate.value());
			}
			catch (const std::exception& e)
			{
				errorlog("%s", e.what());

				os_sleep_ms(10);
				continue;
			}
		}

		if (!openedSensor)
		{
			os_sleep_ms(100);
			continue;
		}

		if (depthStream != INVALID_HANDLE_VALUE)
		{
			bool nearMode = m_kinectNearMode.load(std::memory_order_relaxed);
			if (depthNearMode != nearMode)
			{
				HRESULT hr = m_kinectSensor->NuiImageStreamSetImageFrameFlags(depthStream, (nearMode) ? NUI_IMAGE_STREAM_FLAG_ENABLE_NEAR_MODE : 0);
				if (SUCCEEDED(hr))
					infolog("%s near mode successfully", (nearMode) ? "enabled" : "disabled");
				else
					warnlog("failed to %s near mode: %s", (nearMode) ? "enable" : "disable", ErrToString(hr).c_str());

				depthNearMode = nearMode;
			}
		}

		try
		{
			std::array<HANDLE, 5> events;
			DWORD eventCount = 0;

			if (enabledSourceFlags & Source_Color)
				events[eventCount++] = colorEvent.get();

			if (enabledSourceFlags & (Source_Body | Source_Depth | Source_ColorToDepthMapping))
				events[eventCount++] = depthEvent.get();

			if (enabledSourceFlags & Source_Infrared)
				events[eventCount++] = irEvent.get();

#if HAS_BACKGROUND_REMOVAL
			if (enabledSourceFlags & Source_BackgroundRemoval)
			{
				events[eventCount++] = skeletonEvent.get();
				events[eventCount++] = backgroundRemovalEvent.get();
			}
#endif

			WaitForMultipleObjects(eventCount, events.data(), FALSE, 100);

			// Check if color frame is available
			if ((enabledSourceFlags & Source_Color) && 
			    WaitForSingleObject(colorEvent.get(), 0) == WAIT_OBJECT_0)
			{
				try
				{
					nextFramePtr->colorFrame = RetrieveColorFrame(openedSensor.get(), colorStream, &colorTimestamp);

#if HAS_BACKGROUND_REMOVAL
					if (enabledSourceFlags & Source_BackgroundRemoval)
					{
						UINT byteCount = nextFramePtr->colorFrame->pitch * nextFramePtr->colorFrame->height;
						LARGE_INTEGER time;
						time.QuadPart = colorTimestamp;
						HRESULT hr = backgroundRemovalStream->ProcessColor(byteCount, reinterpret_cast<const BYTE*>(nextFramePtr->colorFrame->ptr.get()), time);
						if (FAILED(hr))
							warnlog("dedicated background removal: failed to process color: %s", ErrToString(hr).c_str());
					}
#endif
				}
				catch (const std::exception& e)
				{
					warnlog("failed to retrieve color frame: %s", e.what());
				}
			}

			if ((enabledSourceFlags & (Source_Depth | Source_ColorToDepthMapping)) && 
			    WaitForSingleObject(depthEvent.get(), 0) == WAIT_OBJECT_0)
			{
				try
				{
					ImageFrameCallback callback;
#if HAS_BACKGROUND_REMOVAL
					if (enabledSourceFlags & Source_BackgroundRemoval)
					{
						callback = [&](NUI_IMAGE_FRAME& depthImageFrame)
						{
							try
							{
								HRESULT hr;

								BOOL nearMode = TRUE;
								INuiFrameTexture* pTexture;

								hr = m_kinectSensor->NuiImageFrameGetDepthImagePixelFrameTexture(depthStream, &depthImageFrame, &nearMode, &pTexture);
								if (FAILED(hr))
									throw std::runtime_error("failed to get depth image pixel frame texture");

								ReleasePtr<INuiFrameTexture> texture(pTexture);

								NUI_LOCKED_RECT lockedRect;

								hr = texture->LockRect(0, &lockedRect, nullptr, 0);
								if (FAILED(hr))
									throw std::runtime_error("failed to lock texture: " + ErrToString(hr));

								auto UnlockRect = [](INuiFrameTexture* texture) { texture->UnlockRect(0); };
								std::unique_ptr<INuiFrameTexture, decltype(UnlockRect)> unlockRect(texture.get(), UnlockRect);

								hr = backgroundRemovalStream->ProcessDepth(lockedRect.size, lockedRect.pBits, depthImageFrame.liTimeStamp);
								if (FAILED(hr))
									throw std::runtime_error("failed to process depth");
							}
							catch (const std::exception& e)
							{
								warnlog("dedicated background removal: %s", e.what());
							}
						};
					}
#endif

					nextFramePtr->depthFrame = RetrieveDepthFrame(openedSensor.get(), depthStream, &depthTimestamp, callback);
				}
				catch (const std::exception& e)
				{
					warnlog("failed to retrieve depth frame: %s", e.what());
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
					warnlog("failed to retrieve infrared frame: %s", e.what());
				}
			}

#if HAS_BACKGROUND_REMOVAL
			if (enabledSourceFlags & Source_BackgroundRemoval)
			{
				if (WaitForSingleObject(skeletonEvent.get(), 0) == WAIT_OBJECT_0)
				{
					try
					{
						HRESULT hr;

						NUI_SKELETON_FRAME skeletonFrame;
						hr = m_kinectSensor->NuiSkeletonGetNextFrame(0, &skeletonFrame);
						if (FAILED(hr))
							throw std::runtime_error("failed to access next frame: " + ErrToString(hr));

						DWORD bestSkeleton = ChooseSkeleton(skeletonFrame, m_trackedSkeleton);
						if (bestSkeleton != m_trackedSkeleton && bestSkeleton != NUI_SKELETON_INVALID_TRACKING_ID)
						{
							infolog("dedicated background removal: now tracking player %lu", static_cast<unsigned long>(bestSkeleton));
							hr = backgroundRemovalStream->SetTrackedPlayer(bestSkeleton);
							if (FAILED(hr))
								throw std::runtime_error("failed to set tracked player: " + ErrToString(hr));

							m_trackedSkeleton = bestSkeleton;
						}

						hr = backgroundRemovalStream->ProcessSkeleton(NUI_SKELETON_COUNT, skeletonFrame.SkeletonData, skeletonFrame.liTimeStamp);
						if (FAILED(hr))
							warnlog("dedicated background removal: failed to process skeleton: %s", ErrToString(hr).c_str());
					}
					catch (const std::exception& e)
					{
						warnlog("failed to retrieve skeleton frame: %s", e.what());
					}
				}

				if (WaitForSingleObject(backgroundRemovalEvent.get(), 0) == WAIT_OBJECT_0)
				{
					try
					{
						nextFramePtr->backgroundRemovalFrame = RetrieveBackgroundRemovalFrame(backgroundRemovalStream.get(), &backgroundRemovalTimestamp);
					}
					catch (const std::exception& e)
					{
						warnlog("failed to retrieve background removed frame: %s", e.what());
					}
				}
			}
#endif

			bool canUpdateFrame = true;

			// Check all timestamp belongs to the same timeframe
			std::array<std::int64_t, 4> timestamps;
			std::size_t timestampCount = 0;

			if (enabledSourceFlags & Source_Color)
				timestamps[timestampCount++] = colorTimestamp;

			if (enabledSourceFlags & (Source_Body | Source_Depth | Source_ColorToDepthMapping))
				timestamps[timestampCount++] = depthTimestamp;

			if (enabledSourceFlags & Source_Infrared)
				timestamps[timestampCount++] = irTimestamp;

#if HAS_BACKGROUND_REMOVAL
			if (enabledSourceFlags & Source_BackgroundRemoval)
				timestamps[timestampCount++] = backgroundRemovalTimestamp;
#endif

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
					
					// "Fix" depth frame by removing body information
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
			errorlog("%s", e.what());

			// Force sleep to prevent log spamming
			os_sleep_ms(100);
		}
	}

	infolog("exiting thread");
}

DepthMappingFrameData KinectSdk10Device::BuildDepthMappingFrame(INuiSensor* /*sensor*/, const ColorFrameData& colorFrame, const DepthFrameData& depthFrame, std::vector<std::uint8_t>& tempMemory)
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

#if HAS_BACKGROUND_REMOVAL
BackgroundRemovalFrameData KinectSdk10Device::RetrieveBackgroundRemovalFrame(INuiBackgroundRemovedColorStream* backgroundRemovalStream, std::int64_t* timestamp)
{
	HRESULT hr;

	NUI_BACKGROUND_REMOVED_COLOR_FRAME backgroundRemovedColorFrame;
	hr = backgroundRemovalStream->GetNextFrame(0, &backgroundRemovedColorFrame);
	if (FAILED(hr))
		throw std::runtime_error("failed to access next frame: " + ErrToString(hr));

	auto ReleaseColorFrame = [&](NUI_BACKGROUND_REMOVED_COLOR_FRAME* frame) { backgroundRemovalStream->ReleaseFrame(frame); };
	std::unique_ptr<NUI_BACKGROUND_REMOVED_COLOR_FRAME, decltype(ReleaseColorFrame)> releaseColor(&backgroundRemovedColorFrame, ReleaseColorFrame);

	BackgroundRemovalFrameData frameData;
	ConvertResolutionToSize(backgroundRemovedColorFrame.backgroundRemovedColorFrameResolution, frameData);

	constexpr std::size_t bpp = 1; //< Background Removal is A8

	std::size_t memSize = frameData.width * frameData.height * bpp;
	frameData.memory.resize(memSize);
	std::uint8_t* memPtr = frameData.memory.data();

	frameData.ptr.reset(memPtr);
	frameData.pitch = frameData.width * bpp;

	const BYTE* inputPtr = backgroundRemovedColorFrame.pBackgroundRemovedColorData;

	for (std::size_t y = 0; y < frameData.height; ++y)
	{
		for (std::size_t x = 0; x < frameData.width; ++x)
		{
			// Background removed color frame is BGRA, keep only alpha
			*memPtr++ = inputPtr[3];
			inputPtr += 4;
		}
	}

	if (timestamp)
		*timestamp = backgroundRemovedColorFrame.liTimeStamp.QuadPart;

	return frameData;
}

DWORD KinectSdk10Device::ChooseSkeleton(const NUI_SKELETON_FRAME& skeletonFrame, DWORD currentSkeleton)
{
	float bestSkeletonDistance = std::numeric_limits<float>::max();
	DWORD bestSkeletonId = NUI_SKELETON_INVALID_TRACKING_ID;

	for (const NUI_SKELETON_DATA& skeleton : skeletonFrame.SkeletonData)
	{
		if (skeleton.eTrackingState == NUI_SKELETON_TRACKED)
		{
			if (currentSkeleton == skeleton.dwTrackingID)
				return currentSkeleton;

			if (skeleton.Position.z < bestSkeletonDistance)
			{
				bestSkeletonDistance = skeleton.Position.z;
				bestSkeletonId = skeleton.dwTrackingID;
			}
		}
	}

	return bestSkeletonId;
}
#endif

ColorFrameData KinectSdk10Device::RetrieveColorFrame(INuiSensor* sensor, HANDLE colorStream, std::int64_t* timestamp, const ImageFrameCallback& rawframeOp)
{
	HRESULT hr;

	NUI_IMAGE_FRAME colorFrame;

	assert(colorStream != INVALID_HANDLE_VALUE);
	hr = sensor->NuiImageStreamGetNextFrame(colorStream, 1, &colorFrame);
	if (FAILED(hr))
		throw std::runtime_error("failed to access next frame: " + ErrToString(hr));

	auto ReleaseColorFrame = [&](NUI_IMAGE_FRAME* frame) { sensor->NuiImageStreamReleaseFrame(colorStream, frame); };
	std::unique_ptr<NUI_IMAGE_FRAME, decltype(ReleaseColorFrame)> releaseColor(&colorFrame, ReleaseColorFrame);

	if (rawframeOp)
		rawframeOp(colorFrame);

	INuiFrameTexture* texture = colorFrame.pFrameTexture;

	NUI_LOCKED_RECT lockedRect;
	
	hr = texture->LockRect(0, &lockedRect, nullptr, 0);
	if (FAILED(hr))
		throw std::runtime_error("failed to lock texture: " + ErrToString(hr));

	auto UnlockRect = [](INuiFrameTexture* texture) { texture->UnlockRect(0); };
	std::unique_ptr<INuiFrameTexture, decltype(UnlockRect)> unlockRect(texture, UnlockRect);

	if (lockedRect.Pitch <= 0)
		throw std::runtime_error("invalid texture pitch (<= 0)");

	std::uint32_t texturePitch = static_cast<std::uint32_t>(lockedRect.Pitch);

	ColorFrameData frameData;
	ConvertResolutionToSize(colorFrame.eResolution, frameData);

	constexpr std::size_t bpp = 4; //< Color is stored as BGRA8

	std::size_t memSize = frameData.width * frameData.height * bpp;
	frameData.memory.resize(memSize);
	std::uint8_t* memPtr = frameData.memory.data();

	frameData.ptr.reset(memPtr);
	frameData.pitch = frameData.width * bpp;
	frameData.format = GS_BGRA;

	if (frameData.pitch == texturePitch)
		std::memcpy(memPtr, lockedRect.pBits, frameData.pitch * frameData.height);
	else
	{
		std::uint32_t bestPitch = std::min(frameData.pitch, texturePitch);
		for (std::size_t y = 0; y < frameData.height; ++y)
		{
			const std::uint8_t* input = &lockedRect.pBits[y * texturePitch];
			std::uint8_t* output = memPtr + y * frameData.pitch;

			std::memcpy(output, input, bestPitch);
		}
	}

	// Fix alpha (color frame alpha is at zero, because reasons)
	for (std::size_t y = 0; y < frameData.height; ++y)
	{
		for (std::size_t x = 0; x < frameData.width; ++x)
		{
			std::uint8_t* ptr = &memPtr[y * frameData.pitch + x * bpp];
			ptr[3] = 255;
		}
	}

	if (timestamp)
		*timestamp = colorFrame.liTimeStamp.QuadPart;

	return frameData;
}

DepthFrameData KinectSdk10Device::RetrieveDepthFrame(INuiSensor* sensor, HANDLE depthStream, std::int64_t* timestamp, const ImageFrameCallback& rawframeOp)
{
	HRESULT hr;

	NUI_IMAGE_FRAME depthFrame;

	assert(depthStream != INVALID_HANDLE_VALUE);
	hr = sensor->NuiImageStreamGetNextFrame(depthStream, 1, &depthFrame);
	if (FAILED(hr))
		throw std::runtime_error("failed to access next frame: " + ErrToString(hr));

	auto ReleaseColorFrame = [&](NUI_IMAGE_FRAME* frame) { sensor->NuiImageStreamReleaseFrame(depthStream, frame); };
	std::unique_ptr<NUI_IMAGE_FRAME, decltype(ReleaseColorFrame)> releaseColor(&depthFrame, ReleaseColorFrame);

	if (rawframeOp)
		rawframeOp(depthFrame);

	INuiFrameTexture* texture = depthFrame.pFrameTexture;

	NUI_LOCKED_RECT lockedRect;

	hr = texture->LockRect(0, &lockedRect, nullptr, 0);
	if (FAILED(hr))
		throw std::runtime_error("failed to lock texture: " + ErrToString(hr));

	auto UnlockRect = [](INuiFrameTexture* texture) { texture->UnlockRect(0); };
	std::unique_ptr<INuiFrameTexture, decltype(UnlockRect)> unlockRect(texture, UnlockRect);

	if (lockedRect.Pitch <= 0)
		throw std::runtime_error("invalid texture pitch (<= 0)");

	std::uint32_t texturePitch = static_cast<std::uint32_t>(lockedRect.Pitch);

	DepthFrameData frameData;
	ConvertResolutionToSize(depthFrame.eResolution, frameData);

	constexpr std::size_t bpp = 2; //< Depth is stored as RG16 (depth and player index combined)

	std::size_t memSize = frameData.width * frameData.height * bpp;
	frameData.memory.resize(memSize);
	std::uint8_t* memPtr = frameData.memory.data();

	frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(memPtr));
	frameData.pitch = frameData.width * bpp;

	if (frameData.pitch == texturePitch)
		std::memcpy(memPtr, lockedRect.pBits, frameData.pitch * frameData.height);
	else
	{
		std::uint32_t bestPitch = std::min(frameData.pitch, texturePitch);
		for (std::size_t y = 0; y < frameData.height; ++y)
		{
			const std::uint8_t* input = &lockedRect.pBits[y * texturePitch];
			std::uint8_t* output = memPtr + y * frameData.pitch;

			std::memcpy(output, input, bestPitch);
		}
	}

	if (timestamp)
		*timestamp = depthFrame.liTimeStamp.QuadPart;

	return frameData;
}

InfraredFrameData KinectSdk10Device::RetrieveInfraredFrame(INuiSensor* sensor, HANDLE irStream, std::int64_t* timestamp, const ImageFrameCallback& rawframeOp)
{
	HRESULT hr;

	NUI_IMAGE_FRAME irFrame;

	assert(irStream != INVALID_HANDLE_VALUE);
	hr = sensor->NuiImageStreamGetNextFrame(irStream, 1, &irFrame);
	if (FAILED(hr))
		throw std::runtime_error("failed to access next frame: " + ErrToString(hr));

	auto ReleaseColorFrame = [&](NUI_IMAGE_FRAME* frame) { sensor->NuiImageStreamReleaseFrame(irStream, frame); };
	std::unique_ptr<NUI_IMAGE_FRAME, decltype(ReleaseColorFrame)> releaseColor(&irFrame, ReleaseColorFrame);

	if (rawframeOp)
		rawframeOp(irFrame);

	INuiFrameTexture* texture = irFrame.pFrameTexture;

	NUI_LOCKED_RECT lockedRect;

	hr = texture->LockRect(0, &lockedRect, nullptr, 0);
	if (FAILED(hr))
		throw std::runtime_error("failed to lock texture: " + ErrToString(hr));

	auto UnlockRect = [](INuiFrameTexture* texture) { texture->UnlockRect(0); };
	std::unique_ptr<INuiFrameTexture, decltype(UnlockRect)> unlockRect(texture, UnlockRect);

	if (lockedRect.Pitch <= 0)
		throw std::runtime_error("invalid texture pitch (<= 0)");

	std::uint32_t texturePitch = static_cast<std::uint32_t>(lockedRect.Pitch);

	InfraredFrameData frameData;
	ConvertResolutionToSize(irFrame.eResolution, frameData);

	constexpr std::size_t bpp = 2; //< Infrared is stored as RG16

	std::size_t memSize = frameData.width * frameData.height * bpp;
	frameData.memory.resize(memSize);
	std::uint8_t* memPtr = frameData.memory.data();

	frameData.ptr.reset(reinterpret_cast<std::uint16_t*>(frameData.memory.data()));
	frameData.pitch = frameData.width * bpp;

	if (frameData.pitch == texturePitch)
		std::memcpy(memPtr, lockedRect.pBits, frameData.pitch * frameData.height);
	else
	{
		std::uint32_t bestPitch = std::min(frameData.pitch, texturePitch);
		for (std::size_t y = 0; y < frameData.height; ++y)
		{
			const std::uint8_t* input = &lockedRect.pBits[y * texturePitch];
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
