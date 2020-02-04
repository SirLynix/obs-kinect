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

#include "KinectSource.hpp"
#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("SirLynix")
OBS_MODULE_USE_DEFAULT_LOCALE("kinect_source", "en-US")

void set_property_visibility(obs_properties_t* props, const char* propertyName, bool visible)
{
	obs_property_t* property = obs_properties_get(props, propertyName);
	if (property)
		obs_property_set_visible(property, visible);
};

static void kinect_source_update(void* data, obs_data_t* settings)
{
	KinectSource* kinectSource = static_cast<KinectSource*>(data);
	kinectSource->SetSourceType(static_cast<KinectSource::SourceType>(obs_data_get_int(settings, "source")));
	kinectSource->ShouldStopOnHide(obs_data_get_bool(settings, "invisible_shutdown"));

	KinectSource::DepthToColorSettings depthToColor;
	depthToColor.averageValue = float(obs_data_get_double(settings, "depth_average"));
	depthToColor.dynamic = obs_data_get_bool(settings, "depth_dynamic");
	depthToColor.standardDeviation = float(obs_data_get_double(settings, "depth_standard_deviation"));

	kinectSource->UpdateDepthToColor(depthToColor);

	KinectSource::InfraredToColorSettings infraredToColor;
	infraredToColor.averageValue = float(obs_data_get_double(settings, "infrared_average"));
	infraredToColor.dynamic = obs_data_get_bool(settings, "infrared_dynamic");
	infraredToColor.standardDeviation = float(obs_data_get_double(settings, "infrared_standard_deviation"));

	kinectSource->UpdateInfraredToColor(infraredToColor);
}

static void* kinect_source_create(obs_data_t* settings, obs_source_t* source)
{
	KinectSource* kinect = new KinectSource(source);
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

	obs_property_t* sourceList = obs_properties_add_list(props, "source", obs_module_text("KinectSource.Source"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(sourceList, "Color", static_cast<int>(KinectSource::SourceType::Color));
	obs_property_list_add_int(sourceList, "Depth", static_cast<int>(KinectSource::SourceType::Depth));
	obs_property_list_add_int(sourceList, "Infrared", static_cast<int>(KinectSource::SourceType::Infrared));

	obs_property_set_modified_callback(sourceList, [](obs_properties_t* props, obs_property_t*, obs_data_t* s)
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
	obs_properties_add_float_slider(props, "depth_standard_deviation", obs_module_text("KinectSource.DepthAverage"), 0.0, 10.0, 0.5);

	obs_properties_add_bool(props, "infrared_dynamic", obs_module_text("KinectSource.InfraredDynamic"));
	obs_properties_add_float_slider(props, "infrared_average", obs_module_text("KinectSource.InfraredAverage"), 0.0, 1.0, 0.005);
	obs_properties_add_float_slider(props, "infrared_standard_deviation", obs_module_text("KinectSource.InfraredAverage"), 0.0, 10.0, 0.5);

	obs_properties_add_bool(props, "invisible_shutdown", obs_module_text("KinectSource.InvisibleShutdown"));

	return props;
}

static void kinect_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "source", static_cast<int>(KinectSource::SourceType::Color));
	obs_data_set_default_bool(settings, "invisible_shutdown", false);
	obs_data_set_default_double(settings, "depth_average", 0.015);
	obs_data_set_default_bool(settings, "depth_dynamic", false);
	obs_data_set_default_double(settings, "depth_standard_deviation", 3);
	obs_data_set_default_double(settings, "infrared_average", 0.08);
	obs_data_set_default_bool(settings, "infrared_dynamic", false);
	obs_data_set_default_double(settings, "infrared_standard_deviation", 3);
}

void RegisterKinectSource()
{
	struct obs_source_info info = {};
	info.id = "kinect_source";
	info.type = OBS_SOURCE_TYPE_INPUT;
	info.output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE;
	info.get_name = [](void*) { return obs_module_text("KinectSource"); };
	info.create = kinect_source_create;
	info.destroy = kinect_source_destroy;
	info.update = kinect_source_update;
	info.get_defaults = kinect_source_defaults;
	info.get_properties = kinect_source_properties;
	info.show = [](void* data) { static_cast<KinectSource*>(data)->OnVisibilityUpdate(true); };
	info.hide = [](void* data) { static_cast<KinectSource*>(data)->OnVisibilityUpdate(false); };
	info.icon_type = OBS_ICON_TYPE_CAMERA;

	obs_register_source(&info);
}

MODULE_EXPORT
bool obs_module_load()
{
	RegisterKinectSource();
	return true;
}

MODULE_EXPORT
void obs_module_unload()
{
}
