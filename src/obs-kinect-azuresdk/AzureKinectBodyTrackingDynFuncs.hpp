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

#pragma once

#ifndef OBS_KINECT_PLUGIN_AZUREKINECTBODYTRACKINGFUNCTIONS
#define OBS_KINECT_PLUGIN_AZUREKINECTBODYTRACKINGFUNCTIONS

// We're loading Body Tracking SDK dynamically, can't include k4abt.h but must define its functions globally to use k4abt.hpp
// TODO: Find a better way to do this, if possible (maybe suggest something to Microsoft to help this?)
#ifdef K4ABT_H
#error This header must be included before k4abt.h
#endif

#define K4ABT_H

#include <k4abtversion.h>
#include <k4abttypes.h>

bool LoadBodyTrackingSdk(void* obsModule);
bool IsBodyTrackingSdkLoaded();
void UnloadBodyTrackingSdk();

// Updated for Azure Kinect Body Tracking SDK 1.01
#define OBS_KINECT_AZURE_SDK_BODY_TRACKING_FOREACH_FUNC(cb) \
	cb(k4a_result_t, k4abt_tracker_create, const k4a_calibration_t* sensor_calibration, k4abt_tracker_configuration_t config, k4abt_tracker_t* tracker_handle) \
	cb(void, k4abt_tracker_destroy, k4abt_tracker_t tracker_handle) \
	cb(void, k4abt_tracker_set_temporal_smoothing, k4abt_tracker_t tracker_handle, float smoothing_factor) \
	cb(k4a_wait_result_t, k4abt_tracker_enqueue_capture, k4abt_tracker_t tracker_handle, k4a_capture_t sensor_capture_handle, int32_t timeout_in_ms) \
	cb(k4a_wait_result_t, k4abt_tracker_pop_result, k4abt_tracker_t tracker_handle, k4abt_frame_t* body_frame_handle, int32_t timeout_in_ms) \
	cb(void, k4abt_tracker_shutdown, k4abt_tracker_t tracker_handle) \
	cb(void, k4abt_frame_release, k4abt_frame_t body_frame_handle) \
	cb(void, k4abt_frame_reference, k4abt_frame_t body_frame_handle) \
	cb(uint32_t, k4abt_frame_get_num_bodies, k4abt_frame_t body_frame_handle) \
	cb(k4a_result_t, k4abt_frame_get_body_skeleton, k4abt_frame_t body_frame_handle, uint32_t index, k4abt_skeleton_t* skeleton) \
	cb(uint32_t, k4abt_frame_get_body_id, k4abt_frame_t body_frame_handle, uint32_t index) \
	cb(uint64_t, k4abt_frame_get_device_timestamp_usec, k4abt_frame_t body_frame_handle) \
	cb(k4a_image_t, k4abt_frame_get_body_index_map, k4abt_frame_t body_frame_handle) \
	cb(k4a_capture_t, k4abt_frame_get_capture, k4abt_frame_t body_frame_handle) \
	cb(uint64_t, k4abt_frame_get_system_timestamp_nsec, k4abt_frame_t body_frame_handle) \

#define OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC(Ret, Name, ...) extern Ret (*Name)(__VA_ARGS__);
OBS_KINECT_AZURE_SDK_BODY_TRACKING_FOREACH_FUNC(OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC)
#undef OBS_KINECT_AZURE_SDK_BODY_TRACKING_FUNC

#endif
