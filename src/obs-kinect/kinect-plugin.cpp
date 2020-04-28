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

#include "KinectDeviceRegistry.hpp"
#include "KinectSource.hpp"
#include <obs-module.h>
#include <cstring>
#include <optional>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("SirLynix")
OBS_MODULE_USE_DEFAULT_LOCALE("kinect_source", "en-US")

static std::optional<KinectDeviceRegistry> s_deviceRegistry;

static const char* NoDevice = "none_none";

void set_property_visibility(obs_properties_t* props, const char* propertyName, bool visible)
{
	obs_property_t* property = obs_properties_get(props, propertyName);
	if (property)
		obs_property_set_visible(property, visible);
};

static void kinect_source_update(void* data, obs_data_t* settings)
{
	KinectSource* kinectSource = static_cast<KinectSource*>(data);

	const char* deviceName = obs_data_get_string(settings, "device");

	kinectSource->UpdateDevice(deviceName);

	kinectSource->SetServicePriority(static_cast<ProcessPriority>(obs_data_get_int(settings, "service_priority")));
	kinectSource->SetSourceType(static_cast<KinectSource::SourceType>(obs_data_get_int(settings, "source")));
	kinectSource->ShouldStopOnHide(obs_data_get_bool(settings, "invisible_shutdown"));

	KinectSource::DepthToColorSettings depthToColor;
	depthToColor.averageValue = float(obs_data_get_double(settings, "depth_average"));
	depthToColor.dynamic = obs_data_get_bool(settings, "depth_dynamic");
	depthToColor.standardDeviation = float(obs_data_get_double(settings, "depth_standard_deviation"));

	kinectSource->UpdateDepthToColor(depthToColor);

	KinectSource::GreenScreenSettings greenScreen;
	greenScreen.blurPassCount = static_cast<std::size_t>(obs_data_get_int(settings, "greenscreen_blurpasses"));
	greenScreen.enabled = obs_data_get_bool(settings, "greenscreen_enabled");
	greenScreen.depthMax = static_cast<std::uint16_t>(obs_data_get_int(settings, "greenscreen_maxdist"));
	greenScreen.depthMin = static_cast<std::uint16_t>(obs_data_get_int(settings, "greenscreen_mindist"));
	greenScreen.fadeDist = static_cast<std::uint16_t>(obs_data_get_int(settings, "greenscreen_fadedist"));
	greenScreen.maxDirtyDepth = static_cast<std::uint8_t>(obs_data_get_int(settings, "greenscreen_maxdirtydepth"));
	greenScreen.gpuDepthMapping = obs_data_get_bool(settings, "greenscreen_gpudepthmapping");
	greenScreen.type = static_cast<KinectSource::GreenScreenType>(obs_data_get_int(settings, "greenscreen_type"));

	kinectSource->UpdateGreenScreen(greenScreen);

	KinectSource::InfraredToColorSettings infraredToColor;
	infraredToColor.averageValue = float(obs_data_get_double(settings, "infrared_average"));
	infraredToColor.dynamic = obs_data_get_bool(settings, "infrared_dynamic");
	infraredToColor.standardDeviation = float(obs_data_get_double(settings, "infrared_standard_deviation"));

	kinectSource->UpdateInfraredToColor(infraredToColor);
}

static void* kinect_source_create(obs_data_t* settings, obs_source_t* source)
{
	KinectSource* kinect = new KinectSource(*s_deviceRegistry);
	kinect_source_update(kinect, settings);

	kinect->OnVisibilityUpdate(obs_source_showing(source));

	return kinect;
}

static void kinect_source_destroy(void *data)
{
	KinectSource* kinectSource = static_cast<KinectSource*>(data);
	delete kinectSource;
}

static obs_properties_t* kinect_source_properties(void *unused)
{
	UNUSED_PARAMETER(unused);

	obs_properties_t* props = obs_properties_create();
	obs_property_t* p;

	p = obs_properties_add_list(props, "device", obs_module_text("KinectSource.Device"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	obs_property_list_add_string(p, obs_module_text("KinectSource.NoDevice"), NoDevice);
	s_deviceRegistry->ForEachDevice([p](const std::string& pluginName, const std::string& uniqueName, const KinectDevice& device)
	{
		std::string label = pluginName + " - " + device.GetUniqueName();
		obs_property_list_add_string(p, label.c_str(), uniqueName.c_str());

		return true;
	});

	obs_properties_add_button(props, "device_refresh", "Refresh Kinect Devices", [](obs_properties_t* props, obs_property_t* property, void* data)
	{
		s_deviceRegistry->Refresh();
		return true;
	});

	p = obs_properties_add_list(props, "service_priority", obs_module_text("KinectSource.Priority"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("KinectSource.Priority_High"), static_cast<int>(ProcessPriority::High));
	obs_property_list_add_int(p, obs_module_text("KinectSource.Priority_AboveNormal"), static_cast<int>(ProcessPriority::AboveNormal));
	obs_property_list_add_int(p, obs_module_text("KinectSource.Priority_Normal"), static_cast<int>(ProcessPriority::Normal));

	obs_properties_add_bool(props, "invisible_shutdown", obs_module_text("KinectSource.InvisibleShutdown"));

	p = obs_properties_add_list(props, "source", obs_module_text("KinectSource.Source"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("KinectSource.Source_Color"), static_cast<int>(KinectSource::SourceType::Color));
	obs_property_list_add_int(p, obs_module_text("KinectSource.Source_Depth"), static_cast<int>(KinectSource::SourceType::Depth));
	obs_property_list_add_int(p, obs_module_text("KinectSource.Source_Infrared"), static_cast<int>(KinectSource::SourceType::Infrared));

	obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
	{
		KinectSource::SourceType sourceType = static_cast<KinectSource::SourceType>(obs_data_get_int(s, "source"));

		bool depthVisible = (sourceType == KinectSource::SourceType::Depth);
		bool infraredVisible = (sourceType == KinectSource::SourceType::Infrared);

		set_property_visibility(props, "depth_dynamic", depthVisible);
		set_property_visibility(props, "depth_average", depthVisible);
		set_property_visibility(props, "depth_standard_deviation", depthVisible);

		set_property_visibility(props, "infrared_dynamic", infraredVisible);
		set_property_visibility(props, "infrared_average", infraredVisible);
		set_property_visibility(props, "infrared_standard_deviation", infraredVisible);

		return true;
	});

	obs_properties_add_bool(props, "depth_dynamic", obs_module_text("KinectSource.DepthDynamic"));
	obs_properties_add_float_slider(props, "depth_average", obs_module_text("KinectSource.DepthAverage"), 0.0, 1.0, 0.005);
	obs_properties_add_float_slider(props, "depth_standard_deviation", obs_module_text("KinectSource.DepthStandardDeviation"), 0.0, 10.0, 0.5);

	obs_properties_add_bool(props, "infrared_dynamic", obs_module_text("KinectSource.InfraredDynamic"));
	obs_properties_add_float_slider(props, "infrared_average", obs_module_text("KinectSource.InfraredAverage"), 0.0, 1.0, 0.005);
	obs_properties_add_float_slider(props, "infrared_standard_deviation", obs_module_text("KinectSource.InfraredStandardDeviation"), 0.0, 10.0, 0.5);

	auto greenscreenVisibilityCallback = [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
	{
		bool enabled = obs_data_get_bool(s, "greenscreen_enabled");
		KinectSource::GreenScreenType type = static_cast<KinectSource::GreenScreenType>(obs_data_get_int(s, "greenscreen_type"));

		set_property_visibility(props, "greenscreen_blurpasses", enabled);
		set_property_visibility(props, "greenscreen_type", enabled);
		set_property_visibility(props, "greenscreen_gpudepthmapping", enabled);

		bool depthSettingsVisible = (enabled && type == KinectSource::GreenScreenType::Depth);

		set_property_visibility(props, "greenscreen_fadedist", depthSettingsVisible);
		set_property_visibility(props, "greenscreen_maxdist", depthSettingsVisible);
		set_property_visibility(props, "greenscreen_mindist", depthSettingsVisible);
		set_property_visibility(props, "greenscreen_maxdirtydepth", depthSettingsVisible);

		return true;
	};

	p = obs_properties_add_bool(props, "greenscreen_enabled", obs_module_text("KinectSource.GreenScreenEnabled"));
	obs_property_set_modified_callback(p, greenscreenVisibilityCallback);

	p = obs_properties_add_list(props, "greenscreen_type", obs_module_text("KinectSource.GreenScreenType"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(p, obs_module_text("KinectSource.GreenScreenType_Body"), static_cast<int>(KinectSource::GreenScreenType::Body));
	obs_property_list_add_int(p, obs_module_text("KinectSource.GreenScreenType_Depth"), static_cast<int>(KinectSource::GreenScreenType::Depth));
	
	obs_property_set_modified_callback(p, greenscreenVisibilityCallback);

	obs_properties_add_int_slider(props, "greenscreen_maxdist", obs_module_text("KinectSource.GreenScreenMaxDist"), 0, 10000, 10);
	obs_properties_add_int_slider(props, "greenscreen_mindist", obs_module_text("KinectSource.GreenScreenMinDist"), 0, 10000, 10);
	obs_properties_add_int_slider(props, "greenscreen_fadedist", obs_module_text("KinectSource.GreenScreenFadeDist"), 0, 200, 1);
	obs_properties_add_int_slider(props, "greenscreen_blurpasses", obs_module_text("KinectSource.GreenScreenBlurPassCount"), 0, 20, 1);
	obs_properties_add_int_slider(props, "greenscreen_maxdirtydepth", obs_module_text("KinectSource.GreenScreenMaxDirtyDepth"), 0, 30, 1);
	obs_properties_add_bool(props, "greenscreen_gpudepthmapping", obs_module_text("KinectSource.GreenScreenGpuDepthMapping"));

	return props;
}

static void kinect_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "device", NoDevice);
	s_deviceRegistry->ForEachDevice([=](const std::string& pluginName, const std::string& uniqueName, const KinectDevice& device)
	{
		obs_data_set_default_string(settings, "device", uniqueName.c_str());
		return false; //< Stop at first device
	});

	obs_data_set_default_int(settings, "service_priority", static_cast<int>(ProcessPriority::Normal));
	obs_data_set_default_int(settings, "source", static_cast<int>(KinectSource::SourceType::Color));
	obs_data_set_default_bool(settings, "invisible_shutdown", false);
	obs_data_set_default_double(settings, "depth_average", 0.015);
	obs_data_set_default_bool(settings, "depth_dynamic", false);
	obs_data_set_default_double(settings, "depth_standard_deviation", 3);
	obs_data_set_default_double(settings, "infrared_average", 0.08);
	obs_data_set_default_bool(settings, "infrared_dynamic", false);
	obs_data_set_default_double(settings, "infrared_standard_deviation", 3);
	obs_data_set_default_bool(settings, "greenscreen_enabled", false);
	obs_data_set_default_bool(settings, "greenscreen_gpudepthmapping", true);
	obs_data_set_default_int(settings, "greenscreen_blurpasses", 3);
	obs_data_set_default_int(settings, "greenscreen_fadedist", 100);
	obs_data_set_default_int(settings, "greenscreen_maxdist", 1200);
	obs_data_set_default_int(settings, "greenscreen_mindist", 1);
	obs_data_set_default_int(settings, "greenscreen_maxdirtydepth", 0);
	obs_data_set_default_int(settings, "greenscreen_type", static_cast<int>(KinectSource::SourceType::Depth));
}

static void kinect_video_render(void* data, gs_effect_t* /*effect*/)
{
	KinectSource* kinectSource = static_cast<KinectSource*>(data);
	kinectSource->Render();
}

static void kinect_video_tick(void* data, float seconds)
{
	KinectSource* kinectSource = static_cast<KinectSource*>(data);
	kinectSource->Update(seconds);
}

void RegisterKinectSource()
{
	struct obs_source_info info = {};
	info.id = "kinect_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
	info.get_name = [](void*) { return obs_module_text("KinectSource"); };
	info.create = kinect_source_create;
	info.destroy = kinect_source_destroy;
	info.update = kinect_source_update;
	info.get_defaults = kinect_source_defaults;
	info.get_properties = kinect_source_properties;
	info.get_width = [](void*) -> uint32_t { return 1920; };
	info.get_height = [](void*) -> uint32_t { return 1080; };
	info.video_render = kinect_video_render;
	info.video_tick = kinect_video_tick;
	info.show = [](void* data) { static_cast<KinectSource*>(data)->OnVisibilityUpdate(true); };
	info.hide = [](void* data) { static_cast<KinectSource*>(data)->OnVisibilityUpdate(false); };
	info.icon_type = OBS_ICON_TYPE_CAMERA;

	obs_register_source(&info);
}

MODULE_EXPORT
bool obs_module_load()
{
	s_deviceRegistry.emplace();
	s_deviceRegistry->RegisterPlugin("obs-kinect-sdk20");

	s_deviceRegistry->Refresh();

	RegisterKinectSource();
	return true;
}

MODULE_EXPORT
void obs_module_unload()
{
	s_deviceRegistry.reset();
}
