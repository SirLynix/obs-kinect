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

#include "AzureKinectDevice.hpp"
#include "AzureKinectPlugin.hpp"
#include <util/threading.h>
#include <array>
#include <optional>
#include <sstream>
#include <tlhelp32.h>

#if HAS_BODY_TRACKING
#include <k4abt.hpp>
#endif

namespace
{
	void set_property_visibility(obs_properties_t* props, const char* propertyName, bool visible)
	{
		obs_property_t* property = obs_properties_get(props, propertyName);
		if (property)
			obs_property_set_visible(property, visible);
	}

	k4a_device_configuration_t BuildConfiguration(SourceFlags enabledSources, ColorResolution colorRes, DepthMode depthMode)
	{
		k4a_device_configuration_t deviceConfig;
		deviceConfig.wired_sync_mode = K4A_WIRED_SYNC_MODE_STANDALONE;
		deviceConfig.subordinate_delay_off_master_usec = 0;
		deviceConfig.disable_streaming_indicator = false;

		if (enabledSources & Source_Color)
		{
			deviceConfig.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
			switch (colorRes)
			{
				case ColorResolution::R1280x720:
					deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_720P;
					break;

				default:
				case ColorResolution::R1920x1080:
					deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_1080P;
					break;

				case ColorResolution::R2560x1440:
					deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_1440P;
					break;

				case ColorResolution::R2048x1536:
					deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_1536P;
					break;

				case ColorResolution::R3840x2160:
					deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_2160P;
					break;

				case ColorResolution::R4096x3072:
					deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_3072P;
					break;
			}
		}
		else
		{
			deviceConfig.color_format = K4A_IMAGE_FORMAT_COLOR_BGRA32;
			deviceConfig.color_resolution = K4A_COLOR_RESOLUTION_OFF;
		}

		if (enabledSources & (Source_Body | Source_Depth | Source_ColorToDepthMapping | Source_Infrared))
		{
			switch (depthMode)
			{
				case DepthMode::Passive:
					// Passive IR cannot be used to read depth values, use it when infrared only is requested
					if ((enabledSources & (Source_Depth | Source_ColorToDepthMapping)) == 0)
					{
						deviceConfig.depth_mode = K4A_DEPTH_MODE_PASSIVE_IR;
						break;
					}
					else
						[[fallthrough]];

				default:
				case DepthMode::NFOVUnbinned:
					deviceConfig.depth_mode = K4A_DEPTH_MODE_NFOV_UNBINNED;
					break;

				case DepthMode::NFOV2x2Binned:
					deviceConfig.depth_mode = K4A_DEPTH_MODE_NFOV_2X2BINNED;
					break;

				case DepthMode::WFOVUnbinned:
					deviceConfig.depth_mode = K4A_DEPTH_MODE_WFOV_UNBINNED;
					break;

				case DepthMode::WFOV2x2Binned:
					deviceConfig.depth_mode = K4A_DEPTH_MODE_WFOV_2X2BINNED;
					break;
			}
		}
		else
			deviceConfig.depth_mode = K4A_DEPTH_MODE_OFF;

		if ((enabledSources & Source_Color) && (enabledSources & Source_Depth))
		{
			deviceConfig.synchronized_images_only = true;
			deviceConfig.depth_delay_off_color_usec = 0;
		}
		else
		{
			deviceConfig.synchronized_images_only = false;
			deviceConfig.depth_delay_off_color_usec = 0;
		}

		if (deviceConfig.depth_mode == K4A_DEPTH_MODE_WFOV_UNBINNED || deviceConfig.color_resolution == K4A_COLOR_RESOLUTION_3072P)
			deviceConfig.camera_fps = K4A_FRAMES_PER_SECOND_15;
		else
			deviceConfig.camera_fps = K4A_FRAMES_PER_SECOND_30;

		return deviceConfig;
	}

	bool CompareConfig(const k4a_device_configuration_t& lhs, const k4a_device_configuration_t& rhs)
	{
		if (lhs.color_resolution != rhs.color_resolution)
			return false;

		if (lhs.depth_mode != rhs.depth_mode)
			return false;

		if (lhs.camera_fps != rhs.camera_fps)
			return false;

		if (lhs.synchronized_images_only != rhs.synchronized_images_only)
			return false;

		if (lhs.depth_delay_off_color_usec != rhs.depth_delay_off_color_usec)
			return false;

		if (lhs.wired_sync_mode != rhs.wired_sync_mode)
			return false;

		if (lhs.subordinate_delay_off_master_usec != rhs.subordinate_delay_off_master_usec)
			return false;

		if (lhs.disable_streaming_indicator != rhs.disable_streaming_indicator)
			return false;

		return true;
	}
}

AzureKinectDevice::AzureKinectDevice(std::uint32_t deviceIndex) :
m_colorResolution(ColorResolution::R1920x1080),
m_depthMode(DepthMode::NFOVUnbinned)
{
	m_device = k4a::device::open(deviceIndex);

	SetUniqueName("#" + std::to_string(deviceIndex) + ": " + m_device.get_serialnum());

	SourceFlags supportedSources = Source_Color | Source_Depth | Source_Infrared | Source_ColorToDepthMapping;

#if HAS_BODY_TRACKING
	if (IsBodyTrackingSdkLoaded())
		supportedSources |= Source_Body;
#endif

	SetSupportedSources(supportedSources);

	auto OrBool = [](bool a, bool b)
	{
		return a || b;
	};

	auto MaxInt = [](long long a, long long b)
	{
		return std::max(a, b);
	};

	// Default values from https://github.com/microsoft/Azure-Kinect-Sensor-SDK/blob/master/tools/k4aviewer/k4adevicedockcontrol.cpp#L194
	RegisterIntParameter("azuresdk_color_resolution", static_cast<long long>(m_colorResolution.load()), MaxInt);
	RegisterIntParameter("azuresdk_depth_mode", static_cast<long long>(m_depthMode.load()), MaxInt);
	RegisterBoolParameter("azuresdk_exposure_auto", true, OrBool);
	RegisterIntParameter("azuresdk_exposure_time", 15625, MaxInt);
	RegisterBoolParameter("azuresdk_whitebalance_auto", true, OrBool);
	RegisterIntParameter("azuresdk_whitebalance", 4500, MaxInt);
	RegisterIntParameter("azuresdk_brightness", 128, MaxInt);
	RegisterIntParameter("azuresdk_contrast", 5, MaxInt);
	RegisterIntParameter("azuresdk_saturation", 32, MaxInt);
	RegisterIntParameter("azuresdk_sharpness", 2, MaxInt);
	RegisterIntParameter("azuresdk_gain", 0, MaxInt);
	RegisterBoolParameter("azuresdk_backlightcompensation", false, OrBool);
	RegisterIntParameter("azuresdk_powerline_frequency", static_cast<int>(PowerlineFrequency::Freq60), MaxInt);
}

AzureKinectDevice::~AzureKinectDevice()
{
	StopCapture(); //< Ensure thread has joined before closing the device
}

obs_properties_t* AzureKinectDevice::CreateProperties() const
{
	obs_property_t* p;

	obs_properties_t* props = obs_properties_create();

	p = obs_properties_add_list(props, "azuresdk_color_resolution", Translate("ObsKinectAzure.ColorResolution"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, Translate("ObsKinectAzure.ColorResolution_1280x720"),  static_cast<int>(ColorResolution::R1280x720));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.ColorResolution_1920x1080"), static_cast<int>(ColorResolution::R1920x1080));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.ColorResolution_2560x1440"), static_cast<int>(ColorResolution::R2560x1440));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.ColorResolution_2048x1536"), static_cast<int>(ColorResolution::R2048x1536));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.ColorResolution_3840x2160"), static_cast<int>(ColorResolution::R3840x2160));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.ColorResolution_4096x3072"), static_cast<int>(ColorResolution::R4096x3072));

	p = obs_properties_add_list(props, "azuresdk_depth_mode", Translate("ObsKinectAzure.DepthMode"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, Translate("ObsKinectAzure.DepthMode_NFOV_Unbinned"),  static_cast<int>(DepthMode::NFOVUnbinned));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.DepthMode_NFOV_2x2Binned"), static_cast<int>(DepthMode::NFOV2x2Binned));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.DepthMode_WFOV_Unbinned"),  static_cast<int>(DepthMode::WFOVUnbinned));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.DepthMode_WFOV_2x2Binned"), static_cast<int>(DepthMode::WFOV2x2Binned));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.DepthMode_Passive"),        static_cast<int>(DepthMode::Passive));

	p = obs_properties_add_bool(props, "azuresdk_exposure_auto", Translate("ObsKinectAzure.AutoExposure"));
	
	obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
	{
		bool autoExposure = obs_data_get_bool(s, "azuresdk_exposure_auto");

		set_property_visibility(props, "azuresdk_exposure_time", !autoExposure);

		return true;
	});

	obs_properties_add_int_slider(props, "azuresdk_exposure_time", Translate("ObsKinectAzure.ExposureTime"), 488, 1000000 / 30, 8);

	p = obs_properties_add_bool(props, "azuresdk_whitebalance_auto", Translate("ObsKinectAzure.AutoWhiteBalance"));
	
	obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
	{
		bool autoWhiteBalance = obs_data_get_bool(s, "azuresdk_whitebalance_auto");

		set_property_visibility(props, "azuresdk_whitebalance", !autoWhiteBalance);

		return true;
	});

	p = obs_properties_add_int_slider(props, "azuresdk_whitebalance", Translate("ObsKinectAzure.WhiteBalance"), 2500, 12500, 1);
	obs_property_int_set_suffix(p, "K");

	obs_properties_add_int_slider(props, "azuresdk_brightness", Translate("ObsKinectAzure.Brightness"), 0, 255, 1);
	obs_properties_add_int_slider(props, "azuresdk_contrast", Translate("ObsKinectAzure.Contrast"), 0, 10, 1);
	obs_properties_add_int_slider(props, "azuresdk_saturation", Translate("ObsKinectAzure.Saturation"), 0, 63, 1);
	obs_properties_add_int_slider(props, "azuresdk_sharpness", Translate("ObsKinectAzure.Saturation"), 0, 4, 1);
	obs_properties_add_int_slider(props, "azuresdk_gain", Translate("ObsKinectAzure.Gain"), 0, 255, 1);
	obs_properties_add_bool(props, "azuresdk_backlightcompensation", Translate("ObsKinectAzure.BacklightCompensation"));

	p = obs_properties_add_list(props, "azuresdk_powerline_frequency", Translate("ObsKinectAzure.PowerlineFrequency"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, Translate("ObsKinectAzure.PowerlineFrequency_50Hz"), static_cast<int>(PowerlineFrequency::Freq50));
	obs_property_list_add_int(p, Translate("ObsKinectAzure.PowerlineFrequency_60Hz"), static_cast<int>(PowerlineFrequency::Freq60));

	obs_properties_add_button2(props, "azuresdk_dump", Translate("ObsKinectAzure.DumpCameraSettings"), [](obs_properties_t* props, obs_property_t* property, void* data)
	{
		k4a_device_t device = static_cast<k4a_device_t>(data);

		std::ostringstream ss;
		ss << "Color settings dump:\n";

		auto PrintValue = [&](const char* setting, k4a_color_control_command_t command, auto&& printCallback)
		{
			ss << setting << ": ";

			k4a_color_control_mode_t mode;
			std::int32_t value;
			k4a_result_t result = k4a_device_get_color_control(device, command, &mode, &value);

			if (result == K4A_RESULT_SUCCEEDED)
			{
				if (mode == K4A_COLOR_CONTROL_MODE_AUTO)
					ss << "<automatic>";
				else
					printCallback(value);
			}
			else
				ss << "failed to retrieve data (an error occurred)";

			ss << '\n';
		};

		auto DefaultPrint = [&](std::int32_t value)
		{
			ss << value;
		};

		PrintValue("brightness", K4A_COLOR_CONTROL_BRIGHTNESS, DefaultPrint);
		PrintValue("contrast", K4A_COLOR_CONTROL_CONTRAST, DefaultPrint);
		PrintValue("exposure time", K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE, DefaultPrint);
		PrintValue("gain", K4A_COLOR_CONTROL_GAIN, DefaultPrint);
		PrintValue("saturation", K4A_COLOR_CONTROL_SATURATION, DefaultPrint);
		PrintValue("sharpness", K4A_COLOR_CONTROL_SHARPNESS, DefaultPrint);
		PrintValue("white balance", K4A_COLOR_CONTROL_WHITEBALANCE, DefaultPrint);

		PrintValue("backlight compensation", K4A_COLOR_CONTROL_BACKLIGHT_COMPENSATION, [&](std::int32_t value)
		{
			switch (value)
			{
				case 0: ss << "disabled"; break;
				case 1: ss << "enabled"; break;
				default: ss << "unknown (" << value << ")"; break;
			}
		});
		
		PrintValue("powerline frequency", K4A_COLOR_CONTROL_POWERLINE_FREQUENCY, [&](std::int32_t value)
		{
			switch (value)
			{
				case 1: ss << "50Hz"; break;
				case 2: ss << "60Hz"; break;
				default: ss << "unknown (" << value << ")"; break;
			}
		});

		// Remove last '\n'
		std::string output = std::move(ss).str();
		assert(!output.empty());
		output.resize(output.size() - 1);

		infolog("%s", output.c_str());

		return true;
	}, m_device.handle());

	return props;
}

void AzureKinectDevice::HandleBoolParameterUpdate(const std::string& parameterName, bool value)
{
	try
	{
		if (parameterName == "azuresdk_exposure_auto")
		{
			std::int32_t intVal = (value) ? 0 : std::int32_t(GetIntParameterValue("azuresdk_exposure_time"));
			m_device.set_color_control(K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE, (value) ? K4A_COLOR_CONTROL_MODE_AUTO : K4A_COLOR_CONTROL_MODE_MANUAL, intVal);
		}
		else if (parameterName == "azuresdk_whitebalance_auto")
		{
			std::int32_t intVal = (value) ? 0 : std::int32_t(GetIntParameterValue("azuresdk_whitebalance"));
			m_device.set_color_control(K4A_COLOR_CONTROL_WHITEBALANCE, (value) ? K4A_COLOR_CONTROL_MODE_AUTO : K4A_COLOR_CONTROL_MODE_MANUAL, intVal);
		}
		else if (parameterName == "azuresdk_backlightcompensation")
			m_device.set_color_control(K4A_COLOR_CONTROL_BACKLIGHT_COMPENSATION, K4A_COLOR_CONTROL_MODE_MANUAL, (value) ? 1 : 0);
		else
			errorlog("unhandled bool parameter %s", parameterName.c_str());
	}
	catch (const k4a::error& err)
	{
		errorlog("failed to update %s to %s: %s", parameterName.c_str(), (value) ? "enabled" : "disabled", err.what());
	}
}

void AzureKinectDevice::HandleIntParameterUpdate(const std::string& parameterName, long long value)
{
	if (parameterName == "azuresdk_color_resolution")
	{
		m_colorResolution.store(static_cast<ColorResolution>(value));
		TriggerSourceFlagsUpdate();
	}
	else if (parameterName == "azuresdk_depth_mode")
	{
		m_depthMode.store(static_cast<DepthMode>(value));
		TriggerSourceFlagsUpdate();
	}
	else
	{
		try
		{
			if (parameterName == "azuresdk_exposure_time")
			{
				// Don't override automatic exposure
				if (!GetBoolParameterValue("azuresdk_exposure_auto"))
					m_device.set_color_control(K4A_COLOR_CONTROL_EXPOSURE_TIME_ABSOLUTE, K4A_COLOR_CONTROL_MODE_MANUAL, std::int32_t(value));
			}
			else if (parameterName == "azuresdk_whitebalance")
			{
				// Don't override automatic white balance
				if (!GetBoolParameterValue("azuresdk_whitebalance_auto"))
					m_device.set_color_control(K4A_COLOR_CONTROL_WHITEBALANCE, K4A_COLOR_CONTROL_MODE_MANUAL, std::int32_t(value));
			}
			else if (parameterName == "azuresdk_brightness")
				m_device.set_color_control(K4A_COLOR_CONTROL_BRIGHTNESS, K4A_COLOR_CONTROL_MODE_MANUAL, std::int32_t(value));
			else if (parameterName == "azuresdk_contrast")
				m_device.set_color_control(K4A_COLOR_CONTROL_CONTRAST, K4A_COLOR_CONTROL_MODE_MANUAL, std::int32_t(value));
			else if (parameterName == "azuresdk_saturation")
				m_device.set_color_control(K4A_COLOR_CONTROL_SATURATION, K4A_COLOR_CONTROL_MODE_MANUAL, std::int32_t(value));
			else if (parameterName == "azuresdk_sharpness")
				m_device.set_color_control(K4A_COLOR_CONTROL_SHARPNESS, K4A_COLOR_CONTROL_MODE_MANUAL, std::int32_t(value));
			else if (parameterName == "azuresdk_gain")
				m_device.set_color_control(K4A_COLOR_CONTROL_GAIN, K4A_COLOR_CONTROL_MODE_MANUAL, std::int32_t(value));
			else if (parameterName == "azuresdk_powerline_frequency")
			{
				PowerlineFrequency powerlineFrequency = static_cast<PowerlineFrequency>(value);
				m_device.set_color_control(K4A_COLOR_CONTROL_POWERLINE_FREQUENCY, K4A_COLOR_CONTROL_MODE_MANUAL, (powerlineFrequency == PowerlineFrequency::Freq50) ? 1 : 2);
			}
			else
				errorlog("unhandled int parameter %s", parameterName.c_str());
		}
		catch (const k4a::error& err)
		{
			errorlog("failed to update %s to %lld: %s", parameterName.c_str(), value, err.what());
		}
	}
}

void AzureKinectDevice::ThreadFunc(std::condition_variable& cv, std::mutex& m, std::exception_ptr& error)
{
	os_set_thread_name("AzureKinectDevice");

	k4a::calibration calibration;
	std::optional<k4a::transformation> transformation;

#if HAS_BODY_TRACKING
	std::optional<k4abt::tracker> bodyTracker;
#endif

	k4a_device_configuration_t activeConfig = K4A_DEVICE_CONFIG_INIT_DISABLE_ALL;
	SourceFlags enabledSourceFlags = 0;
	bool cameraStarted = false;
	auto UpdateKinectStreams = [&](SourceFlags enabledSources)
	{
		k4a_device_configuration_t newConfig = BuildConfiguration(enabledSources, m_colorResolution.load(), m_depthMode.load());
		if (!CompareConfig(newConfig, activeConfig))
		{
			// Restart cameras only if configuration changed
			if (cameraStarted)
			{
				m_device.stop_cameras();
				cameraStarted = false;
			}

			m_device.start_cameras(&newConfig);
			cameraStarted = true;
			calibration = m_device.get_calibration(newConfig.depth_mode, newConfig.color_resolution);
		}

		if (enabledSources & (Source_Body | Source_ColorToDepthMapping))
		{
			if (!transformation || activeConfig.depth_mode != newConfig.depth_mode || activeConfig.color_resolution != newConfig.color_resolution)
				transformation = k4a::transformation(calibration);
		}
		else
			transformation.reset();

#if HAS_BODY_TRACKING
		if ((enabledSources & Source_Body) && IsBodyTrackingSdkLoaded())
		{
			if (!bodyTracker || activeConfig.depth_mode != newConfig.depth_mode || activeConfig.color_resolution != newConfig.color_resolution)
			{
				bodyTracker.reset(); //< Only one body tracker can exist at a given time in a process
				bodyTracker = k4abt::tracker::create(calibration);
			}
		}
		else
			bodyTracker.reset();
#endif

		activeConfig = newConfig;
		enabledSourceFlags = enabledSources;
	};

	{
		std::unique_lock<std::mutex> lk(m);
		cv.notify_all();
	} // m & cv no longer exists from here

	if (error)
		return;

	while (IsRunning())
	{
		try
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

			if (!cameraStarted)
			{
				os_sleep_ms(10);
				continue;
			}

			// Wait until a capture is available
			k4a::capture capture;
			m_device.get_capture(&capture);

			KinectFramePtr framePtr = std::make_shared<KinectFrame>();
			if (enabledSourceFlags & Source_Color)
			{
				if (k4a::image colorImage = capture.get_color_image())
					framePtr->colorFrame = ToColorFrame(colorImage);
			}

			if (enabledSourceFlags & (Source_Body | Source_Depth | Source_ColorToDepthMapping))
			{
				if (k4a::image depthImage = capture.get_depth_image())
				{
					if (enabledSourceFlags & Source_Depth)
						framePtr->depthFrame = ToDepthFrame(depthImage);

					if (enabledSourceFlags & (Source_Body | Source_ColorToDepthMapping))
					{
						k4a::image mappedDepthImage;

#if HAS_BODY_TRACKING
						if ((enabledSourceFlags & Source_Body) && bodyTracker)
						{
							// Process bodies (TODO: Add possibility to do this asynchronously?)
							bodyTracker->enqueue_capture(capture);
							k4abt::frame bodyTrackingFrame = bodyTracker->pop_result();

							if (bodyTrackingFrame)
							{
								assert(transformation);
								if (k4a::image bodyIndexMap = bodyTrackingFrame.get_body_index_map())
								{
									if (enabledSourceFlags & Source_ColorToDepthMapping)
									{
										auto [mappedDepth, mappedBodyIndexImage] = transformation->depth_image_to_color_camera_custom(depthImage, bodyIndexMap, K4A_TRANSFORMATION_INTERPOLATION_TYPE_NEAREST, K4ABT_BODY_INDEX_MAP_BACKGROUND);
										mappedDepthImage = std::move(mappedDepth);

										framePtr->bodyIndexFrame = ToBodyIndexFrame(mappedBodyIndexImage);
									}
									else
										framePtr->bodyIndexFrame = ToBodyIndexFrame(bodyIndexMap);
								}

							}
						}
#endif

						if (enabledSourceFlags & Source_ColorToDepthMapping)
						{
							if (!mappedDepthImage)
								mappedDepthImage = transformation->depth_image_to_color_camera(depthImage);
						
							framePtr->mappedDepthFrame = ToDepthFrame(mappedDepthImage);
						}
					}
				}
			}

			if (enabledSourceFlags & Source_Infrared)
			{
				if (k4a::image infraredImage = capture.get_ir_image())
					framePtr->infraredFrame = ToInfraredFrame(infraredImage);
			}

			UpdateFrame(std::move(framePtr));
		}
		catch (const std::exception& e)
		{
			errorlog("%s", e.what());

			// Force sleep to prevent log spamming
			os_sleep_ms(100);
		}
	}

	if (cameraStarted)
	{
#if HAS_BODY_TRACKING
		bodyTracker.reset();
#endif
		m_device.stop_cameras();
	}

	infolog("exiting thread");
}

BodyIndexFrameData AzureKinectDevice::ToBodyIndexFrame(const k4a::image& image)
{
	constexpr std::size_t bpp = 1; //< Color is stored as R8

	BodyIndexFrameData bodyIndexFrame;
	bodyIndexFrame.width = image.get_width_pixels();
	bodyIndexFrame.height = image.get_height_pixels();

	std::size_t memSize = bodyIndexFrame.width * bodyIndexFrame.height * bpp;
	bodyIndexFrame.memory.resize(memSize);
	std::uint8_t* memPtr = bodyIndexFrame.memory.data();

	bodyIndexFrame.ptr.reset(memPtr);
	bodyIndexFrame.pitch = bodyIndexFrame.width * bpp;

	const uint8_t* imageBuffer = image.get_buffer();
	int imagePitch = image.get_stride_bytes();
	if (bodyIndexFrame.pitch == imagePitch)
		std::memcpy(memPtr, imageBuffer, bodyIndexFrame.pitch * bodyIndexFrame.height);
	else
	{
		std::uint32_t bestPitch = std::min<std::uint32_t>(bodyIndexFrame.pitch, imagePitch);
		for (std::size_t y = 0; y < bodyIndexFrame.height; ++y)
		{
			const std::uint8_t* input = &imageBuffer[y * imagePitch];
			std::uint8_t* output = memPtr + y * bodyIndexFrame.pitch;

			std::memcpy(output, input, bestPitch);
		}
	}

	return bodyIndexFrame;
}

ColorFrameData AzureKinectDevice::ToColorFrame(const k4a::image& image)
{
	constexpr std::size_t bpp = 4; //< Color is stored as BGRA8

	ColorFrameData colorFrame;
	colorFrame.width = image.get_width_pixels();
	colorFrame.height = image.get_height_pixels();

	std::size_t memSize = colorFrame.width * colorFrame.height * bpp;
	colorFrame.memory.resize(memSize);
	std::uint8_t* memPtr = colorFrame.memory.data();

	colorFrame.ptr.reset(memPtr);
	colorFrame.pitch = colorFrame.width * bpp;
	colorFrame.format = GS_BGRA;

	const uint8_t* imageBuffer = image.get_buffer();
	int imagePitch = image.get_stride_bytes();
	if (colorFrame.pitch == imagePitch)
		std::memcpy(memPtr, imageBuffer, colorFrame.pitch * colorFrame.height);
	else
	{
		std::uint32_t bestPitch = std::min<std::uint32_t>(colorFrame.pitch, imagePitch);
		for (std::size_t y = 0; y < colorFrame.height; ++y)
		{
			const std::uint8_t* input = &imageBuffer[y * imagePitch];
			std::uint8_t* output = memPtr + y * colorFrame.pitch;

			std::memcpy(output, input, bestPitch);
		}
	}

	return colorFrame;
}

DepthFrameData AzureKinectDevice::ToDepthFrame(const k4a::image& image)
{
	constexpr std::size_t bpp = 2; //< Color is stored as R16

	DepthFrameData depthFrame;
	depthFrame.width = image.get_width_pixels();
	depthFrame.height = image.get_height_pixels();

	std::size_t memSize = depthFrame.width * depthFrame.height * bpp;
	depthFrame.memory.resize(memSize);
	std::uint8_t* memPtr = depthFrame.memory.data();

	depthFrame.ptr.reset(reinterpret_cast<std::uint16_t*>(memPtr));
	depthFrame.pitch = depthFrame.width * bpp;

	const uint8_t* imageBuffer = image.get_buffer();
	int imagePitch = image.get_stride_bytes();
	if (depthFrame.pitch == imagePitch)
		std::memcpy(memPtr, imageBuffer, depthFrame.pitch * depthFrame.height);
	else
	{
		std::uint32_t bestPitch = std::min<std::uint32_t>(depthFrame.pitch, imagePitch);
		for (std::size_t y = 0; y < depthFrame.height; ++y)
		{
			const std::uint8_t* input = &imageBuffer[y * imagePitch];
			std::uint8_t* output = memPtr + y * depthFrame.pitch;

			std::memcpy(output, input, bestPitch);
		}
	}

	return depthFrame;
}

InfraredFrameData AzureKinectDevice::ToInfraredFrame(const k4a::image& image)
{
	constexpr std::size_t bpp = 2; //< Color is stored as R16

	InfraredFrameData irFrame;
	irFrame.width = image.get_width_pixels();
	irFrame.height = image.get_height_pixels();

	std::size_t memSize = irFrame.width * irFrame.height * bpp;
	irFrame.memory.resize(memSize);
	std::uint8_t* memPtr = irFrame.memory.data();

	irFrame.ptr.reset(reinterpret_cast<std::uint16_t*>(irFrame.memory.data()));
	irFrame.pitch = irFrame.width * bpp;

	const uint8_t* imageBuffer = image.get_buffer();
	int imagePitch = image.get_stride_bytes();
	if (irFrame.pitch == imagePitch)
		std::memcpy(memPtr, imageBuffer, irFrame.pitch * irFrame.height);
	else
	{
		std::uint32_t bestPitch = std::min<std::uint32_t>(irFrame.pitch, imagePitch);
		for (std::size_t y = 0; y < irFrame.height; ++y)
		{
			const std::uint8_t* input = &imageBuffer[y * imagePitch];
			std::uint8_t* output = memPtr + y * irFrame.pitch;

			std::memcpy(output, input, bestPitch);
		}
	}

	return irFrame;
}
