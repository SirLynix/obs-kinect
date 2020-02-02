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

static void kinect_source_update(void* data, obs_data_t* settings)
{
	KinectSource* kinectSource = static_cast<KinectSource*>(data);
	kinectSource->SetSourceType(static_cast<KinectSource::SourceType>(obs_data_get_int(settings, "source")));
	kinectSource->ShouldStopOnHide(obs_data_get_bool(settings, "invisible_shutdown"));
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

	obs_properties_t *props = obs_properties_create();

	obs_property_t* sourceList = obs_properties_add_list(props, "source", obs_module_text("KinectSource.Source"), OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	obs_property_list_add_int(sourceList, "Color", static_cast<int>(KinectSource::SourceType::Color));
	obs_property_list_add_int(sourceList, "Depth", static_cast<int>(KinectSource::SourceType::Depth));
	obs_property_list_add_int(sourceList, "Infrared", static_cast<int>(KinectSource::SourceType::Infrared));

	obs_properties_add_bool(props, "invisible_shutdown", obs_module_text("KinectSource.InvisibleShutdown"));

	return props;
}

static void kinect_source_defaults(obs_data_t *settings)
{
	obs_data_set_default_int(settings, "source", static_cast<int>(KinectSource::SourceType::Color));
	obs_data_set_default_bool(settings, "invisible_shutdown", false);
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
