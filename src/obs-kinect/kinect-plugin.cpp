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

#include <obs-kinect/KinectDeviceRegistry.hpp>
#include <obs-kinect/KinectSource.hpp>
#include <obs-module.h>
#include <array>
#include <cstring>
#include <optional>

#ifndef _WIN32
#pragma GCC visibility push(default)
#endif

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("SirLynix")
OBS_MODULE_USE_DEFAULT_LOCALE("kinect_source", "en-US")

#ifndef _WIN32
#pragma GCC visibility pop
#endif

static std::shared_ptr<KinectDeviceRegistry> s_deviceRegistry;

static const char* NoDevice = "none_none";

struct Source
{
	const char* text;
	KinectSource::SourceType value;
	SourceFlags requiredSources;
};

static std::array<Source, 3> s_sources = {
	{
		{ "ObsKinect.Source_Color", KinectSource::SourceType::Color, Source_Color },
		{ "ObsKinect.Source_Depth", KinectSource::SourceType::Depth, Source_Depth },
		{ "ObsKinect.Source_Infrared", KinectSource::SourceType::Infrared, Source_Infrared }
	}
};

struct GreenScreenType
{
	const char* text;
	KinectSource::GreenScreenFilterType value;
	SourceFlags requiredDeviceSources;
	SourceFlags supportedSources;
};

static std::array<GreenScreenType, 5> s_greenscreenTypes = {
	{
		{ "ObsKinect.GreenScreenType_Body", KinectSource::GreenScreenFilterType::Body, Source_Body, Source_Color | Source_Depth | Source_Infrared },
		{ "ObsKinect.GreenScreenType_Depth", KinectSource::GreenScreenFilterType::Depth, Source_Depth, Source_Color | Source_Depth | Source_Infrared },
		{ "ObsKinect.GreenScreenType_BodyOrDepth", KinectSource::GreenScreenFilterType::BodyOrDepth, Source_Body | Source_Depth, Source_Color | Source_Depth | Source_Infrared },
		{ "ObsKinect.GreenScreenType_BodyWithinDepth", KinectSource::GreenScreenFilterType::BodyWithinDepth, Source_Body | Source_Depth, Source_Color | Source_Depth | Source_Infrared },
		{ "ObsKinect.GreenScreenType_Dedicated", KinectSource::GreenScreenFilterType::Dedicated, Source_BackgroundRemoval, Source_Color }
	}
};

struct GreenScreenEffect
{
	const char* name;
	const char* text;
	GreenscreenEffectConfigs value; //< Dummy variant, is only used to retrieve the type
};

static std::array<GreenScreenEffect, 3> s_greenscreenEffects = {
	{
		{ "removebackground",  "ObsKinect.GreenScreenEffect_RemoveBackground", RemoveBackgroundEffect::Config{} },
		{ "blurbackground",    "ObsKinect.GreenScreenEffect_BlurBackground",   BlurBackgroundEffect::Config{} },
		{ "replacebackground", "ObsKinect.GreenScreenEffect_ReplaceBackground",   ReplaceBackgroundEffect::Config{} }
	}
};

bool get_property_visibility(obs_properties_t* props, const char* propertyName)
{
	obs_property_t* property = obs_properties_get(props, propertyName);
	if (!property)
		return false;

	return obs_property_visible(property);
}

void set_property_visibility(obs_properties_t* props, const char* propertyName, bool visible)
{
	obs_property_t* property = obs_properties_get(props, propertyName);
	if (property)
		obs_property_set_visible(property, visible);
}

void update_depthinfrared_visibility(obs_properties_t* props, obs_data_t* s)
{
	bool sourceVisible = get_property_visibility(props, "source");
	KinectSource::SourceType sourceType = static_cast<KinectSource::SourceType>(obs_data_get_int(s, "source"));

	bool depthVisible = (sourceVisible && sourceType == KinectSource::SourceType::Depth);
	bool infraredVisible = (sourceVisible && sourceType == KinectSource::SourceType::Infrared);

	set_property_visibility(props, "depth_dynamic", depthVisible);
	set_property_visibility(props, "depth_average", depthVisible);
	set_property_visibility(props, "depth_standard_deviation", depthVisible);

	set_property_visibility(props, "infrared_dynamic", infraredVisible);
	set_property_visibility(props, "infrared_average", infraredVisible);
	set_property_visibility(props, "infrared_standard_deviation", infraredVisible);
}

void update_greenscreen_availability(KinectDevice* device, obs_properties_t* props, obs_data_t* s)
{
	bool sourceVisible = get_property_visibility(props, "source");

	SourceFlags source = 0;
	switch (static_cast<KinectSource::SourceType>(obs_data_get_int(s, "source")))
	{
		case KinectSource::SourceType::Color:
			source = Source_Color;
			break;

		case KinectSource::SourceType::Depth:
			source = Source_Depth;
			break;

		case KinectSource::SourceType::Infrared:
			source = Source_Infrared;
			break;

		default:
			assert(false);
	}

	obs_property_t* type = obs_properties_get(props, "greenscreen_type");
	assert(type);

	SourceFlags supportedSource = device->GetSupportedSources();
	for (std::size_t i = 0; i < s_greenscreenTypes.size(); ++i)
	{
		const GreenScreenType& greenscreen = s_greenscreenTypes[i];
		obs_property_list_item_disable(type, i, !sourceVisible || ((greenscreen.requiredDeviceSources & supportedSource) != greenscreen.requiredDeviceSources) || ((greenscreen.supportedSources & source)) != source);
	}
}

void update_greenscreen_visibility(obs_properties_t* props, obs_data_t* s)
{
	bool enabled = obs_data_get_bool(s, "greenscreen_enabled") && get_property_visibility(props, "greenscreen_enabled");
	KinectSource::GreenScreenFilterType type = static_cast<KinectSource::GreenScreenFilterType>(obs_data_get_int(s, "greenscreen_type"));

	set_property_visibility(props, "greenscreen", enabled);

	bool depthSettingsVisible = (enabled && type != KinectSource::GreenScreenFilterType::Body && type != KinectSource::GreenScreenFilterType::Dedicated);

	set_property_visibility(props, "greenscreen_fadedist", depthSettingsVisible);
	set_property_visibility(props, "greenscreen_maxdist", depthSettingsVisible);
	set_property_visibility(props, "greenscreen_mindist", depthSettingsVisible);

	bool blurSettingsVisible = (enabled && type != KinectSource::GreenScreenFilterType::Dedicated);

	set_property_visibility(props, "greenscreen_maxdirtydepth", blurSettingsVisible);
	set_property_visibility(props, "greenscreen_blurpasses", blurSettingsVisible);
	set_property_visibility(props, "greenscreen_gpudepthmapping", blurSettingsVisible);

	// Green screen effects
	std::size_t activeEffect = std::min(static_cast<std::size_t>(obs_data_get_int(s, "greenscreen_effect")), s_greenscreenEffects.size() - 1);
	for (std::size_t i = 0; i < s_greenscreenEffects.size(); ++i)
		set_property_visibility(props, s_greenscreenEffects[i].name, (activeEffect == i));
};

void update_device_list(obs_property_t* deviceList)
{
	obs_property_list_clear(deviceList);
	obs_property_list_add_string(deviceList, obs_module_text("ObsKinect.NoDevice"), NoDevice);

	s_deviceRegistry->ForEachDevice([deviceList](const std::string& pluginName, const std::string& uniqueName, const KinectDevice& device)
	{
		std::string label = pluginName + " - " + device.GetUniqueName();
		obs_property_list_add_string(deviceList, label.c_str(), uniqueName.c_str());

		return true;
	});
}

static void kinect_source_update(void* data, obs_data_t* settings)
{
	KinectSource* kinectSource = static_cast<KinectSource*>(data);

	const char* deviceName = obs_data_get_string(settings, "device");

	kinectSource->UpdateDevice(deviceName);
	kinectSource->UpdateDeviceParameters(settings);

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
	greenScreen.filterType = static_cast<KinectSource::GreenScreenFilterType>(obs_data_get_int(settings, "greenscreen_type"));

	std::size_t activeEffect = std::min(static_cast<std::size_t>(obs_data_get_int(settings, "greenscreen_effect")), s_greenscreenEffects.size() - 1);
	std::visit([&](auto&& arg)
	{
		using T = std::decay_t<decltype(arg)>;
		using E = typename T::Effect;

		greenScreen.effectConfig = E::ToConfig(settings);

	}, s_greenscreenEffects[activeEffect].value);

	kinectSource->UpdateGreenScreen(std::move(greenScreen));

	KinectSource::InfraredToColorSettings infraredToColor;
	infraredToColor.averageValue = float(obs_data_get_double(settings, "infrared_average"));
	infraredToColor.dynamic = obs_data_get_bool(settings, "infrared_dynamic");
	infraredToColor.standardDeviation = float(obs_data_get_double(settings, "infrared_standard_deviation"));

	kinectSource->UpdateInfraredToColor(infraredToColor);

	kinectSource->UpdateVisibilityMaskFile(obs_data_get_string(settings, "greenscreen_visibilitymaskpath"));
}

static void* kinect_source_create(obs_data_t* settings, obs_source_t* source)
{
	KinectSource* kinect = new KinectSource(s_deviceRegistry, source);
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

	obs_properties_add_bool(props, "invisible_shutdown", obs_module_text("ObsKinect.InvisibleShutdown"));

	// Device selection
	p = obs_properties_add_list(props, "device", obs_module_text("ObsKinect.Device"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	update_device_list(p);

	obs_properties_add_button(props, "device_refresh", obs_module_text("ObsKinect.RefreshDevices"), [](obs_properties_t* props, obs_property_t* /*property*/, void* /*data*/)
	{
		s_deviceRegistry->Refresh();

		obs_property_t* deviceList = obs_properties_get(props, "device");
		update_device_list(deviceList);
		return true;
	});

	s_deviceRegistry->ForEachDevice([&](const std::string& /*pluginName*/, const std::string& uniqueName, const KinectDevice& device)
	{
		obs_properties_t* deviceProperties = device.CreateProperties();
		if (deviceProperties)
			obs_properties_add_group(props, ("device_properties_" + uniqueName).c_str(), device.GetUniqueName().c_str(), OBS_GROUP_NORMAL, deviceProperties);

		return true;
	});

	obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
	{
		s_deviceRegistry->ForEachDevice([=](const std::string& /*pluginName*/, const std::string& uniqueName, const KinectDevice& /*device*/)
		{
			set_property_visibility(props, ("device_properties_" + uniqueName).c_str(), false);
			return true;
		});

		std::string selectedDevice = obs_data_get_string(s, "device");
		if (KinectDevice* device = s_deviceRegistry->GetDevice(selectedDevice))
		{
			set_property_visibility(props, ("device_properties_" + selectedDevice).c_str(), true);
			set_property_visibility(props, "source", true);

			obs_property_t* sourceList = obs_properties_get(props, "source");
			assert(sourceList);

			SourceFlags supportedSource = device->GetSupportedSources();
			for (std::size_t i = 0; i < s_sources.size(); ++i)
			{
				const Source& sourceData = s_sources[i];
				obs_property_list_item_disable(sourceList, i, (sourceData.requiredSources & supportedSource) != sourceData.requiredSources);
			}

			update_greenscreen_availability(device, props, s);
		}
		else
			set_property_visibility(props, "source", false);

		update_depthinfrared_visibility(props, s);
		update_greenscreen_visibility(props, s);

		return true;
	});

	// Source selection
	p = obs_properties_add_list(props, "source", obs_module_text("ObsKinect.Source"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	for (const Source& sourceData : s_sources)
		obs_property_list_add_int(p, obs_module_text(sourceData.text), static_cast<int>(sourceData.value));

	obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
	{
		update_depthinfrared_visibility(props, s);
		update_greenscreen_visibility(props, s);

		std::string selectedDevice = obs_data_get_string(s, "device");
		if (KinectDevice* device = s_deviceRegistry->GetDevice(selectedDevice))
			update_greenscreen_availability(device, props, s);

		return true;
	});

	// Depth/infrared to color settings
	obs_properties_add_bool(props, "depth_dynamic", obs_module_text("ObsKinect.DepthDynamic"));
	obs_properties_add_float_slider(props, "depth_average", obs_module_text("ObsKinect.DepthAverage"), 0.0, 1.0, 0.005);
	obs_properties_add_float_slider(props, "depth_standard_deviation", obs_module_text("ObsKinect.DepthStandardDeviation"), 0.0, 10.0, 0.5);

	obs_properties_add_bool(props, "infrared_dynamic", obs_module_text("ObsKinect.InfraredDynamic"));
	obs_properties_add_float_slider(props, "infrared_average", obs_module_text("ObsKinect.InfraredAverage"), 0.0, 1.0, 0.005);
	obs_properties_add_float_slider(props, "infrared_standard_deviation", obs_module_text("ObsKinect.InfraredStandardDeviation"), 0.0, 10.0, 0.5);

	// Green screen stuff
	p = obs_properties_add_bool(props, "greenscreen_enabled", obs_module_text("ObsKinect.GreenScreenEnabled"));
	obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
	{
		update_greenscreen_visibility(props, s);
		return true;
	});

	obs_properties_t* greenscreenProps = obs_properties_create();

	// Greenscreen filter type (body, depth, ...)
	p = obs_properties_add_list(greenscreenProps, "greenscreen_type", obs_module_text("ObsKinect.GreenScreenType"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	for (const GreenScreenType& greenscreen : s_greenscreenTypes)
		obs_property_list_add_int(p, obs_module_text(greenscreen.text), static_cast<int>(greenscreen.value));

	obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
	{
		update_greenscreen_visibility(props, s);
		return true;
	});

	std::string filter = obs_module_text("BrowsePath.Images");
	filter += " (*.bmp *.jpg *.jpeg *.tga *.gif *.png);;";
	filter += obs_module_text("BrowsePath.AllFiles");
	filter += " (*.*)";

	obs_properties_add_path(greenscreenProps, "greenscreen_visibilitymaskpath", obs_module_text("ObsKinect.GreenScreenVisibilityMask"), OBS_PATH_FILE, filter.data(), nullptr);

	// Greenscreen effect (remove background, blur background, ...)
	p = obs_properties_add_list(greenscreenProps, "greenscreen_effect", obs_module_text("ObsKinect.GreenScreenEffect"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	for (std::size_t i = 0; i < s_greenscreenEffects.size(); ++i)
	{
		const GreenScreenEffect& effectType = s_greenscreenEffects[i];
		obs_property_list_add_int(p, obs_module_text(effectType.text), static_cast<long long>(i));

		obs_properties_t* effectProperties;
		std::visit([&](auto&& arg)
		{
			using T = std::decay_t<decltype(arg)>;
			using E = typename T::Effect;

			effectProperties = E::BuildProperties();

		}, effectType.value);

		if (effectProperties)
			obs_properties_add_group(greenscreenProps, effectType.name, obs_module_text(effectType.text), OBS_GROUP_NORMAL, effectProperties);
	}
	
	obs_property_set_modified_callback(p, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
	{
		update_greenscreen_visibility(props, s);
		return true;
	});

	p = obs_properties_add_int_slider(greenscreenProps, "greenscreen_maxdist", obs_module_text("ObsKinect.GreenScreenMaxDist"), 0, 10000, 10);
	obs_property_int_set_suffix(p, obs_module_text("ObsKinect.GreenScreenDistUnit"));

	p = obs_properties_add_int_slider(greenscreenProps, "greenscreen_mindist", obs_module_text("ObsKinect.GreenScreenMinDist"), 0, 10000, 10);
	obs_property_int_set_suffix(p, obs_module_text("ObsKinect.GreenScreenDistUnit"));

	p = obs_properties_add_int_slider(greenscreenProps, "greenscreen_fadedist", obs_module_text("ObsKinect.GreenScreenFadeDist"), 0, 2000, 1);
	obs_property_int_set_suffix(p, obs_module_text("ObsKinect.GreenScreenDistUnit"));

	obs_properties_add_int_slider(greenscreenProps, "greenscreen_blurpasses", obs_module_text("ObsKinect.GreenScreenBlurPassCount"), 0, 20, 1);

	p = obs_properties_add_int_slider(greenscreenProps, "greenscreen_maxdirtydepth", obs_module_text("ObsKinect.GreenScreenMaxDirtyDepth"), 0, 30, 1);
	obs_property_set_long_description(p, obs_module_text("ObsKinect.GreenScreenMaxDirtyDepthDesc"));

	p = obs_properties_add_bool(greenscreenProps, "greenscreen_gpudepthmapping", obs_module_text("ObsKinect.GreenScreenGpuDepthMapping"));
	obs_property_set_long_description(p, obs_module_text("ObsKinect.GreenScreenGpuDepthMappingDesc"));

	obs_properties_add_group(props, "greenscreen", obs_module_text("ObsKinect.GreenScreen"), OBS_GROUP_NORMAL, greenscreenProps);

	return props;
}

static void kinect_source_defaults(obs_data_t* settings)
{
	obs_data_set_default_string(settings, "device", NoDevice);

	// Set the first device of the list as the default one
	s_deviceRegistry->ForEachDevice([=](const std::string& /*pluginName*/, const std::string& uniqueName, const KinectDevice& /*device*/)
	{
		obs_data_set_default_string(settings, "device", uniqueName.c_str());
		return false; //< Stop at first device
	});

	obs_data_set_default_int(settings, "source", static_cast<int>(KinectSource::SourceType::Color));
	obs_data_set_default_bool(settings, "invisible_shutdown", true);
	obs_data_set_default_double(settings, "depth_average", 0.015);
	obs_data_set_default_bool(settings, "depth_dynamic", false);
	obs_data_set_default_double(settings, "depth_standard_deviation", 3);
	obs_data_set_default_double(settings, "infrared_average", 0.08);
	obs_data_set_default_bool(settings, "infrared_dynamic", false);
	obs_data_set_default_double(settings, "infrared_standard_deviation", 3);
	obs_data_set_default_bool(settings, "greenscreen_enabled", false);
	obs_data_set_default_bool(settings, "greenscreen_gpudepthmapping", true);
	obs_data_set_default_int(settings, "greenscreen_blurpasses", 3);
	obs_data_set_default_int(settings, "greenscreen_effect", 0);
	obs_data_set_default_int(settings, "greenscreen_fadedist", 100);
	obs_data_set_default_int(settings, "greenscreen_maxdist", 1200);
	obs_data_set_default_int(settings, "greenscreen_mindist", 1);
	obs_data_set_default_int(settings, "greenscreen_maxdirtydepth", 0);
	obs_data_set_default_int(settings, "greenscreen_type", static_cast<int>(KinectSource::GreenScreenFilterType::Depth));
	
	// Register default values
	s_deviceRegistry->ForEachDevice([=](const std::string& /*pluginName*/, const std::string& /*uniqueName*/, const KinectDevice& device)
	{
		device.SetDefaultValues(settings);
		return true;
	});

	for (const GreenScreenEffect& effectType : s_greenscreenEffects)
	{
		std::visit([&](auto&& arg)
		{
			using T = std::decay_t<decltype(arg)>;
			using E = typename T::Effect;

			E::SetDefaultValues(settings);
		}, effectType.value);
	}
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
	info.get_name = [](void*) { return obs_module_text("ObsKinect.KinectSource"); };
	info.create = kinect_source_create;
	info.destroy = kinect_source_destroy;
	info.update = kinect_source_update;
	info.get_defaults = kinect_source_defaults;
	info.get_properties = kinect_source_properties;
	info.get_width = [](void* data) { return static_cast<KinectSource*>(data)->GetWidth(); };
	info.get_height = [](void* data) { return static_cast<KinectSource*>(data)->GetHeight(); };
	info.video_render = kinect_video_render;
	info.video_tick = kinect_video_tick;
	info.show = [](void* data) { static_cast<KinectSource*>(data)->OnVisibilityUpdate(true); };
	info.hide = [](void* data) { static_cast<KinectSource*>(data)->OnVisibilityUpdate(false); };
	info.icon_type = OBS_ICON_TYPE_CAMERA;

	obs_register_source(&info);
}

OBSKINECT_EXPORT bool obs_module_load()
{
	if (obs_get_version() < MAKE_SEMANTIC_VERSION(25, 0, 0))
	{
		errorlog("this plugins requires a least OBS 25 to work, please upgrade or create a GitHub issue if upgrading is not an option");
		return false;
	}

	SetTranslateFunction([](const char* key)
	{
		return obs_module_text(key);
	});

	s_deviceRegistry = std::make_shared<KinectDeviceRegistry>();
	s_deviceRegistry->RegisterPlugin("obs-kinect-azuresdk");
	s_deviceRegistry->RegisterPlugin("obs-kinect-freenect");
	s_deviceRegistry->RegisterPlugin("obs-kinect-freenect2");
	s_deviceRegistry->RegisterPlugin("obs-kinect-sdk10");
	s_deviceRegistry->RegisterPlugin("obs-kinect-sdk20");

	s_deviceRegistry->Refresh();

	RegisterKinectSource();
	return true;
}

OBSKINECT_EXPORT void obs_module_unload()
{
	infolog("unloading obs-kinect");
	s_deviceRegistry.reset();

	SetTranslateFunction(nullptr);
}
